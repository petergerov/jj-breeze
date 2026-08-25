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
        const auto arcRadius = radius * 0.76f;
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (GearPalette::metalDark.withAlpha (0.9f));
        g.strokePath (track, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
        g.setColour (accentColour);
        g.strokePath (valueArc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Cast shadow under the cap, for a raised/moulded feel.
        const auto faceRadius = radius * 0.60f;
        g.setColour (GearPalette::chassisBottom.withAlpha (0.5f));
        g.fillEllipse (juce::Rectangle<float> (faceRadius * 2.0f, faceRadius * 2.0f)
                            .withCentre (centre.translated (1.5f, 2.5f)));

        // Brushed-metal knob cap, lit from the upper-left.
        const juce::Rectangle<float> faceBounds (centre.x - faceRadius, centre.y - faceRadius,
                                                   faceRadius * 2.0f, faceRadius * 2.0f);
        juce::ColourGradient faceGrad (GearPalette::metalLight, centre.x - faceRadius * 0.5f, centre.y - faceRadius * 0.6f,
                                        GearPalette::metalDark, centre.x + faceRadius * 0.6f, centre.y + faceRadius * 0.7f, false);
        g.setGradientFill (faceGrad);
        g.fillEllipse (faceBounds);
        g.setColour (GearPalette::chassisBottom.withAlpha (0.8f));
        g.drawEllipse (faceBounds, 1.2f);

        // Knurled grip ridges around the cap's rim — rotate with the knob,
        // since they're moulded into the knob body itself.
        {
            juce::Graphics::ScopedSaveState save (g);
            g.addTransform (juce::AffineTransform::rotation (angle, centre.x, centre.y));
            constexpr int numRidges = 24;
            for (int i = 0; i < numRidges; ++i)
            {
                const auto ridgeAngle = (float) i / (float) numRidges * juce::MathConstants<float>::twoPi;
                const juce::Point<float> inner (centre.x + faceRadius * 0.88f * std::sin (ridgeAngle),
                                                 centre.y - faceRadius * 0.88f * std::cos (ridgeAngle));
                const juce::Point<float> outer (centre.x + faceRadius * 0.99f * std::sin (ridgeAngle),
                                                 centre.y - faceRadius * 0.99f * std::cos (ridgeAngle));
                g.setColour (GearPalette::chassisBottom.withAlpha (0.5f));
                g.drawLine ({ inner, outer }, 1.0f);
            }
        }

        // Painted indicator line, like the white/orange stripe on a
        // hardware pot knob.
        juce::Path pointer;
        const float pointerLength = faceRadius * 0.80f;
        const float pointerThickness = 3.0f;
        pointer.addRoundedRectangle (-pointerThickness * 0.5f, -pointerLength,
                                      pointerThickness, pointerLength * 0.6f, pointerThickness * 0.5f);
        g.setColour (accentColour);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));

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

class JJBreezeAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit JJBreezeAudioProcessorEditor (JJBreezeAudioProcessor&);
    ~JJBreezeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void setUpSectionLabel (juce::Label& label, const juce::String& text);
    void drawScrew (juce::Graphics& g, juce::Point<float> centre) const;

    JJBreezeAudioProcessor& processorRef;
    RetroLookAndFeel retroLookAndFeel;

    juce::Image portholeImage;
    juce::Rectangle<int> portholeBounds;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label widthSectionLabel;
    juce::Label slapSectionLabel;
    juce::Label vibratoSectionLabel;

    // Panel backgrounds drawn behind each group of knobs.
    juce::Rectangle<int> widthPanelBounds, slapPanelBounds, vibratoPanelBounds;

    LabelledKnob pitchLKnob, pitchRKnob, delayLKnob, delayRKnob, focusKnob, mixKnob;
    LabelledKnob slapTimeKnob, slapFeedbackKnob, slapMixKnob;
    LabelledKnob vibratoRateKnob, vibratoDepthKnob, vibratoMixKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JJBreezeAudioProcessorEditor)
};
