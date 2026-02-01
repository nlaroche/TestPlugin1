/*
  ==============================================================================
    DelayWave - Audio Processor Implementation
    A wavey modulated delay effect
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.h"

#if HAS_PROJECT_DATA
#include "ProjectData.h"
#endif

#if BEATCONNECT_ACTIVATION_ENABLED
#include <beatconnect/Activation.h>
#endif

// Increment this when making breaking changes to parameter structure
static constexpr int kStateVersion = 1;

//==============================================================================
DelayWaveProcessor::DelayWaveProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()),
      delayLineL(static_cast<int>(maxDelaySeconds * 192000)),  // Max sample rate headroom
      delayLineR(static_cast<int>(maxDelaySeconds * 192000))
{
    loadProjectData();
}

DelayWaveProcessor::~DelayWaveProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout DelayWaveProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Time: 10ms to 1000ms (1 second max delay)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::time, 1 },
        "Time",
        juce::NormalisableRange<float>(10.0f, 1000.0f, 1.0f, 0.5f),  // Skew for better low-end control
        300.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("ms")
    ));

    // Feedback: 0% to 95% (avoid infinite feedback)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::feedback, 1 },
        "Feedback",
        juce::NormalisableRange<float>(0.0f, 0.95f, 0.01f),
        0.4f,
        juce::AudioParameterFloatAttributes()
            .withLabel("%")
    ));

    // Mix: 0% to 100%
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::mix, 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f,
        juce::AudioParameterFloatAttributes()
            .withLabel("%")
    ));

    // Mod Rate: 0.1 Hz to 10 Hz
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::modRate, 1 },
        "Mod Rate",
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.5f),
        0.5f,
        juce::AudioParameterFloatAttributes()
            .withLabel("Hz")
    ));

    // Mod Depth: 0% to 100%
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::modDepth, 1 },
        "Mod Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.3f,
        juce::AudioParameterFloatAttributes()
            .withLabel("%")
    ));

    // Tone: 0% (dark) to 100% (bright)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::tone, 1 },
        "Tone",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.7f,
        juce::AudioParameterFloatAttributes()
            .withLabel("%")
    ));

    // Bypass
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamIDs::bypass, 1 },
        "Bypass",
        false
    ));

    return { params.begin(), params.end() };
}

//==============================================================================
const juce::String DelayWaveProcessor::getName() const
{
    return JucePlugin_Name;
}

bool DelayWaveProcessor::acceptsMidi() const { return false; }
bool DelayWaveProcessor::producesMidi() const { return false; }
bool DelayWaveProcessor::isMidiEffect() const { return false; }

double DelayWaveProcessor::getTailLengthSeconds() const
{
    // Tail length depends on delay time and feedback
    return maxDelaySeconds * 2.0;
}

int DelayWaveProcessor::getNumPrograms() { return 1; }
int DelayWaveProcessor::getCurrentProgram() { return 0; }
void DelayWaveProcessor::setCurrentProgram(int index) { juce::ignoreUnused(index); }
const juce::String DelayWaveProcessor::getProgramName(int index) { juce::ignoreUnused(index); return {}; }
void DelayWaveProcessor::changeProgramName(int index, const juce::String& newName) { juce::ignoreUnused(index, newName); }

//==============================================================================
void DelayWaveProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // Prepare delay lines with 2x headroom for variable buffer sizes
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock * 2);
    spec.numChannels = 2;

    delayLineL.prepare(spec);
    delayLineR.prepare(spec);

    // Set max delay
    int maxDelaySamples = static_cast<int>(maxDelaySeconds * sampleRate);
    delayLineL.setMaximumDelayInSamples(maxDelaySamples);
    delayLineR.setMaximumDelayInSamples(maxDelaySamples);

    // Initialize smoothed values (20ms smoothing time)
    smoothedTime.reset(sampleRate, 0.02);
    smoothedFeedback.reset(sampleRate, 0.02);
    smoothedMix.reset(sampleRate, 0.02);
    smoothedModRate.reset(sampleRate, 0.02);
    smoothedModDepth.reset(sampleRate, 0.02);
    smoothedTone.reset(sampleRate, 0.02);

    // Set initial values
    smoothedTime.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamIDs::time)->load());
    smoothedFeedback.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamIDs::feedback)->load());
    smoothedMix.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamIDs::mix)->load());
    smoothedModRate.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamIDs::modRate)->load());
    smoothedModDepth.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamIDs::modDepth)->load());
    smoothedTone.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamIDs::tone)->load());

    // Reset filter state
    filterStateL = 0.0f;
    filterStateR = 0.0f;

    // Reset LFO phase
    lfoPhase = 0.0f;
}

void DelayWaveProcessor::releaseResources()
{
    delayLineL.reset();
    delayLineR.reset();
}

bool DelayWaveProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void DelayWaveProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples = buffer.getNumSamples();

    // Clear unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    // Measure input levels before processing
    float inL = buffer.getMagnitude(0, 0, numSamples);
    float inR = totalNumInputChannels > 1 ? buffer.getMagnitude(1, 0, numSamples) : inL;
    inputLevelL.store(inL);
    inputLevelR.store(inR);

    // Get parameter values
    bool bypassValue = apvts.getRawParameterValue(ParamIDs::bypass)->load() > 0.5f;

    if (bypassValue)
    {
        // Reset smoothed values to prevent clicks when re-enabling
        smoothedTime.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamIDs::time)->load());
        smoothedFeedback.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamIDs::feedback)->load());
        smoothedMix.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamIDs::mix)->load());
        smoothedModRate.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamIDs::modRate)->load());
        smoothedModDepth.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamIDs::modDepth)->load());
        smoothedTone.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamIDs::tone)->load());

        // Measure output levels even when bypassed
        outputLevelL.store(inL);
        outputLevelR.store(inR);
        return;
    }

    // Update target values
    smoothedTime.setTargetValue(apvts.getRawParameterValue(ParamIDs::time)->load());
    smoothedFeedback.setTargetValue(apvts.getRawParameterValue(ParamIDs::feedback)->load());
    smoothedMix.setTargetValue(apvts.getRawParameterValue(ParamIDs::mix)->load());
    smoothedModRate.setTargetValue(apvts.getRawParameterValue(ParamIDs::modRate)->load());
    smoothedModDepth.setTargetValue(apvts.getRawParameterValue(ParamIDs::modDepth)->load());
    smoothedTone.setTargetValue(apvts.getRawParameterValue(ParamIDs::tone)->load());

    // Get channel pointers
    auto* leftChannel = buffer.getWritePointer(0);
    auto* rightChannel = totalNumInputChannels > 1 ? buffer.getWritePointer(1) : leftChannel;

    // LFO phase increment base (will be modulated per sample)
    const float twoPi = juce::MathConstants<float>::twoPi;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Get smoothed parameter values
        float timeMs = smoothedTime.getNextValue();
        float feedback = smoothedFeedback.getNextValue();
        float mix = smoothedMix.getNextValue();
        float modRate = smoothedModRate.getNextValue();
        float modDepth = smoothedModDepth.getNextValue();
        float tone = smoothedTone.getNextValue();

        // Convert time to samples
        float baseDelaySamples = (timeMs / 1000.0f) * static_cast<float>(currentSampleRate);

        // Calculate LFO modulation (sine wave)
        float lfoValue = std::sin(lfoPhase);

        // Modulation amount (up to 20ms of wobble)
        float modAmount = modDepth * 0.02f * static_cast<float>(currentSampleRate);
        float modulatedDelaySamplesL = baseDelaySamples + lfoValue * modAmount;
        float modulatedDelaySamplesR = baseDelaySamples - lfoValue * modAmount;  // Inverted for stereo width

        // Clamp to valid range
        modulatedDelaySamplesL = juce::jlimit(1.0f, static_cast<float>(delayLineL.getMaximumDelayInSamples() - 1), modulatedDelaySamplesL);
        modulatedDelaySamplesR = juce::jlimit(1.0f, static_cast<float>(delayLineR.getMaximumDelayInSamples() - 1), modulatedDelaySamplesR);

        // Read from delay lines
        float delayedL = delayLineL.popSample(0, modulatedDelaySamplesL);
        float delayedR = delayLineR.popSample(0, modulatedDelaySamplesR);

        // Apply tone filter (simple one-pole lowpass)
        // tone = 0 -> very dark (low cutoff), tone = 1 -> bright (high cutoff)
        float filterCoeff = 0.1f + tone * 0.85f;  // Range from 0.1 to 0.95
        filterStateL = filterStateL + filterCoeff * (delayedL - filterStateL);
        filterStateR = filterStateR + filterCoeff * (delayedR - filterStateR);

        float filteredL = filterStateL;
        float filteredR = filterStateR;

        // Get dry input
        float dryL = leftChannel[sample];
        float dryR = rightChannel[sample];

        // Write to delay lines (input + filtered feedback)
        delayLineL.pushSample(0, dryL + filteredL * feedback);
        delayLineR.pushSample(0, dryR + filteredR * feedback);

        // Mix dry and wet
        leftChannel[sample] = dryL * (1.0f - mix) + filteredL * mix;
        rightChannel[sample] = dryR * (1.0f - mix) + filteredR * mix;

        // Advance LFO phase
        lfoPhase += twoPi * modRate / static_cast<float>(currentSampleRate);
        if (lfoPhase >= twoPi)
            lfoPhase -= twoPi;
    }

    // Measure output levels after processing
    float outL = buffer.getMagnitude(0, 0, numSamples);
    float outR = totalNumInputChannels > 1 ? buffer.getMagnitude(1, 0, numSamples) : outL;
    outputLevelL.store(outL);
    outputLevelR.store(outR);
}

//==============================================================================
bool DelayWaveProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* DelayWaveProcessor::createEditor()
{
    return new DelayWaveEditor(*this);
}

//==============================================================================
void DelayWaveProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    xml->setAttribute("stateVersion", kStateVersion);
    copyXmlToBinary(*xml, destData);
}

void DelayWaveProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        int loadedVersion = xmlState->getIntAttribute("stateVersion", 0);
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

        if (loadedVersion != kStateVersion)
        {
            DBG("State version mismatch (loaded: " + juce::String(loadedVersion) +
                ", current: " + juce::String(kStateVersion) + ") - using defaults");
        }
    }
}

//==============================================================================
// BeatConnect Integration
//==============================================================================

bool DelayWaveProcessor::hasActivationEnabled() const
{
#if HAS_PROJECT_DATA && BEATCONNECT_ACTIVATION_ENABLED
    return static_cast<bool>(buildFlags_.getProperty("enableActivationKeys", false));
#else
    return false;
#endif
}

#if BEATCONNECT_ACTIVATION_ENABLED
beatconnect::Activation* DelayWaveProcessor::getActivation()
{
    return activation_.get();
}
#endif

void DelayWaveProcessor::loadProjectData()
{
#if HAS_PROJECT_DATA
    int dataSize = 0;
    const char* data = ProjectData::getNamedResource("project_data_json", dataSize);

    if (data == nullptr || dataSize == 0)
    {
        DBG("No project_data.json found in BinaryData");
        return;
    }

    auto jsonString = juce::String::fromUTF8(data, dataSize);
    auto parsed = juce::JSON::parse(jsonString);

    if (parsed.isVoid())
    {
        DBG("Failed to parse project_data.json");
        return;
    }

    pluginId_ = parsed.getProperty("pluginId", "").toString();
    apiBaseUrl_ = parsed.getProperty("apiBaseUrl", "").toString();
    supabasePublishableKey_ = parsed.getProperty("supabasePublishableKey", "").toString();
    buildFlags_ = parsed.getProperty("flags", juce::var());

    DBG("Loaded BeatConnect project data - Plugin ID: " + pluginId_);
#endif

#if BEATCONNECT_ACTIVATION_ENABLED
    activation_ = beatconnect::Activation::createFromBuildData(
        JucePlugin_Name,
        false
    );

    if (activation_)
    {
        DBG("BeatConnect Activation SDK initialized for: " + pluginId_);
    }
#endif
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DelayWaveProcessor();
}
