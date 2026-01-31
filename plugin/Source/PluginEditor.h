/*
  ==============================================================================
    DelayWave - Plugin Editor (JUCE 8 WebView UI)
    A wavey modulated delay effect
  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

//==============================================================================
class DelayWaveEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit DelayWaveEditor(DelayWaveProcessor&);
    ~DelayWaveEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    //==============================================================================
    DelayWaveProcessor& processorRef;

    //==============================================================================
    // WebView component
    std::unique_ptr<juce::WebBrowserComponent> webView;
    juce::File resourcesDir;

    //==============================================================================
    // JUCE 8 Parameter Relays
    std::unique_ptr<juce::WebSliderRelay> timeRelay;
    std::unique_ptr<juce::WebSliderRelay> feedbackRelay;
    std::unique_ptr<juce::WebSliderRelay> mixRelay;
    std::unique_ptr<juce::WebSliderRelay> modRateRelay;
    std::unique_ptr<juce::WebSliderRelay> modDepthRelay;
    std::unique_ptr<juce::WebSliderRelay> toneRelay;
    std::unique_ptr<juce::WebToggleButtonRelay> bypassRelay;

    //==============================================================================
    // Parameter Attachments
    std::unique_ptr<juce::WebSliderParameterAttachment> timeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> feedbackAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> modRateAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> modDepthAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> toneAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> bypassAttachment;

    //==============================================================================
    void setupWebView();
    void setupRelaysAndAttachments();
    void setupActivationEvents();
    void sendVisualizerData();
    void sendActivationState();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayWaveEditor)
};
