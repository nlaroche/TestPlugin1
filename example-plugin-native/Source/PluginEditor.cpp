#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.h"

//==============================================================================
// ModernLookAndFeel
//==============================================================================

ExamplePluginNativeEditor::ModernLookAndFeel::ModernLookAndFeel()
{
    // Dark color scheme
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xFF1a1a1a));
    setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFF6366f1));  // Indigo
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xFF3f3f46));
    setColour(juce::Slider::thumbColourId, juce::Colours::white);
    setColour(juce::Label::textColourId, juce::Colours::white);
    setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF6366f1));
}

void ExamplePluginNativeEditor::ModernLookAndFeel::drawRotarySlider(
    juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    auto outline = slider.findColour(juce::Slider::rotarySliderOutlineColourId);
    auto fill = slider.findColour(juce::Slider::rotarySliderFillColourId);

    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(10);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    auto lineW = juce::jmin(6.0f, radius * 0.2f);
    auto arcRadius = radius - lineW * 0.5f;

    // Background arc
    juce::Path backgroundArc;
    backgroundArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(),
                                 arcRadius, arcRadius, 0.0f,
                                 rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(outline);
    g.strokePath(backgroundArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc
    if (slider.isEnabled())
    {
        juce::Path valueArc;
        valueArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(),
                                arcRadius, arcRadius, 0.0f,
                                rotaryStartAngle, toAngle, true);
        g.setColour(fill);
        g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Thumb dot
    auto thumbWidth = lineW * 1.5f;
    juce::Point<float> thumbPoint(
        bounds.getCentreX() + (arcRadius - lineW) * std::cos(toAngle - juce::MathConstants<float>::halfPi),
        bounds.getCentreY() + (arcRadius - lineW) * std::sin(toAngle - juce::MathConstants<float>::halfPi));
    g.setColour(juce::Colours::white);
    g.fillEllipse(juce::Rectangle<float>(thumbWidth, thumbWidth).withCentre(thumbPoint));

    // Center value text
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    auto text = juce::String(slider.getValue(), 2);
    g.drawText(text, bounds.toNearestInt(), juce::Justification::centred, false);
}

//==============================================================================
// Editor
//==============================================================================

ExamplePluginNativeEditor::ExamplePluginNativeEditor(ExamplePluginNativeProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setLookAndFeel(&modernLookAndFeel);

    // Title
    titleLabel.setText("Example Plugin", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(24.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    // Gain knob
    gainSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    gainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(gainSlider);

    gainLabel.setText("Gain", juce::dontSendNotification);
    gainLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(gainLabel);

    // Mix knob
    mixSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    mixSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(mixSlider);

    mixLabel.setText("Mix", juce::dontSendNotification);
    mixLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(mixLabel);

    // Bypass button
    bypassButton.setButtonText("Bypass");
    addAndMakeVisible(bypassButton);

    // Connect to APVTS
    auto& apvts = processorRef.getAPVTS();
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParamIDs::gain, gainSlider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParamIDs::mix, mixSlider);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, ParamIDs::bypass, bypassButton);

    setSize(400, 300);
    startTimerHz(30);
}

ExamplePluginNativeEditor::~ExamplePluginNativeEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void ExamplePluginNativeEditor::timerCallback()
{
    // Smooth the level meters
    float targetIn = processorRef.getInputLevel();
    float targetOut = processorRef.getOutputLevel();

    displayInputLevel = displayInputLevel * 0.8f + targetIn * 0.2f;
    displayOutputLevel = displayOutputLevel * 0.8f + targetOut * 0.2f;

    repaint();
}

void ExamplePluginNativeEditor::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colour(0xFF1a1a1a));

    // Subtle gradient header
    juce::ColourGradient headerGradient(
        juce::Colour(0xFF2d2d2d), 0.0f, 0.0f,
        juce::Colour(0xFF1a1a1a), 0.0f, 60.0f, false);
    g.setGradientFill(headerGradient);
    g.fillRect(0, 0, getWidth(), 60);

    // Level meters
    auto meterArea = getLocalBounds().removeFromRight(40).reduced(10, 80);
    auto meterWidth = (meterArea.getWidth() - 5) / 2;

    // Input meter background
    g.setColour(juce::Colour(0xFF3f3f46));
    auto inputMeterBounds = meterArea.removeFromLeft(meterWidth).toFloat();
    g.fillRoundedRectangle(inputMeterBounds, 3.0f);

    // Input meter fill
    auto inputFillHeight = inputMeterBounds.getHeight() * juce::jlimit(0.0f, 1.0f, displayInputLevel);
    g.setColour(juce::Colour(0xFF22c55e));  // Green
    g.fillRoundedRectangle(
        inputMeterBounds.getX(),
        inputMeterBounds.getBottom() - inputFillHeight,
        inputMeterBounds.getWidth(),
        inputFillHeight,
        3.0f);

    meterArea.removeFromLeft(5);  // Spacing

    // Output meter background
    g.setColour(juce::Colour(0xFF3f3f46));
    auto outputMeterBounds = meterArea.toFloat();
    g.fillRoundedRectangle(outputMeterBounds, 3.0f);

    // Output meter fill
    auto outputFillHeight = outputMeterBounds.getHeight() * juce::jlimit(0.0f, 1.0f, displayOutputLevel);
    g.setColour(juce::Colour(0xFF6366f1));  // Indigo
    g.fillRoundedRectangle(
        outputMeterBounds.getX(),
        outputMeterBounds.getBottom() - outputFillHeight,
        outputMeterBounds.getWidth(),
        outputFillHeight,
        3.0f);

    // Meter labels
    g.setColour(juce::Colour(0xFF71717a));
    g.setFont(10.0f);
    g.drawText("IN", inputMeterBounds.toNearestInt().translated(0, -15), juce::Justification::centred);
    g.drawText("OUT", outputMeterBounds.toNearestInt().translated(0, -15), juce::Justification::centred);
}

void ExamplePluginNativeEditor::resized()
{
    auto area = getLocalBounds();

    // Title at top
    titleLabel.setBounds(area.removeFromTop(60));

    // Remove meter area from right
    area.removeFromRight(50);

    // Knobs area
    auto knobArea = area.reduced(20);
    auto knobWidth = knobArea.getWidth() / 2;

    // Gain knob
    auto gainArea = knobArea.removeFromLeft(knobWidth);
    gainSlider.setBounds(gainArea.removeFromTop(120).reduced(10));
    gainLabel.setBounds(gainArea.removeFromTop(25));

    // Mix knob
    auto mixArea = knobArea;
    mixSlider.setBounds(mixArea.removeFromTop(120).reduced(10));
    mixLabel.setBounds(mixArea.removeFromTop(25));

    // Bypass at bottom center
    bypassButton.setBounds(
        getWidth() / 2 - 50,
        getHeight() - 50,
        100, 30);
}
