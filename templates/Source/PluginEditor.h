/*
  ==============================================================================
    {{PLUGIN_NAME}} - Plugin Editor (JUCE 8 WebView UI)
    BeatConnect Plugin Template

    Uses JUCE 8's native WebView relay system for bidirectional parameter sync.
  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

//==============================================================================
class {{PLUGIN_NAME}}Editor : public juce::AudioProcessorEditor,
                              private juce::Timer
{
public:
    explicit {{PLUGIN_NAME}}Editor({{PLUGIN_NAME}}Processor&);
    ~{{PLUGIN_NAME}}Editor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    //==============================================================================
    {{PLUGIN_NAME}}Processor& processorRef;

    //==============================================================================
    // WebView component
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // Resources directory for production builds
    juce::File resourcesDir;

    //==============================================================================
    // JUCE 8 Parameter Relays
    // These provide automatic bidirectional sync between web UI and APVTS.
    // Create one relay per parameter, matching the identifier used in web code.

    // Example relays (add your own parameters here):
    std::unique_ptr<juce::WebSliderRelay> gainRelay;
    std::unique_ptr<juce::WebSliderRelay> mixRelay;
    std::unique_ptr<juce::WebToggleButtonRelay> bypassRelay;
    // std::unique_ptr<juce::WebComboBoxRelay> modeRelay;

    //==============================================================================
    // Parameter Attachments
    // Connect relays to APVTS parameters for automatic value sync

    std::unique_ptr<juce::WebSliderParameterAttachment> gainAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> mixAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> bypassAttachment;
    // std::unique_ptr<juce::WebComboBoxParameterAttachment> modeAttachment;

    //==============================================================================
    void setupWebView();
    void setupRelaysAndAttachments();
    void setupActivationEvents();
    void sendVisualizerData();
    void sendActivationState();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR({{PLUGIN_NAME}}Editor)
};
