/*
  ==============================================================================
    DelayWave - Audio Processor
    A wavey modulated delay effect
  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <memory>

#if BEATCONNECT_ACTIVATION_ENABLED
namespace beatconnect { class Activation; }
#endif

//==============================================================================
class DelayWaveProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    DelayWaveProcessor();
    ~DelayWaveProcessor() override;

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

#if BEATCONNECT_ACTIVATION_ENABLED
    beatconnect::Activation* getActivation();
#endif

private:
    //==============================================================================
    // Parameters
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //==============================================================================
    // BeatConnect project data
    void loadProjectData();
    juce::String pluginId_;
    juce::String apiBaseUrl_;
    juce::String supabasePublishableKey_;
    juce::var buildFlags_;

#if BEATCONNECT_ACTIVATION_ENABLED
    std::unique_ptr<beatconnect::Activation> activation_;
#endif

    //==============================================================================
    // DSP - Delay line with modulation
    static constexpr float maxDelaySeconds = 2.0f;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLineL;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLineR;

    // LFO for modulation
    float lfoPhase = 0.0f;
    double currentSampleRate = 44100.0;

    // Smoothed parameter values (prevent clicks)
    juce::SmoothedValue<float> smoothedTime;
    juce::SmoothedValue<float> smoothedFeedback;
    juce::SmoothedValue<float> smoothedMix;
    juce::SmoothedValue<float> smoothedModRate;
    juce::SmoothedValue<float> smoothedModDepth;
    juce::SmoothedValue<float> smoothedTone;

    // Simple lowpass filter for tone control
    float filterStateL = 0.0f;
    float filterStateR = 0.0f;

    //==============================================================================
    // Level metering
    std::atomic<float> inputLevelL { 0.0f };
    std::atomic<float> inputLevelR { 0.0f };
    std::atomic<float> outputLevelL { 0.0f };
    std::atomic<float> outputLevelR { 0.0f };

public:
    // Get peak levels (0.0 - 1.0 range)
    float getInputLevel() const { return std::max(inputLevelL.load(), inputLevelR.load()); }
    float getOutputLevel() const { return std::max(outputLevelL.load(), outputLevelR.load()); }

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayWaveProcessor)
};
