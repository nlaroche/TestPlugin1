#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.h"

//==============================================================================
// Modern Look and Feel - Drive-inspired styling
//==============================================================================

ExamplePluginNativeEditor::ModernLookAndFeel::ModernLookAndFeel()
{
    // Dark warm color scheme matching Drive/HAZE style
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xFF0f0f12));

    // Warm orange accent (like Drive)
    setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFFff6b35));
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xFF252530));
    setColour(juce::Slider::thumbColourId, juce::Colours::white);

    setColour(juce::Label::textColourId, juce::Colour(0xFF888899));
    setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFFff6b35));
}

void ExamplePluginNativeEditor::ModernLookAndFeel::drawRotarySlider(
    juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    auto fill = slider.findColour(juce::Slider::rotarySliderFillColourId);
    auto outline = slider.findColour(juce::Slider::rotarySliderOutlineColourId);

    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(8);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    auto lineW = 6.0f;
    auto arcRadius = radius - lineW * 0.5f - 4.0f;

    // Background track arc
    juce::Path backgroundArc;
    backgroundArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(),
                                 arcRadius, arcRadius, 0.0f,
                                 rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(outline);
    g.strokePath(backgroundArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc with glow effect
    if (slider.isEnabled() && sliderPos > 0.0f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(),
                                arcRadius, arcRadius, 0.0f,
                                rotaryStartAngle, toAngle, true);

        // Glow layer
        g.setColour(fill.withAlpha(0.3f));
        g.strokePath(valueArc, juce::PathStrokeType(lineW + 6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Main arc
        g.setColour(fill);
        g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Center circle (dark with subtle border)
    auto innerRadius = radius - 24.0f;
    g.setColour(juce::Colour(0xFF1a1a20));
    g.fillEllipse(bounds.getCentreX() - innerRadius,
                  bounds.getCentreY() - innerRadius,
                  innerRadius * 2.0f, innerRadius * 2.0f);

    g.setColour(juce::Colour(0xFF2a2a35));
    g.drawEllipse(bounds.getCentreX() - innerRadius,
                  bounds.getCentreY() - innerRadius,
                  innerRadius * 2.0f, innerRadius * 2.0f, 1.0f);

    // Indicator line
    auto indicatorLength = innerRadius * 0.6f;
    auto indicatorRadius = innerRadius - 4.0f;
    juce::Point<float> indicatorStart(
        bounds.getCentreX() + (indicatorRadius - indicatorLength) * std::cos(toAngle - juce::MathConstants<float>::halfPi),
        bounds.getCentreY() + (indicatorRadius - indicatorLength) * std::sin(toAngle - juce::MathConstants<float>::halfPi));
    juce::Point<float> indicatorEnd(
        bounds.getCentreX() + indicatorRadius * std::cos(toAngle - juce::MathConstants<float>::halfPi),
        bounds.getCentreY() + indicatorRadius * std::sin(toAngle - juce::MathConstants<float>::halfPi));

    g.setColour(fill);
    g.drawLine(indicatorStart.x, indicatorStart.y, indicatorEnd.x, indicatorEnd.y, 3.0f);

    // Center value text
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(18.0f).withExtraKerningFactor(0.05f));
    auto text = juce::String(juce::roundToInt(sliderPos * 100)) + "%";
    g.drawText(text, bounds.toNearestInt(), juce::Justification::centred, false);
}

void ExamplePluginNativeEditor::ModernLookAndFeel::drawToggleButton(
    juce::Graphics& g, juce::ToggleButton& button,
    bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsDown);

    auto bounds = button.getLocalBounds().toFloat();
    auto isOn = button.getToggleState();

    // Background
    auto bgColour = isOn ? juce::Colour(0xFFff6b35) : juce::Colour(0xFF1a1a20);
    auto borderColour = isOn ? juce::Colour(0xFFff6b35) : juce::Colour(0xFF3a3a45);

    if (shouldDrawButtonAsHighlighted && !isOn)
    {
        borderColour = juce::Colour(0xFFff6b35).withAlpha(0.5f);
    }

    g.setColour(bgColour);
    g.fillRoundedRectangle(bounds.reduced(1), 6.0f);

    g.setColour(borderColour);
    g.drawRoundedRectangle(bounds.reduced(1), 6.0f, 1.0f);

    // Text
    g.setColour(isOn ? juce::Colour(0xFF0f0f12) : juce::Colour(0xFF888899));
    g.setFont(juce::Font(11.0f).withExtraKerningFactor(0.1f));
    g.drawText(button.getButtonText().toUpperCase(), bounds, juce::Justification::centred);
}

//==============================================================================
// Editor
//==============================================================================

ExamplePluginNativeEditor::ExamplePluginNativeEditor(ExamplePluginNativeProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setLookAndFeel(&modernLookAndFeel);

    // Title - uses gradient effect via custom paint
    titleLabel.setText("EXAMPLE PLUGIN", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(28.0f).withExtraKerningFactor(0.2f));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFff6b35));
    addAndMakeVisible(titleLabel);

    // Gain knob
    gainSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    gainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    gainSlider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible(gainSlider);

    gainLabel.setText("GAIN", juce::dontSendNotification);
    gainLabel.setFont(juce::Font(10.0f).withExtraKerningFactor(0.15f));
    gainLabel.setJustificationType(juce::Justification::centred);
    gainLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF555566));
    addAndMakeVisible(gainLabel);

    // Mix knob
    mixSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    mixSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    mixSlider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                                   juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible(mixSlider);

    mixLabel.setText("MIX", juce::dontSendNotification);
    mixLabel.setFont(juce::Font(10.0f).withExtraKerningFactor(0.15f));
    mixLabel.setJustificationType(juce::Justification::centred);
    mixLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF555566));
    addAndMakeVisible(mixLabel);

    // Bypass button
    bypassButton.setButtonText("Active");
    addAndMakeVisible(bypassButton);

    // Connect to APVTS
    auto& apvts = processorRef.getAPVTS();
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParamIDs::gain, gainSlider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParamIDs::mix, mixSlider);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, ParamIDs::bypass, bypassButton);

    // Update bypass button text when state changes
    bypassButton.onClick = [this]() {
        bypassButton.setButtonText(bypassButton.getToggleState() ? "Bypassed" : "Active");
    };

    setSize(500, 400);
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

    displayInputLevel = displayInputLevel * 0.85f + targetIn * 0.15f;
    displayOutputLevel = displayOutputLevel * 0.85f + targetOut * 0.15f;

    repaint();
}

void ExamplePluginNativeEditor::paint(juce::Graphics& g)
{
    // Dark background
    g.fillAll(juce::Colour(0xFF0f0f12));

    // Subtle center glow
    juce::ColourGradient centerGlow(
        juce::Colour(0xFFff6b35).withAlpha(0.08f),
        getWidth() * 0.5f, getHeight() * 0.5f,
        juce::Colours::transparentBlack,
        getWidth() * 0.5f, 0.0f, true);
    g.setGradientFill(centerGlow);
    g.fillRect(getLocalBounds());

    // Header border
    g.setColour(juce::Colour(0xFF1a1a20));
    g.drawLine(0, 70, (float)getWidth(), 70, 1.0f);

    // Footer border
    g.setColour(juce::Colour(0xFF1a1a20));
    g.drawLine(0, getHeight() - 50.0f, (float)getWidth(), getHeight() - 50.0f, 1.0f);

    // Level meters
    auto meterArea = getLocalBounds().removeFromRight(60).reduced(15, 90);
    auto meterWidth = (meterArea.getWidth() - 8) / 2;
    auto meterHeight = meterArea.getHeight();

    // Input meter
    auto inputMeterBounds = meterArea.removeFromLeft(meterWidth).toFloat();

    // Background
    g.setColour(juce::Colour(0xFF252530));
    g.fillRoundedRectangle(inputMeterBounds, 6.0f);

    // Fill
    auto inputFillHeight = meterHeight * juce::jlimit(0.0f, 1.0f, displayInputLevel);
    g.setColour(juce::Colour(0xFF22c55e));
    g.fillRoundedRectangle(
        inputMeterBounds.getX() + 2,
        inputMeterBounds.getBottom() - inputFillHeight - 2,
        inputMeterBounds.getWidth() - 4,
        inputFillHeight,
        4.0f);

    // Glow
    if (displayInputLevel > 0.1f)
    {
        g.setColour(juce::Colour(0xFF22c55e).withAlpha(displayInputLevel * 0.3f));
        g.fillRoundedRectangle(inputMeterBounds.expanded(2), 8.0f);
    }

    meterArea.removeFromLeft(8);  // Spacing

    // Output meter
    auto outputMeterBounds = meterArea.toFloat();

    // Background
    g.setColour(juce::Colour(0xFF252530));
    g.fillRoundedRectangle(outputMeterBounds, 6.0f);

    // Fill
    auto outputFillHeight = meterHeight * juce::jlimit(0.0f, 1.0f, displayOutputLevel);
    g.setColour(juce::Colour(0xFFff6b35));
    g.fillRoundedRectangle(
        outputMeterBounds.getX() + 2,
        outputMeterBounds.getBottom() - outputFillHeight - 2,
        outputMeterBounds.getWidth() - 4,
        outputFillHeight,
        4.0f);

    // Glow
    if (displayOutputLevel > 0.1f)
    {
        g.setColour(juce::Colour(0xFFff6b35).withAlpha(displayOutputLevel * 0.3f));
        g.fillRoundedRectangle(outputMeterBounds.expanded(2), 8.0f);
    }

    // Meter labels
    g.setColour(juce::Colour(0xFF555566));
    g.setFont(juce::Font(9.0f).withExtraKerningFactor(0.1f));
    g.drawText("IN", inputMeterBounds.toNearestInt().translated(0, -18), juce::Justification::centred);
    g.drawText("OUT", outputMeterBounds.toNearestInt().translated(0, -18), juce::Justification::centred);

    // Footer text
    g.setColour(juce::Colour(0xFF444455));
    g.setFont(juce::Font(10.0f).withExtraKerningFactor(0.05f));
    g.drawText("BeatConnect Example Plugin v1.0",
               getLocalBounds().removeFromBottom(50),
               juce::Justification::centred);
}

void ExamplePluginNativeEditor::resized()
{
    auto area = getLocalBounds();

    // Title at top
    titleLabel.setBounds(area.removeFromTop(70));

    // Remove meter area from right
    area.removeFromRight(70);

    // Remove footer area
    area.removeFromBottom(50);

    // Knobs area - centered
    auto knobArea = area.reduced(40, 20);
    auto knobWidth = knobArea.getWidth() / 2;

    // Gain knob
    auto gainArea = knobArea.removeFromLeft(knobWidth);
    gainSlider.setBounds(gainArea.removeFromTop(140).reduced(15, 10));
    gainLabel.setBounds(gainArea.removeFromTop(20));

    // Mix knob
    auto mixArea = knobArea;
    mixSlider.setBounds(mixArea.removeFromTop(140).reduced(15, 10));
    mixLabel.setBounds(mixArea.removeFromTop(20));

    // Bypass at bottom center
    bypassButton.setBounds(
        (getWidth() - 70) / 2 - 50,
        getHeight() - 85,
        100, 30);
}
