#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace GearPalette
{
    // One named colourway for the whole panel — chassis gradient, section
    // card fill, knob metal (light/dark drives the knob cap's gradient, so
    // a "dark" theme also gets a dark knob cap rather than the light one),
    // accent, text and the LED-style numeric readout. Everything that
    // draws itself (RetroLookAndFeel, LedToggleButton, UndoRedoButton, the
    // editor's own paint()) is handed a `const Theme&` rather than reading
    // fixed colours, so the whole plugin can be reskinned live.
    struct Theme
    {
        juce::String id, displayName;
        juce::Colour chassisTop, chassisBottom, panelFill;
        juce::Colour metalLight, metalMid, metalDark;
        juce::Colour accent, accentDim;
        juce::Colour textLight, textMuted;
        juce::Colour ledBackground, ledText;
        // Whether this theme's knob cap is a dark-metal disc (so the
        // indicator pointer is painted in textLight to show up on it) or a
        // light/cream one (so the pointer is painted in accent instead) —
        // matches the design mockup's `dark` flag on its Knob component.
        bool darkKnobCap;
    };

    // The plugin's original look (dark brushed-metal, amber accent) plus
    // three lighter/alternate rack-hardware colourways — same drawing
    // code throughout, just a different palette. Menu order == this order.
    inline const std::array<Theme, 4>& allThemes()
    {
        static const std::array<Theme, 4> themes { {
            { "dark",  "Dark",
              juce::Colour (0xff2d2f34), juce::Colour (0xff141517), juce::Colour (0xff222327),
              juce::Colour (0xffd6d9de), juce::Colour (0xff8d9299), juce::Colour (0xff45484e),
              juce::Colour (0xffe08a3c), juce::Colour (0xff8a5628),
              juce::Colour (0xffe9e7e0), juce::Colour (0xff8b8e94),
              juce::Colour (0xff0e0c0a), juce::Colour (0xffff9d4d),
              true },

            { "cream", "Cream",
              juce::Colour (0xffddd7c6), juce::Colour (0xffbfb79e), juce::Colour (0xffcfc8b3),
              juce::Colour (0xffefe9db), juce::Colour (0xffb9b09a), juce::Colour (0xff83795f),
              juce::Colour (0xff2c6b63), juce::Colour (0xff1c433d),
              juce::Colour (0xff2b2620), juce::Colour (0xff6f6554),
              juce::Colour (0xff1a1712), juce::Colour (0xff6fc2b0),
              false },

            { "olive", "Olive",
              juce::Colour (0xffdfe1c9), juce::Colour (0xffb7ba9c), juce::Colour (0xffc9cdb0),
              juce::Colour (0xffefe9db), juce::Colour (0xffb9b09a), juce::Colour (0xff83795f),
              juce::Colour (0xff2c6b63), juce::Colour (0xff1c433d),
              juce::Colour (0xff292a1f), juce::Colour (0xff66634f),
              juce::Colour (0xff1a1712), juce::Colour (0xff6fc2b0),
              false },

            { "slate", "Slate",
              juce::Colour (0xff51616a), juce::Colour (0xff2c363c), juce::Colour (0xff3a464e),
              juce::Colour (0xff7d8184), juce::Colour (0xff5c6164), juce::Colour (0xff2a2c2e),
              juce::Colour (0xffc9974a), juce::Colour (0xff7a5b2c),
              juce::Colour (0xfff2efe4), juce::Colour (0xffaab0ac),
              juce::Colour (0xff1a1712), juce::Colour (0xffe0b876),
              true },
        } };
        return themes;
    }

    inline const Theme& defaultTheme() { return allThemes()[0]; }

    // Falls back to defaultTheme() for an unrecognised id — e.g. a session
    // saved by a later version of the plugin with a theme this build
    // doesn't know, rather than refusing to load.
    inline const Theme& findTheme (const juce::String& id)
    {
        for (auto& t : allThemes())
            if (t.id == id)
                return t;
        return defaultTheme();
    }
}

/** Analog-gear look for rotary knobs: a knurled brushed-metal cap that
    rotates, a painted indicator line, a printed tick scale, and a thin
    amber value arc — a hardware potentiometer rather than a flat modern
    dial. */
class RetroLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Which colourway to draw with — a pointer rather than a copy so
    // switching themes (JJBreezeAudioProcessorEditor::applyTheme()) is
    // just reassigning this and repainting, and points into allThemes()'s
    // static storage, so it stays valid for the plugin's whole lifetime.
    const GearPalette::Theme* theme = &GearPalette::defaultTheme();
    void setTheme (const GearPalette::Theme& t) { theme = &t; }

    // Matches the design mockup's flat Knob component exactly: a knurled
    // skirt ring, a rotating cap with a straight pointer + jewel tip, and a
    // fixed 7-tick scale — all proportions are percentages of the knob's
    // own diameter (as the mockup's CSS insets were), and the mockup's
    // fixed-pixel details (pointer width, tick width, jewel size) are
    // scaled from its 78px reference knob to whatever size this one is.
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                            float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                            juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
        const auto diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto radius = diameter * 0.5f;
        const auto centre = bounds.getCentre();
        const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const auto accentColour = slider.findColour (juce::Slider::rotarySliderFillColourId);
        const auto pointerColour = theme->darkKnobCap ? theme->textLight : accentColour;
        const float px = diameter / 78.0f; // scales the mockup's fixed-px details

        // Soft cast shadow behind the whole knob, like the mockup's
        // `filter: drop-shadow` on its dial.
        g.setColour (theme->chassisBottom.withAlpha (0.3f));
        g.fillEllipse (juce::Rectangle<float> (diameter, diameter).withCentre (centre.translated (2.0f * px, 3.0f * px)));

        // Fixed 7-tick scale (-135/-90/-45/0/45/90/135) — printed on the
        // panel, doesn't rotate with the knob. Majors are the same length
        // as minors, just thicker/brighter, matching the mockup.
        for (int i = 0; i < 7; ++i)
        {
            const auto t = (float) i / 6.0f;
            const auto tickAngle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
            const bool major = (i % 3) == 0; // 0, 3, 6 -> the -135/0/135 ticks
            const juce::Point<float> outer (centre.x + radius * 0.99f * std::sin (tickAngle),
                                             centre.y - radius * 0.99f * std::cos (tickAngle));
            const juce::Point<float> inner (centre.x + radius * 0.89f * std::sin (tickAngle),
                                             centre.y - radius * 0.89f * std::cos (tickAngle));
            g.setColour (major ? theme->textLight.withAlpha (0.85f) : theme->textMuted.withAlpha (0.5f));
            g.drawLine ({ inner, outer }, (major ? 2.6f : 2.0f) * px);
        }

        // Skirt (inset 13% each side -> 74% of the knob's diameter).
        const auto skirtRadius = radius * 0.74f;
        const juce::Rectangle<float> skirtBounds (juce::Rectangle<float> (skirtRadius * 2.0f, skirtRadius * 2.0f).withCentre (centre));
        juce::ColourGradient skirtGrad (theme->metalMid, centre.x - skirtRadius * 0.32f, centre.y - skirtRadius * 0.44f,
                                         theme->chassisBottom, centre.x, centre.y, true);
        g.setGradientFill (skirtGrad);
        g.fillEllipse (skirtBounds);

        // Cap (inset 17% of the skirt -> 66% of the skirt's diameter),
        // rotating with the knob's value.
        const auto capRadius = skirtRadius * 0.66f;
        const juce::Rectangle<float> capBounds (juce::Rectangle<float> (capRadius * 2.0f, capRadius * 2.0f).withCentre (centre));
        juce::ColourGradient capGrad (theme->metalLight, centre.x - capRadius * 0.5f, centre.y - capRadius * 0.6f,
                                       theme->metalDark, centre.x + capRadius * 0.6f, centre.y + capRadius * 0.7f, false);
        g.setGradientFill (capGrad);
        {
            juce::Graphics::ScopedSaveState save (g);
            g.addTransform (juce::AffineTransform::rotation (angle, centre.x, centre.y));
            g.fillEllipse (capBounds);
        }

        // Pointer + jewel tip, in the cap's local (unrotated) frame — a
        // short bar in the cap's upper half plus a small dot just past its
        // outer end, then both rotated together with the cap's value angle.
        auto pointerTransform = juce::AffineTransform::rotation (angle).translated (centre);

        juce::Path pointer;
        const float pointerWidth = 3.0f * px;
        const float pointerNear = -0.32f * capRadius, pointerFar = -0.8f * capRadius;
        pointer.addRoundedRectangle (-pointerWidth * 0.5f, pointerFar, pointerWidth, pointerNear - pointerFar, pointerWidth * 0.4f);
        g.setColour (pointerColour);
        g.fillPath (pointer, pointerTransform);

        const auto jewel = juce::Point<float> (0.0f, -0.84f * capRadius).transformedBy (pointerTransform);
        const float jewelDiameter = 5.0f * px;
        g.setColour (accentColour.withAlpha (0.55f));
        g.fillEllipse (juce::Rectangle<float> (jewelDiameter * 2.4f, jewelDiameter * 2.4f).withCentre (jewel));
        g.setColour (accentColour);
        g.fillEllipse (juce::Rectangle<float> (jewelDiameter, jewelDiameter).withCentre (jewel));
    }

    // The numeric readout under each knob (a Slider's built-in text box) —
    // restyled as the mockup's small dark LCD-style "chip" (monospace
    // digits, tight rounded pill) rather than LookAndFeel_V4's plain
    // rectangle, and tagged by name so drawLabel() below can tell it apart
    // from every other Label in the editor (title, captions, section
    // headings) without touching their look.
    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (slider);
        l->setName ("knobValueChip");
        l->setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 10.5f, juce::Font::bold)));
        l->setBorderSize (juce::BorderSize<int> (1, 4, 1, 4));
        return l;
    }

    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        if (label.getName() != "knobValueChip")
        {
            LookAndFeel_V4::drawLabel (g, label);
            return;
        }

        auto bounds = label.getLocalBounds().toFloat();
        g.setColour (label.findColour (juce::Label::backgroundColourId));
        g.fillRoundedRectangle (bounds, 3.0f);

        if (! label.isBeingEdited())
        {
            g.setColour (label.findColour (juce::Label::textColourId));
            g.setFont (label.getFont());
            g.drawFittedText (label.getText(), label.getLocalBounds(), label.getJustificationType(), 1);
        }
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
        const juce::Rectangle<float> bounds (0.0f, 0.0f, (float) width, (float) height);

        g.setColour (theme->chassisBottom.withAlpha (0.5f));
        g.fillRoundedRectangle (bounds.translated (0.0f, 1.5f), 6.0f);

        g.setColour (theme->ledBackground.withAlpha (0.97f));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (theme->accent.withAlpha (0.6f));
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
    // not the layout that produced them. Not static (unlike before) since
    // it now needs `theme` for the text colour.
    juce::TextLayout layoutTooltipText (const juce::String& text) const
    {
        juce::AttributedString s;
        s.setWordWrap (juce::AttributedString::WordWrap::byWord);
        s.setJustification (juce::Justification::topLeft);
        s.append (text, juce::FontOptions (14.5f, juce::Font::plain), theme->textLight);

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
    const GearPalette::Theme* theme = &GearPalette::defaultTheme();
    void setTheme (const GearPalette::Theme& t) { theme = &t; repaint(); }

    // Matches the design mockup's Toggle component: a small vertical slot
    // with a lever that sits at the top (on) or bottom (off), glowing
    // accent-coloured when on — a physical rocker/slide switch, not an
    // illuminated pushbutton. Fit to whatever bounds this button is given,
    // at the mockup's 17:30 slot aspect ratio.
    void paintButton (juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        auto full = getLocalBounds().toFloat();
        constexpr float slotAspect = 17.0f / 30.0f; // width / height
        float slotHeight = full.getHeight();
        float slotWidth = slotHeight * slotAspect;
        if (slotWidth > full.getWidth())
        {
            slotWidth = full.getWidth();
            slotHeight = slotWidth / slotAspect;
        }
        const auto slot = juce::Rectangle<float> (slotWidth, slotHeight).withCentre (full.getCentre());
        const bool on = getToggleState();

        juce::ColourGradient slotGrad (theme->ledBackground.brighter (0.1f), slot.getX(), slot.getY(),
                                        theme->chassisBottom, slot.getX(), slot.getBottom(), false);
        g.setGradientFill (slotGrad);
        g.fillRoundedRectangle (slot, slotWidth * 0.18f);

        // The lever's own colour is fixed cream metal regardless of theme
        // (same as the mockup, which never themes it) - only its glow when
        // on picks up the theme's accent.
        const float margin = slotWidth * 0.15f;
        const float leverWidth = slotWidth - margin * 2.0f;
        const float leverHeight = slotHeight * 0.43f;
        const float leverTop = on ? slotHeight * 0.08f : slotHeight * (1.0f - 0.43f - 0.08f);
        const auto lever = juce::Rectangle<float> (leverWidth, leverHeight)
                                .withPosition (slot.getX() + margin, slot.getY() + leverTop);

        if (on)
        {
            g.setColour (theme->accent.withAlpha (isButtonDown ? 0.55f : 0.4f));
            g.fillRoundedRectangle (lever.expanded (slotWidth * 0.28f), leverHeight * 0.5f);
        }

        juce::ColourGradient leverGrad (juce::Colour (0xffefe9db), lever.getX(), lever.getY(),
                                         juce::Colour (0xffa9a08c), lever.getRight(), lever.getBottom(), false);
        g.setGradientFill (leverGrad);
        g.fillRoundedRectangle (lever, leverWidth * 0.2f);

        if (isMouseOverButton)
        {
            g.setColour (juce::Colours::white.withAlpha (isButtonDown ? 0.1f : 0.05f));
            g.fillRoundedRectangle (slot, slotWidth * 0.18f);
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

    const GearPalette::Theme* theme = &GearPalette::defaultTheme();
    void setTheme (const GearPalette::Theme& t) { theme = &t; repaint(); }

    void paintButton (juce::Graphics& g, bool /*isMouseOverButton*/, bool isButtonDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (4.0f);
        const auto centre = bounds.getCentre();
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;

        const juce::Colour colour = ! isEnabled() ? theme->metalDark.withAlpha (0.6f)
                                                    : (isButtonDown ? theme->accent.brighter (0.3f) : theme->accent);

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
        // A tight LCD-style chip rather than a full-width box — see
        // RetroLookAndFeel::createSliderTextBox()/drawLabel().
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 15);
        slider.setTooltip (tooltip);
        addAndMakeVisible (slider);

        label.setText (caption, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)).withExtraKerningFactor (0.08f));
        label.setTooltip (tooltip);
        addAndMakeVisible (label);

        // Colours set via setColour() are cached on the component, so a
        // later theme switch has to re-apply them explicitly — see
        // applyTheme() below and JJBreezeAudioProcessorEditor::applyTheme().
        applyTheme (*theme);

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

    // Re-applies every colour this component cached via setColour() —
    // called once from the constructor above (against whatever theme was
    // live at that moment) and again by
    // JJBreezeAudioProcessorEditor::applyTheme() on every theme switch.
    void applyTheme (const GearPalette::Theme& t)
    {
        theme = &t;
        slider.setColour (juce::Slider::rotarySliderFillColourId, theme->accent);
        slider.setColour (juce::Slider::rotarySliderOutlineColourId, theme->metalMid);
        slider.setColour (juce::Slider::textBoxTextColourId, theme->ledText);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, theme->ledBackground);
        slider.setColour (juce::Slider::textBoxOutlineColourId, theme->metalDark);
        label.setColour (juce::Label::textColourId, theme->textLight.withAlpha (0.85f));
        repaint();
    }

private:
    const GearPalette::Theme* theme = &GearPalette::defaultTheme();
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
    // A single rack-mount bolt, like the ones in the rack ears' left/right
    // strips (see paint()) - matches the design mockup's flat radial-
    // gradient bolt exactly, no screwdriver slot.
    void drawBolt (juce::Graphics& g, juce::Point<float> centre, float radius) const;

    // Leaves the slot being switched away from holding whatever the user
    // last tweaked (so it isn't lost), makes a first-time target slot start
    // as a copy of the current sound (so the initial switch is silent), then
    // recalls the target slot and updates which A/B button reads as active.
    void switchCompareSlot (int targetSlot);
    void updateCompareButtonColours();
    void updateBypassToggleState();

    // Switches the whole panel's colourway: updates currentTheme, hands it
    // to every child that draws with one (the LookAndFeel, both toggle
    // styles, every knob), re-applies the colours the rest of the editor's
    // own components cached via setColour(), persists the choice on the
    // processor (so it survives a session save/reload — see
    // JJBreezeAudioProcessor::uiThemeId), and repaints.
    void applyTheme (const juce::String& themeId);

    // Repopulates presetBox from the factory list plus whatever's on disk
    // in JJBreezeAudioProcessor::getUserPresetsDirectory(), then restores
    // the correct selection. Called at startup and after any save/delete.
    void refreshPresetBox();
    // "Save current as..." — prompts for a name via a small modal dialog,
    // then writes it to disk and selects it.
    void promptAndSaveUserPreset();
    // Confirms via a modal dialog (deletion isn't undoable) before actually
    // deleting the selected user preset.
    void promptAndDeleteUserPreset();

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

    // The colourway every themed component below currently draws with —
    // points into GearPalette::allThemes()'s static storage (permanent for
    // the plugin's lifetime), reassigned by applyTheme(). Read directly by
    // this editor's own paint()/drawBolt()/updateCompareButtonColours()/
    // updateBypassToggleState()/setUpSectionLabel(); everything else
    // (RetroLookAndFeel, LedToggleButton, UndoRedoButton, LabelledKnob)
    // keeps its own copy of the pointer via setTheme()/applyTheme().
    const GearPalette::Theme* currentTheme = &GearPalette::defaultTheme();
    // Lets the user pick a colourway — populated from GearPalette::allThemes()
    // in the constructor, selection persisted via processorRef.uiThemeId.
    juce::ComboBox themeBox;

    // Needed for any child component's setTooltip() text to actually pop up
    // as a tooltip; owns no visible bounds of its own.
    juce::TooltipWindow tooltipWindow { this, 500 };

    // Nameplate: an italic serif wordmark plus a small tracked-caps
    // subtitle, sharing one baseline, left-aligned - see resized() and the
    // design mockup this matches.
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    // Tiny build-info readout in the bottom margin, next to the rack-ear
    // bolts — just enough to tell which build you're looking at without
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

    // Labelled POWER in the header — on = processing normally, off =
    // bypassed (forces all three sections off, the untouched dry signal,
    // without disturbing any knob or which sections were individually on).
    // Drawn state is the *inverse* of processorRef.isBypassed() (a power
    // switch reads on when active); not a click-toggling button, see
    // updateBypassToggleState(). Sits in the header on the same row as the
    // nameplate, like the design mockup's POWER switch.
    LedToggleButton bypassButton;
    juce::Label bypassCaptionLabel;

    // A/B compare — see JJBreezeAudioProcessor::storeCompareSnapshot/recallCompareSnapshot.
    juce::TextButton compareAButton { "A" }, compareBButton { "B" };

    LedToggleButton shiftToggle, vibratoToggle, warmthToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> shiftToggleAttachment,
        vibratoToggleAttachment, warmthToggleAttachment;
    bool shiftWasOn = true, vibratoWasOn = false, warmthWasOn = false;

    LabelledKnob pitchLKnob, pitchRKnob, delayLKnob, delayRKnob, focusKnob, mixKnob;
    LabelledKnob vibratoRateKnob, vibratoDepthKnob, vibratoMixKnob;
    LabelledKnob warmthToneKnob, warmthDriveKnob, warmthBodyKnob, warmthMixKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JJBreezeAudioProcessorEditor)
};
