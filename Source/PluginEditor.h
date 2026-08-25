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

    // Tooltips, styled and sized: the default LookAndFeel wraps at 400px,
    // which is most of this plugin's own window width, and drops the box
    // right next to the cursor — in a UI this dense that meant tooltips
    // routinely covered several neighbouring knobs at once. A narrower wrap
    // width plus flipping to whichever side/edge keeps the whole box inside
    // the editor (rather than just clamping after the fact) fixes the
    // overlap; the LED-readout look matches the rest of the panel instead
    // of the default plain grey box.
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                            juce::Rectangle<int> parentArea) override
    {
        const auto tl = layoutTooltipText (tipText);
        const int w = (int) std::ceil (tl.getWidth())  + tooltipPadX * 2;
        const int h = (int) std::ceil (tl.getHeight()) + tooltipPadY * 2;

        // Prefer below-right of the cursor; flip to the other side of
        // whichever axis would otherwise run off the editor's edge, so the
        // box never has to be clamped on top of the control being hovered.
        constexpr int gap = 14;
        int x = screenPos.x + gap;
        if (x + w > parentArea.getRight())
            x = screenPos.x - gap - w;

        int y = screenPos.y + gap;
        if (y + h > parentArea.getBottom())
            y = screenPos.y - gap - h;

        return juce::Rectangle<int> (x, y, w, h).constrainedWithin (parentArea);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override
    {
        using namespace GearPalette;
        const juce::Rectangle<float> bounds (0.0f, 0.0f, (float) width, (float) height);

        g.setColour (chassisBottom.withAlpha (0.5f));
        g.fillRoundedRectangle (bounds.translated (0.0f, 1.5f), 6.0f);

        g.setColour (ledBackground.withAlpha (0.97f));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (accent.withAlpha (0.6f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.2f);

        layoutTooltipText (text).draw (g, bounds.reduced ((float) tooltipPadX, (float) tooltipPadY));
    }

private:
    // Kept narrow on purpose (this plugin's own window is only 480px wide
    // by default) — see the comment on getTooltipBounds() above.
    static constexpr int tooltipMaxWidth = 230;
    static constexpr int tooltipPadX = 10;
    static constexpr int tooltipPadY = 8;

    // Shared by getTooltipBounds() and drawTooltip() so both always agree
    // on exactly how the text wraps — drawTooltip only gets a width/height,
    // not the layout that produced them.
    static juce::TextLayout layoutTooltipText (const juce::String& text)
    {
        juce::AttributedString s;
        s.setWordWrap (juce::AttributedString::WordWrap::byWord);
        s.setJustification (juce::Justification::topLeft);
        s.append (text, juce::FontOptions (14.5f, juce::Font::plain), GearPalette::textLight);

        juce::TextLayout tl;
        tl.createLayoutWithBalancedLineLengths (s, (float) tooltipMaxWidth);
        return tl;
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

/** Undo/redo drawn as a curved arrow — a circular sweep with an arrowhead,
    the standard "history" pictogram — rather than relying on a Unicode
    glyph (U+21B6/U+21B7), which renders inconsistently (sometimes as a
    plain tofu box) depending on the host's font fallback. Redo is undo's
    exact mirror image, both generated from the same drawing code. */
class UndoRedoButton : public juce::Button
{
public:
    explicit UndoRedoButton (bool isRedoButton) : juce::Button ({}), isRedo (isRedoButton) {}

    void paintButton (juce::Graphics& g, bool /*isMouseOverButton*/, bool isButtonDown) override
    {
        using namespace GearPalette;

        auto bounds = getLocalBounds().toFloat().reduced (4.0f);
        const auto centre = bounds.getCentre();
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;

        const juce::Colour colour = ! isEnabled() ? metalDark.withAlpha (0.6f)
                                                    : (isButtonDown ? accent.brighter (0.3f) : accent);

        // Same angle convention as RetroLookAndFeel's rotary knob: 0 = up,
        // increasing clockwise. Redo sweeps clockwise (gap at the bottom,
        // curling right); undo is the mirror image (curling left) — same
        // sweep, just negated.
        const float sign = isRedo ? 1.0f : -1.0f;
        const float startAngle = sign * juce::degreesToRadians (-125.0f);
        const float endAngle   = sign * juce::degreesToRadians (125.0f);

        auto pointOnArc = [&] (float angle)
        {
            return centre.translated (radius * std::sin (angle), -radius * std::cos (angle));
        };

        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
        g.setColour (colour);
        g.strokePath (arc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Arrowhead at the arc's leading end. Built from the tangent
        // direction (the vector from a point just behind the tip to the tip
        // itself) rather than a hand-derived rotation angle, so it's always
        // correct regardless of which way the arc sweeps.
        const auto tip = pointOnArc (endAngle);
        const auto justBehind = pointOnArc (endAngle - sign * juce::degreesToRadians (10.0f));
        auto dir = tip - justBehind;
        const float len = dir.getDistanceFromOrigin();
        if (len > 0.0001f)
        {
            dir = juce::Point<float> (dir.x / len, dir.y / len);
            const juce::Point<float> perp (-dir.y, dir.x);
            constexpr float headLen = 6.5f, headWidth = 6.0f;
            const auto base = tip - dir * headLen;

            juce::Path head;
            head.addTriangle (tip, base + perp * (headWidth * 0.5f), base - perp * (headWidth * 0.5f));
            g.setColour (colour);
            g.fillPath (head);
        }
    }

private:
    bool isRedo;
};

/** A rotary slider with a caption underneath and an amber LED-style
    numeric readout — the plugin's whole UI is a handful of these plus
    section groupings. */
class LabelledKnob : public juce::Component
{
public:
    LabelledKnob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& caption,
                  const juce::String& tooltip)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 20);
        slider.setColour (juce::Slider::rotarySliderFillColourId, GearPalette::accent);
        slider.setColour (juce::Slider::rotarySliderOutlineColourId, GearPalette::metalMid);
        slider.setColour (juce::Slider::textBoxTextColourId, GearPalette::ledText);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, GearPalette::ledBackground);
        slider.setColour (juce::Slider::textBoxOutlineColourId, GearPalette::metalDark);
        slider.setTooltip (tooltip);
        addAndMakeVisible (slider);

        label.setText (caption, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)).withExtraKerningFactor (0.08f));
        label.setColour (juce::Label::textColourId, GearPalette::textLight.withAlpha (0.85f));
        label.setTooltip (tooltip);
        addAndMakeVisible (label);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, slider);

        // Double-click resets the knob to the parameter's default — the
        // attachment above configures the slider's range from the
        // parameter, so the default has to be pulled and converted the same
        // way, rather than guessed as e.g. the midpoint.
        if (auto* param = apvts.getParameter (paramID))
        {
            const auto range = apvts.getParameterRange (paramID);
            slider.setDoubleClickReturnValue (true, range.convertFrom0to1 (param->getDefaultValue()));
        }

        // Group everything that happens over one drag into a single undo
        // step, rather than every intermediate value the drag passes
        // through — apvts.undoManager is public specifically for cases
        // like this (see JJBreezeAudioProcessor::undoManager).
        if (auto* um = apvts.undoManager)
        {
            const juce::String transactionName = caption;
            slider.onDragStart = [um, transactionName] { um->beginNewTransaction (transactionName); };
        }
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
    void setUpToggle (LedToggleButton& button, const juce::String& tooltip);
    void drawScrew (juce::Graphics& g, juce::Point<float> centre) const;

    // Leaves the slot being switched away from holding whatever the user
    // last tweaked (so it isn't lost), makes a first-time target slot start
    // as a copy of the current sound (so the initial switch is silent), then
    // recalls the target slot and updates which A/B button reads as active.
    void switchCompareSlot (int targetSlot);
    void updateCompareButtonColours();
    void updateBypassButtonColour();

    // Repopulates presetBox from the factory list plus whatever's on disk
    // in JJBreezeAudioProcessor::getUserPresetsDirectory(), then restores
    // the correct selection. Called at startup and after any save/delete.
    void refreshPresetBox();
    // "Save current as..." — prompts for a name via a small modal dialog,
    // then writes it to disk and selects it.
    void promptAndSaveUserPreset();

    // Bonus for the Standalone build, which has no host-supplied Edit menu
    // of its own; harmless (and likely just inert) when hosted in a DAW,
    // since EDITOR_WANTS_KEYBOARD_FOCUS is off there.
    bool keyPressed (const juce::KeyPress& key) override;

    // Polls the three section-enabled parameters (so a toggle flipped by
    // the host — automation, preset recall — collapses/expands its section
    // too, not just clicks made in this editor), the current program (so
    // picking a preset from the host's own menu updates presetBox too),
    // whether the live patch still matches the selected preset (the
    // "modified" indicator), and undo/redo availability.
    void timerCallback() override;

    // ComboBox item IDs: factory presets use 1..getNumPrograms() (their
    // host program index + 1); user preset items start here instead, well
    // clear of that range, indexed into JJBreezeAudioProcessor::getUserPresetNames().
    static constexpr int firstUserPresetItemId = 1000;

    JJBreezeAudioProcessor& processorRef;
    RetroLookAndFeel retroLookAndFeel;

    // Needed for any child component's setTooltip() text to actually pop up
    // as a tooltip; owns no visible bounds of its own.
    juce::TooltipWindow tooltipWindow { this, 500 };

    juce::Label titleLabel;
    // Tiny build-info readout in the bottom margin, next to the corner
    // screws — just enough to tell which build you're looking at without
    // competing visually with the header. setStateInformation() presets
    // don't carry a version, so this is the only place it shows at all.
    juce::Label versionLabel;
    juce::Label shiftSectionLabel;
    juce::Label vibratoSectionLabel;
    juce::Label warmthSectionLabel;

    // Preset picker: factory presets (JJBreezeAudioProcessor's existing
    // program list — previously only reachable through the host's own,
    // often buried, preset menu) plus any user presets on disk, refreshed
    // by refreshPresetBox().
    juce::ComboBox presetBox;
    juce::TextButton saveButton { "SAVE" }, deleteButton { "DEL" };
    int lastKnownProgram = -1;
    // Empty when a factory preset is the active patch; the name of the
    // active user preset otherwise — presetBox items in both ranges share
    // one control, but only user presets are deletable and only factory
    // presets are tracked by getCurrentProgram().
    juce::String activeUserPresetName;
    // What the currently-selected preset (factory or user) actually
    // contains, captured right after loading it — compared against the live
    // patch each timer tick to show/hide the "modified" indicator.
    std::array<float, ParamIDs::all.size()> activePresetSnapshot {};

    // Undo/redo — see JJBreezeAudioProcessor::undoManager.
    UndoRedoButton undoButton { false }, redoButton { true };

    // Forces all three sections off — the untouched dry signal — without
    // disturbing any knob or which sections were individually on.
    juce::TextButton bypassButton { "BYPASS" };

    // A/B compare — see JJBreezeAudioProcessor::storeCompareSnapshot/recallCompareSnapshot.
    juce::TextButton compareAButton { "A" }, compareBButton { "B" };

    LedToggleButton shiftToggle, vibratoToggle, warmthToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> shiftToggleAttachment,
        vibratoToggleAttachment, warmthToggleAttachment;
    bool shiftWasOn = true, vibratoWasOn = false, warmthWasOn = false;

    // Panel backgrounds drawn behind each group of knobs.
    juce::Rectangle<int> shiftPanelBounds, vibratoPanelBounds, warmthPanelBounds;

    LabelledKnob pitchLKnob, pitchRKnob, delayLKnob, delayRKnob, focusKnob, mixKnob;
    LabelledKnob vibratoRateKnob, vibratoDepthKnob, vibratoMixKnob;
    LabelledKnob warmthToneKnob, warmthDriveKnob, warmthBodyKnob, warmthMixKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JJBreezeAudioProcessorEditor)
};
