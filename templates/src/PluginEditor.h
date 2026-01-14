/*
  ==============================================================================
    {{PLUGIN_NAME}} - Plugin Editor (WebView-based UI)
    BeatConnect Plugin Template
  ==============================================================================
*/

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

//==============================================================================
class {{PLUGIN_NAME}}Editor : public juce::AudioProcessorEditor,
                              private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit {{PLUGIN_NAME}}Editor({{PLUGIN_NAME}}Processor&);
    ~{{PLUGIN_NAME}}Editor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    //==============================================================================
    // Processor reference
    {{PLUGIN_NAME}}Processor& processorRef;

    //==============================================================================
    // WebView for UI
    std::unique_ptr<juce::WebBrowserComponent> webView;

    //==============================================================================
    // Web <-> Native Communication
    void handleWebMessage(const juce::var& message);
    void sendToWeb(const juce::String& type, const juce::var& data);
    void sendParameterState();
    void sendSingleParameter(const juce::String& paramId, float value);

    //==============================================================================
    // Parameter Listener
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    //==============================================================================
    // Parameter IDs to listen to
    juce::StringArray parameterIds;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR({{PLUGIN_NAME}}Editor)
};
