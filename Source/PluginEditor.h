#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace GearPalette
{
    // A dark brushed-metal chassis with a single warm amber accent — the
    // colour language of a piece of rack hardware (a Neve/API-style knob
    // line, an old tube unit's indicator light) rather than a paper sleeve.
    static const juce::Colour chassisTop     (0xff2d2f34);
    static const juce::Colour chassisBottom  (0xff141517);
    static const juce::Colour panelFill      (0xff222327);
    static const juce::Colour metalLight     (0xffd6d9de);
    static const juce::Colour metalMid       (0xff8d9299);
    static const juce::Colour metalDark      (0xff45484e);
    static const juce::Colour accent         (0xffe08a3c);
    static const juce::Colour accentDim      (0xff8a5628);
    static const juce::Colour textLight      (0xffe9e7e0);
    static const juce::Colour textMuted      (0xff8b8e94);
    static const juce::Colour ledBackground  (0xff0e0c0a);
    static const juce::Colour ledText        (0xffff9d4d);
}

/** Analog-gear look for rotary knobs: a knurled brushed-metal cap that
    rotates, a painted indicator line, a printed tick scale, and a thin
    amber value arc — a hardware potentiometer rather than a flat modern
    dial. */
class RetroLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                            float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                            juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const auto accentColour = slider.findColour (juce::Slider::rotarySliderFillColourId);

        // Printed scale ticks on the panel — static, don't rotate with the knob.
        constexpr int numTicks = 11;
        for (int i = 0; i < numTicks; ++i)
        {
            const auto t = (float) i / (float) (numTicks - 1);
            const auto tickAngle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
            const bool major = (i == 0 || i == numTicks - 1 || i == numTicks / 2);
            const juce::Point<float> inner (centre.x + (radius * 0.88f) * std::sin (tickAngle),
                                             centre.y - (radius * 0.88f) * std::cos (tickAngle));
            const juce::Point<float> outer (centre.x + radius * std::sin (tickAngle),
                                             centre.y - radius * std::cos (tickAngle));
            g.setColour (GearPalette::textMuted.withAlpha (major ? 0.85f : 0.45f));
            g.drawLine ({ inner, outer }, major ? 1.6f : 1.0f);
        }

        // Value arc — thin printed track with an amber fill, like a
        // hardware gain-reduction scale rather than a glowing progress ring.
        const auto arcRadius = radius * 0.82f;
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (GearPalette::metalDark.withAlpha (0.9f));
        g.strokePath (track, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
        g.setColour (accentColour);
        g.strokePath (valueArc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // A real hardware pot knob reads as two pieces: a knurled skirt
        // (the moulded base, gripped to turn) topped by a slightly smaller,
        // brighter cap that carries the indicator line — rather than one
        // flat disc.
        const auto skirtRadius = radius * 0.66f;
        const auto capRadius   = skirtRadius * 0.80f;

        // Cast shadow grounding the knob on the panel.
        g.setColour (GearPalette::chassisBottom.withAlpha (0.55f));
        g.fillEllipse (juce::Rectangle<float> (skirtRadius * 2.0f, skirtRadius * 2.0f)
                            .withCentre (centre.translated (1.5f, 2.5f)));

        // Skirt — dark gunmetal, lit from the upper-left.
        const juce::Rectangle<float> skirtBounds (centre.x - skirtRadius, centre.y - skirtRadius,
                                                    skirtRadius * 2.0f, skirtRadius * 2.0f);
        juce::ColourGradient skirtGrad (GearPalette::metalMid, centre.x - skirtRadius * 0.5f, centre.y - skirtRadius * 0.6f,
                                         GearPalette::chassisBottom, centre.x + skirtRadius * 0.6f, centre.y + skirtRadius * 0.7f, false);
        g.setGradientFill (skirtGrad);
        g.fillEllipse (skirtBounds);
        g.setColour (GearPalette::chassisBottom.withAlpha (0.9f));
        g.drawEllipse (skirtBounds, 1.2f);

        // Knurled grip ridges around the skirt — rotate with the knob,
        // since they're moulded into the knob body itself.
        {
            juce::Graphics::ScopedSaveState save (g);
            g.addTransform (juce::AffineTransform::rotation (angle, centre.x, centre.y));
            constexpr int numRidges = 28;
            for (int i = 0; i < numRidges; ++i)
            {
                const auto ridgeAngle = (float) i / (float) numRidges * juce::MathConstants<float>::twoPi;
                const juce::Point<float> inner (centre.x + skirtRadius * 0.84f * std::sin (ridgeAngle),
                                                 centre.y - skirtRadius * 0.84f * std::cos (ridgeAngle));
                const juce::Point<float> outer (centre.x + skirtRadius * 0.97f * std::sin (ridgeAngle),
                                                 centre.y - skirtRadius * 0.97f * std::cos (ridgeAngle));
                g.setColour (GearPalette::chassisBottom.withAlpha (0.6f));
                g.drawLine ({ inner, outer }, 1.1f);
            }
        }

        // Cap — brighter brushed aluminium, domed, sitting on top of the skirt.
        const juce::Rectangle<float> capBounds (centre.x - capRadius, centre.y - capRadius,
                                                  capRadius * 2.0f, capRadius * 2.0f);
        juce::ColourGradient capGrad (GearPalette::metalLight, centre.x - capRadius * 0.5f, centre.y - capRadius * 0.6f,
                                       GearPalette::metalDark, centre.x + capRadius * 0.6f, centre.y + capRadius * 0.7f, false);
        g.setGradientFill (capGrad);
        g.fillEllipse (capBounds);
        g.setColour (GearPalette::chassisBottom.withAlpha (0.85f));
        g.drawEllipse (capBounds, 1.0f);

        // Specular highlight — a soft glint that sells the dome.
        {
            const auto glintCentre = centre.translated (-capRadius * 0.32f, -capRadius * 0.38f);
            juce::ColourGradient glintGrad (juce::Colours::white.withAlpha (0.5f), glintCentre.x, glintCentre.y,
                                             juce::Colours::white.withAlpha (0.0f), glintCentre.x, glintCentre.y + capRadius * 0.7f, true);
            g.setGradientFill (glintGrad);
            g.fillEllipse (juce::Rectangle<float> (capRadius * 1.1f, capRadius * 0.85f).withCentre (glintCentre));
        }

        // Painted indicator line with a bright jewel tip, like the
        // white/orange stripe (and inset marker) on a hardware pot knob.
        juce::Path pointer;
        const float pointerLength = capRadius * 0.82f;
        const float pointerThickness = 2.6f;
        pointer.addRoundedRectangle (-pointerThickness * 0.5f, -pointerLength,
                                      pointerThickness, pointerLength * 0.62f, pointerThickness * 0.5f);
        auto pointerTransform = juce::AffineTransform::rotation (angle).translated (centre);
        g.setColour (GearPalette::chassisBottom.withAlpha (0.5f));
        g.fillPath (pointer, pointerTransform.translated (0.6f, 0.6f));
        g.setColour (accentColour);
        g.fillPath (pointer, pointerTransform);

        const auto jewel = juce::Point<float> (0.0f, -pointerLength).transformedBy (pointerTransform);
        g.setColour (accentColour.brighter (0.5f));
        g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (jewel));

        // Centre hub with a screwdriver slot.
        const auto hub = juce::Rectangle<float> (7.0f, 7.0f).withCentre (centre);
        juce::ColourGradient hubGrad (GearPalette::metalMid, hub.getX(), hub.getY(),
                                       GearPalette::chassisBottom, hub.getRight(), hub.getBottom(), false);
        g.setGradientFill (hubGrad);
        g.fillEllipse (hub);
        g.setColour (GearPalette::chassisBottom);
        g.drawLine ({ hub.getCentre().translated (-2.2f, -1.0f), hub.getCentre().translated (2.2f, 1.0f) }, 1.0f);
    }
};

/** A small illuminated pushbutton, like a hardware unit's IN/bypass switch —
    lights amber when the section it belongs to is on. */
class LedToggleButton : public juce::ToggleButton
{
public:
    void paintButton (juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        using namespace GearPalette;

        auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        const bool on = getToggleState();

        juce::ColourGradient housingGrad (metalMid, bounds.getX(), bounds.getY(),
                                           chassisBottom, bounds.getRight(), bounds.getBottom(), false);
        g.setGradientFill (housingGrad);
        g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);
        g.setColour (chassisBottom.withAlpha (0.9f));
        g.drawRoundedRectangle (bounds, bounds.getHeight() * 0.5f, 1.0f);

        const auto ledCentre = bounds.getCentre();
        const float ledRadius = bounds.getHeight() * 0.26f;

        if (on)
        {
            g.setColour (accent.withAlpha (0.30f));
            g.fillEllipse (juce::Rectangle<float> (ledRadius * 3.6f, ledRadius * 3.6f).withCentre (ledCentre));
        }

        g.setColour (on ? accent : juce::Colour (0xff35342f));
        g.fillEllipse (juce::Rectangle<float> (ledRadius * 2.0f, ledRadius * 2.0f).withCentre (ledCentre));
        g.setColour (chassisBottom.withAlpha (0.7f));
        g.drawEllipse (juce::Rectangle<float> (ledRadius * 2.0f, ledRadius * 2.0f).withCentre (ledCentre), 0.8f);

        if (isMouseOverButton)
        {
            g.setColour (juce::Colours::white.withAlpha (isButtonDown ? 0.14f : 0.07f));
            g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);
        }
    }
};

/** A rotary slider with a caption underneath and an amber LED-style
    numeric readout — the plugin's whole UI is a handful of these plus
    section groupings. */
class LabelledKnob : public juce::Component
{
public:
    LabelledKnob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& caption)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 20);
        slider.setColour (juce::Slider::rotarySliderFillColourId, GearPalette::accent);
        slider.setColour (juce::Slider::rotarySliderOutlineColourId, GearPalette::metalMid);
        slider.setColour (juce::Slider::textBoxTextColourId, GearPalette::ledText);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, GearPalette::ledBackground);
        slider.setColour (juce::Slider::textBoxOutlineColourId, GearPalette::metalDark);
        addAndMakeVisible (slider);

        label.setText (caption, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)).withExtraKerningFactor (0.08f));
        label.setColour (juce::Label::textColourId, GearPalette::textLight.withAlpha (0.85f));
        addAndMakeVisible (label);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, slider);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        label.setBounds (area.removeFromTop (20));
        slider.setBounds (area);
    }

private:
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class JJBreezeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit JJBreezeAudioProcessorEditor (JJBreezeAudioProcessor&);
    ~JJBreezeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void setUpSectionLabel (juce::Label& label, const juce::String& text);
    void setUpToggle (LedToggleButton& button);
    void drawScrew (juce::Graphics& g, juce::Point<float> centre) const;

    // Polls the three section-enabled parameters so a toggle flipped by the
    // host (automation, preset recall) collapses/expands its section too,
    // not just clicks made in this editor.
    void timerCallback() override;

    JJBreezeAudioProcessor& processorRef;
    RetroLookAndFeel retroLookAndFeel;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label shiftSectionLabel;
    juce::Label slapSectionLabel;
    juce::Label vibratoSectionLabel;

    LedToggleButton shiftToggle, slapToggle, vibratoToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> shiftToggleAttachment,
        slapToggleAttachment, vibratoToggleAttachment;
    bool shiftWasOn = true, slapWasOn = false, vibratoWasOn = false;

    // Panel backgrounds drawn behind each group of knobs.
    juce::Rectangle<int> shiftPanelBounds, slapPanelBounds, vibratoPanelBounds;

    LabelledKnob pitchLKnob, pitchRKnob, delayLKnob, delayRKnob, focusKnob, mixKnob;
    LabelledKnob slapTimeKnob, slapFeedbackKnob, slapMixKnob;
    LabelledKnob vibratoRateKnob, vibratoDepthKnob, vibratoMixKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JJBreezeAudioProcessorEditor)
};
