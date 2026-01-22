/*
  ==============================================================================
    {{PLUGIN_NAME}} - Audio Processor
    BeatConnect Plugin Template
  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
class {{PLUGIN_NAME}}Processor : public juce::AudioProcessor
{
public:
    //==============================================================================
    {{PLUGIN_NAME}}Processor();
    ~{{PLUGIN_NAME}}Processor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameter Access
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    //==============================================================================
    // BeatConnect Integration
    bool hasActivationEnabled() const;
    juce::String getPluginId() const { return pluginId_; }
    juce::String getApiBaseUrl() const { return apiBaseUrl_; }
    juce::String getSupabaseKey() const { return supabasePublishableKey_; }

private:
    //==============================================================================
    // Parameters
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //==============================================================================
    // BeatConnect project data (loaded from embedded project_data.json)
    void loadProjectData();
    juce::String pluginId_;
    juce::String apiBaseUrl_;
    juce::String supabasePublishableKey_;
    juce::var buildFlags_;

    //==============================================================================
    // DSP - Add your processing members here
    // Example:
    // juce::dsp::Gain<float> gain;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR({{PLUGIN_NAME}}Processor)
};
