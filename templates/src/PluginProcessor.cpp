/*
  ==============================================================================
    {{PLUGIN_NAME}} - Audio Processor Implementation
    BeatConnect Plugin Template
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
{{PLUGIN_NAME}}Processor::{{PLUGIN_NAME}}Processor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

{{PLUGIN_NAME}}Processor::~{{PLUGIN_NAME}}Processor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout {{PLUGIN_NAME}}Processor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // ==============================================================================
    // Define your parameters here
    // ==============================================================================

    // Example: Gain parameter
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("gain", 1),
        "Gain",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f,
        juce::AudioParameterFloatAttributes()
            .withLabel("dB")
    ));

    // Example: Mix parameter
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("mix", 1),
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        1.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("%")
    ));

    // Example: Boolean toggle
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("bypass", 1),
        "Bypass",
        false
    ));

    return { params.begin(), params.end() };
}

//==============================================================================
const juce::String {{PLUGIN_NAME}}Processor::getName() const
{
    return JucePlugin_Name;
}

bool {{PLUGIN_NAME}}Processor::acceptsMidi() const
{
    return false;
}

bool {{PLUGIN_NAME}}Processor::producesMidi() const
{
    return false;
}

bool {{PLUGIN_NAME}}Processor::isMidiEffect() const
{
    return false;
}

double {{PLUGIN_NAME}}Processor::getTailLengthSeconds() const
{
    return 0.0;
}

int {{PLUGIN_NAME}}Processor::getNumPrograms()
{
    return 1;
}

int {{PLUGIN_NAME}}Processor::getCurrentProgram()
{
    return 0;
}

void {{PLUGIN_NAME}}Processor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String {{PLUGIN_NAME}}Processor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void {{PLUGIN_NAME}}Processor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void {{PLUGIN_NAME}}Processor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // ==============================================================================
    // Initialize your DSP here
    // ==============================================================================

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    // Example:
    // gain.prepare(spec);
    // gain.setGainLinear(0.5f);
}

void {{PLUGIN_NAME}}Processor::releaseResources()
{
    // Release DSP resources here
}

bool {{PLUGIN_NAME}}Processor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void {{PLUGIN_NAME}}Processor::processBlock(juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // ==============================================================================
    // Get parameter values (atomic read - thread safe)
    // ==============================================================================
    auto gainValue = apvts.getRawParameterValue("gain")->load();
    auto mixValue = apvts.getRawParameterValue("mix")->load();
    auto bypassValue = apvts.getRawParameterValue("bypass")->load() > 0.5f;

    if (bypassValue)
        return;

    // ==============================================================================
    // Process audio here
    // ==============================================================================

    // Example: Simple gain processing
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            // Dry/wet mix with gain
            float dry = channelData[sample];
            float wet = dry * gainValue;
            channelData[sample] = dry * (1.0f - mixValue) + wet * mixValue;
        }
    }
}

//==============================================================================
bool {{PLUGIN_NAME}}Processor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* {{PLUGIN_NAME}}Processor::createEditor()
{
    return new {{PLUGIN_NAME}}Editor(*this);
}

//==============================================================================
void {{PLUGIN_NAME}}Processor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void {{PLUGIN_NAME}}Processor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new {{PLUGIN_NAME}}Processor();
}
