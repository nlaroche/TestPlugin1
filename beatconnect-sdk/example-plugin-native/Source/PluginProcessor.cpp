#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.h"

ExamplePluginNativeProcessor::ExamplePluginNativeProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

ExamplePluginNativeProcessor::~ExamplePluginNativeProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout ExamplePluginNativeProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Gain: 0-200% (0.0 to 2.0)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::gain, 1 },
        "Gain",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f),
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // Mix: 0-100%
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::mix, 1 },
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // Bypass
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamIDs::bypass, 1 },
        "Bypass",
        false));

    return { params.begin(), params.end() };
}

void ExamplePluginNativeProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    // Initialize smoothed values with 20ms ramp time
    smoothedGain.reset(sampleRate, 0.02);
    smoothedMix.reset(sampleRate, 0.02);

    // Set initial values
    smoothedGain.setCurrentAndTargetValue(*apvts.getRawParameterValue(ParamIDs::gain));
    smoothedMix.setCurrentAndTargetValue(*apvts.getRawParameterValue(ParamIDs::mix));
}

void ExamplePluginNativeProcessor::releaseResources()
{
}

void ExamplePluginNativeProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Calculate input level
    float inLevel = 0.0f;
    for (int ch = 0; ch < totalNumInputChannels; ++ch)
        inLevel = std::max(inLevel, buffer.getMagnitude(ch, 0, buffer.getNumSamples()));
    inputLevel.store(inLevel);

    // Check bypass
    bool bypassed = *apvts.getRawParameterValue(ParamIDs::bypass) > 0.5f;

    if (bypassed)
    {
        smoothedGain.setCurrentAndTargetValue(*apvts.getRawParameterValue(ParamIDs::gain));
        smoothedMix.setCurrentAndTargetValue(*apvts.getRawParameterValue(ParamIDs::mix));
        outputLevel.store(inLevel);
        return;
    }

    // Update target values
    smoothedGain.setTargetValue(*apvts.getRawParameterValue(ParamIDs::gain));
    smoothedMix.setTargetValue(*apvts.getRawParameterValue(ParamIDs::mix));

    // Process audio
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float gain = smoothedGain.getNextValue();
        float mix = smoothedMix.getNextValue();

        for (int ch = 0; ch < totalNumInputChannels; ++ch)
        {
            float dry = buffer.getSample(ch, sample);
            float wet = dry * gain;
            buffer.setSample(ch, sample, dry * (1.0f - mix) + wet * mix);
        }
    }

    // Calculate output level
    float outLevel = 0.0f;
    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        outLevel = std::max(outLevel, buffer.getMagnitude(ch, 0, buffer.getNumSamples()));
    outputLevel.store(outLevel);
}

juce::AudioProcessorEditor* ExamplePluginNativeProcessor::createEditor()
{
    return new ExamplePluginNativeEditor(*this);
}

void ExamplePluginNativeProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    xml->setAttribute("stateVersion", kStateVersion);
    copyXmlToBinary(*xml, destData);
}

void ExamplePluginNativeProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState && xmlState->hasTagName(apvts.state.getType()))
    {
        int loadedVersion = xmlState->getIntAttribute("stateVersion", 0);
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

        if (loadedVersion != kStateVersion)
        {
            DBG("State version mismatch: loaded " << loadedVersion << ", expected " << kStateVersion);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ExamplePluginNativeProcessor();
}
