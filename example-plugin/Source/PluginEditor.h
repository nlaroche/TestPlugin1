#pragma once

#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

class ExamplePluginEditor : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    explicit ExamplePluginEditor(ExamplePluginProcessor&);
    ~ExamplePluginEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void setupWebView();
    void setupAttachments();
    void timerCallback() override;
    void sendVisualizerData();
    void sendActivationState();

    ExamplePluginProcessor& processorRef;

    // Parameter relays - created BEFORE WebBrowserComponent
    std::unique_ptr<juce::WebSliderRelay> gainRelay;
    std::unique_ptr<juce::WebSliderRelay> mixRelay;
    std::unique_ptr<juce::WebToggleButtonRelay> bypassRelay;

    // Parameter attachments - created AFTER WebBrowserComponent
    std::unique_ptr<juce::WebSliderParameterAttachment> gainAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> bypassAttachment;

    std::unique_ptr<juce::WebBrowserComponent> webView;
    juce::File resourcesDir;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExamplePluginEditor)
};
