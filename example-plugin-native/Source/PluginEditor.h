#pragma once

#include "PluginProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

/**
 * Native JUCE UI Example
 *
 * This editor demonstrates a traditional JUCE plugin UI using standard components:
 * - Rotary sliders with custom LookAndFeel (arc-style like Drive/HAZE)
 * - Labels with tracking/letter-spacing
 * - Toggle buttons with custom styling
 * - Level meters with glow effects
 *
 * No WebView, no React - just JUCE Components.
 *
 * Design inspired by BeatConnect plugins (Drive, HAZE, DRIFT):
 * - Dark background (#0f0f12)
 * - Warm orange accent (#ff6b35)
 * - Arc-based rotary controls
 * - Subtle glow effects on active elements
 */
class ExamplePluginNativeEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit ExamplePluginNativeEditor(ExamplePluginNativeProcessor&);
    ~ExamplePluginNativeEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    ExamplePluginNativeProcessor& processorRef;

    // Custom look and feel for modern plugin appearance
    class ModernLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        ModernLookAndFeel();

        void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                              float sliderPosProportional, float rotaryStartAngle,
                              float rotaryEndAngle, juce::Slider&) override;

        void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;
    };

    ModernLookAndFeel modernLookAndFeel;

    // UI Components
    juce::Slider gainSlider;
    juce::Slider mixSlider;
    juce::ToggleButton bypassButton;

    juce::Label gainLabel;
    juce::Label mixLabel;
    juce::Label titleLabel;

    // Level meters (smoothed display values)
    float displayInputLevel = 0.0f;
    float displayOutputLevel = 0.0f;

    // Parameter attachments (connect UI to APVTS)
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExamplePluginNativeEditor)
};
