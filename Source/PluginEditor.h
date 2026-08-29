#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "GearTheme.h"
#include "PluginProcessor.h"

/** Analog-gear look for rotary knobs: a black bakelite pointer knob seated
    in a chrome collar, inside a scale printed on the panel — the knob on
    most 60s-70s outboard gear. The only concession to the screen is the
    thin accent arc tracking the printed scale, which reads at a glance
    where a bare pointer would not. Ported from the AUv3 sibling's
    AnalogKnob (../jj-breeze-auv3, JJBreezeExtension/UI/KnobView.swift). */
class RetroLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Which colourway to draw with — a pointer rather than a copy so
    // switching finishes (JJBreezeAudioProcessorEditor::applyTheme()) is
    // just reassigning this and repainting, and points into allThemes()'s
    // static storage, so it stays valid for the plugin's whole lifetime.
    const GearPalette::Theme* theme = &GearPalette::defaultTheme();
    void setTheme (const GearPalette::Theme& t) { theme = &t; }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                            float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                            juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
        const auto diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto radius = diameter * 0.5f;
        const auto centre = bounds.getCentre();
        const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
        const float unit = diameter / 78.0f; // the design's reference knob size

        auto pointOnCircle = [&] (float a, float r)
        {
            return juce::Point<float> (centre.x + std::sin (a) * r, centre.y - std::cos (a) * r);
        };

        // --- Scale printed on the panel around the knob ------------------
        for (int i = 0; i < 11; ++i)
        {
            const float t = (float) i / 10.0f;
            const float tickAngle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
            const bool major = (i % 5) == 0;
            g.setColour (theme->textLight.withAlpha (major ? 0.9f : 0.55f));
            g.drawLine ({ pointOnCircle (tickAngle, radius * (major ? 0.80f : 0.86f)),
                          pointOnCircle (tickAngle, radius * 0.97f) },
                        (major ? 2.2f : 1.4f) * unit);
        }

        // --- Value arc ---------------------------------------------------
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius * 0.885f, radius * 0.885f, 0.0f,
                            rotaryStartAngle, angle, true);
        g.setColour (accent.withAlpha (0.18f));
        g.strokePath (arc, juce::PathStrokeType (4.6f * unit, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent.withAlpha (0.92f));
        g.strokePath (arc, juce::PathStrokeType (2.0f * unit, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // --- Chrome collar the knob is seated in -------------------------
        const float collarRadius = radius * 0.74f;
        const auto collar = juce::Rectangle<float> (collarRadius * 2.0f, collarRadius * 2.0f).withCentre (centre);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (collar.translated (0.0f, 2.6f * unit));

        juce::ColourGradient collarFill (theme->metalLight, collar.getX(), collar.getY(),
                                          theme->metalMid, collar.getRight(), collar.getBottom(), false);
        collarFill.addColour (0.33, theme->metalMid);
        collarFill.addColour (0.66, theme->metalDark);
        g.setGradientFill (collarFill);
        g.fillEllipse (collar);

        // --- Bakelite cap ------------------------------------------------
        const float capRadius = collarRadius * 0.86f;
        const auto cap = juce::Rectangle<float> (capRadius * 2.0f, capRadius * 2.0f).withCentre (centre);
        g.setGradientFill (RetroDraw::radial (theme->bakeliteLight, theme->bakeliteDark,
                                               centre.translated (-capRadius * 0.4f, -capRadius * 0.5f),
                                               capRadius * 1.5f));
        g.fillEllipse (cap);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawEllipse (cap, 1.2f * unit);

        // Gloss on the moulded top surface.
        const juce::Rectangle<float> gloss (centre.x - capRadius * 0.62f, centre.y - capRadius * 0.78f,
                                             capRadius * 1.05f, capRadius * 0.62f);
        g.setGradientFill (RetroDraw::radial (juce::Colours::white.withAlpha (0.2f), juce::Colours::transparentWhite,
                                               gloss.getCentre(), gloss.getWidth() * 0.6f));
        g.fillEllipse (gloss);

        // --- Pointer, rotating with the value ----------------------------
        {
            juce::Graphics::ScopedSaveState save (g);
            g.addTransform (juce::AffineTransform::rotation (angle, centre.x, centre.y));

            const float pointerWidth = 3.4f * unit;
            const juce::Rectangle<float> pointer (centre.x - pointerWidth * 0.5f, centre.y - capRadius * 0.94f,
                                                   pointerWidth, capRadius * 0.94f);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (pointer.translated (0.0f, 1.2f * unit), pointerWidth * 0.5f);
            g.setColour (theme->textLight);
            g.fillRoundedRectangle (pointer, pointerWidth * 0.5f);
        }
    }

    // The numeric readout under each knob (a Slider's built-in text box) —
    // drawn as a recessed glass window in a chrome bezel (see
    // RetroDraw::ledWindow) rather than LookAndFeel_V4's plain rectangle,
    // and tagged by name so drawLabel() below can tell it apart from every
    // other Label in the editor without touching their look.
    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (slider);
        l->setName ("knobValueChip");
        l->setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::bold)));
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

        RetroDraw::ledWindow (g, *theme, label.getLocalBounds().toFloat().reduced (0.5f), 2.5f);

        if (! label.isBeingEdited())
        {
            g.setColour (label.findColour (juce::Label::textColourId));
            g.setFont (label.getFont());
            g.drawFittedText (label.getText(), label.getLocalBounds(), label.getJustificationType(), 1);
        }
    }

    // The preset picker, as another glass window in the panel — same
    // recessed readout as the knob chips, with a small accent triangle
    // where a hardware unit would have a chrome thumbwheel.
    void drawComboBox (juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                        int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                        juce::ComboBox& box) override
    {
        RetroDraw::ledWindow (g, *theme, juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f), 3.0f);

        juce::Path arrow;
        const float cx = (float) width - 13.0f, cy = (float) height * 0.5f;
        arrow.addTriangle (cx - 4.5f, cy - 2.5f, cx + 4.5f, cy - 2.5f, cx, cy + 3.0f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 1.0f : 0.4f));
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::bold));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (7, 1, box.getWidth() - 26, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    // Text buttons (SAVE, the A/B compare pair) drawn as small machined
    // metal plates rather than flat rounded rectangles — the caller's
    // buttonColourId still picks the base colour, so the A/B pair keeps
    // signalling which slot is live (see updateCompareButtonColours()).
    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                bool isMouseOver, bool isButtonDown) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const auto base = isButtonDown ? backgroundColour.brighter (0.15f)
                                       : (isMouseOver ? backgroundColour.brighter (0.07f) : backgroundColour);

        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (bounds.translated (0.0f, 1.2f), 3.0f);

        g.setGradientFill ({ base.brighter (0.28f), bounds.getCentreX(), bounds.getY(),
                             base.darker (0.35f), bounds.getCentreX(), bounds.getBottom(), false });
        g.fillRoundedRectangle (bounds, 3.0f);

        g.setColour (theme->metalLight.withAlpha (0.22f));
        g.drawLine (bounds.getX() + 2.0f, bounds.getY() + 1.0f, bounds.getRight() - 2.0f, bounds.getY() + 1.0f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (bounds, 3.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int /*buttonHeight*/) override
    {
        return juce::Font (juce::FontOptions (10.5f, juce::Font::bold)).withExtraKerningFactor (0.1f);
    }

    // Tooltips, styled and sized: the default LookAndFeel wraps at 400px,
    // which is most of this plugin's own window width, and drops the box
    // right next to the cursor — in a UI this dense that meant tooltips
    // routinely covered several neighbouring knobs at once. A narrower wrap
    // width plus flipping to whichever side/edge keeps the whole box inside
    // the editor (rather than just clamping after the fact) fixes the
    // overlap; the glass-readout look matches the rest of the panel instead
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
    // not the layout that produced them.
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

/** Silkscreen lettering: a Label that paints its text with a hard shadow
    under it, the way paint sits proud of an enamelled plate. Used for
    every caption printed on the panel (wordmark, section names, knob
    captions) — the plain Label look is left to anything that isn't. */
class EngravedLabel : public juce::Label
{
public:
    void paint (juce::Graphics& g) override
    {
        RetroDraw::engravedText (g, getText(), getLocalBounds(), getFont(),
                                  getJustificationType(), findColour (juce::Label::textColourId));
    }
};

/** A chrome bat switch — the section IN/OUT and POWER controls. Replaces
    the old slide toggle; see RetroDraw::batSwitch. */
class BatSwitchButton : public juce::ToggleButton
{
public:
    const GearPalette::Theme* theme = &GearPalette::defaultTheme();
    void setTheme (const GearPalette::Theme& t) { theme = &t; repaint(); }

    void paintButton (juce::Graphics& g, bool isMouseOverButton, bool /*isButtonDown*/) override
    {
        RetroDraw::batSwitch (g, *theme, getLocalBounds().toFloat(), getToggleState());

        if (isMouseOverButton)
        {
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillEllipse (getLocalBounds().toFloat());
        }
    }
};

/** A jewel indicator lamp in a chrome bezel — one beside each section
    name, and one over the POWER switch. */
class JewelLampComponent : public juce::Component
{
public:
    JewelLampComponent() { setInterceptsMouseClicks (false, false); }

    void setTheme (const GearPalette::Theme& t) { theme = &t; repaint(); }
    void setColourway (juce::Colour c) { colour = c; repaint(); }

    void setOn (bool shouldBeOn)
    {
        if (on == shouldBeOn)
            return;
        on = shouldBeOn;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        RetroDraw::jewelLamp (g, *theme, getLocalBounds().toFloat(), on,
                               colour.isTransparent() ? theme->accent : colour);
    }

private:
    const GearPalette::Theme* theme = &GearPalette::defaultTheme();
    juce::Colour colour { juce::Colours::transparentBlack };
    bool on = false;
};

/** The panel-finish selector: a small rotary switch left of the wordmark,
    its cap painted in whatever finish is currently fitted and its pointer
    standing at that finish's detent. Clicking turns it to the next one —
    the same gesture as reaching up and turning the knob on the front of the
    real thing. */
class FinishSelectorButton : public juce::Button
{
public:
    FinishSelectorButton() : juce::Button ("Panel finish") {}

    const GearPalette::Theme* theme = &GearPalette::defaultTheme();
    void setTheme (const GearPalette::Theme& t) { theme = &t; repaint(); }

    void paintButton (juce::Graphics& g, bool isMouseOverButton, bool /*isButtonDown*/) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto centre = bounds.getCentre();
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const int count = (int) GearPalette::allThemes().size();
        const int index = GearPalette::indexOfTheme (theme->id);

        auto pointOnCircle = [&] (float a, float r)
        {
            return juce::Point<float> (centre.x + std::sin (a) * r, centre.y - std::cos (a) * r);
        };

        // Detents spread over a 70-degree arc, centred — two finishes sit
        // at +/-35 degrees, and more would simply divide the same sweep.
        auto detentAngle = [count] (int i)
        {
            const float steps = (float) juce::jmax (1, count - 1);
            return juce::degreesToRadians (-35.0f + (float) i / steps * 70.0f);
        };

        for (int i = 0; i < count; ++i)
        {
            g.setColour (theme->textLight.withAlpha (i == index ? 0.9f : 0.45f));
            g.drawLine ({ pointOnCircle (detentAngle (i), radius * 0.82f),
                          pointOnCircle (detentAngle (i), radius * 0.99f) }, 1.4f);
        }

        // Chrome bezel.
        const float bezelRadius = radius * 0.74f;
        const auto bezel = juce::Rectangle<float> (bezelRadius * 2.0f, bezelRadius * 2.0f).withCentre (centre);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (bezel.translated (0.0f, 1.4f));
        juce::ColourGradient bezelFill (theme->metalLight, bezel.getX(), bezel.getY(),
                                         theme->metalDark, bezel.getRight(), bezel.getBottom(), false);
        bezelFill.addColour (0.5, theme->metalMid);
        g.setGradientFill (bezelFill);
        g.fillEllipse (bezel);

        // Cap, painted in the fitted finish so the switch reads as a colour
        // swatch as well as a control.
        const float capRadius = bezelRadius * 0.78f;
        const auto cap = juce::Rectangle<float> (capRadius * 2.0f, capRadius * 2.0f).withCentre (centre);
        g.setGradientFill ({ theme->paintTop, cap.getCentreX(), cap.getY(),
                             theme->paintBottom, cap.getCentreX(), cap.getBottom(), false });
        g.fillEllipse (cap);
        g.setColour (juce::Colours::black.withAlpha (isMouseOverButton ? 0.35f : 0.55f));
        g.drawEllipse (cap, 0.8f);

        // Pointer standing at the fitted finish's detent.
        {
            juce::Graphics::ScopedSaveState save (g);
            g.addTransform (juce::AffineTransform::rotation (detentAngle (index), centre.x, centre.y));
            const juce::Rectangle<float> pointer (centre.x - 1.0f, centre.y - capRadius * 0.92f, 2.0f, capRadius * 0.92f);
            g.setColour (theme->textLight);
            g.fillRoundedRectangle (pointer, 1.0f);
        }

        // Gloss.
        const juce::Rectangle<float> gloss (centre.x - capRadius * 0.55f, centre.y - capRadius * 0.72f,
                                             capRadius * 0.9f, capRadius * 0.5f);
        g.setGradientFill (RetroDraw::radial (juce::Colours::white.withAlpha (0.22f), juce::Colours::transparentWhite,
                                               gloss.getCentre(), capRadius * 0.6f));
        g.fillEllipse (gloss);
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

/** Delete drawn as a small trash-bin icon (lid + handle + ridged body) —
    same reasoning as UndoRedoButton above: a real vector icon instead of
    text, matching the rest of the transport row. */
class DeleteIconButton : public juce::Button
{
public:
    DeleteIconButton() : juce::Button ({}) {}

    const GearPalette::Theme* theme = &GearPalette::defaultTheme();
    void setTheme (const GearPalette::Theme& t) { theme = &t; repaint(); }

    void paintButton (juce::Graphics& g, bool /*isMouseOverButton*/, bool isButtonDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (6.0f);
        const juce::Colour colour = ! isEnabled() ? theme->metalDark.withAlpha (0.6f)
                                                    : (isButtonDown ? theme->accent.brighter (0.3f) : theme->accent);
        g.setColour (colour);

        const float w = bounds.getWidth();
        const float lidY = bounds.getY() + w * 0.22f;

        // Handle, sitting on top of the lid.
        juce::Path handle;
        handle.addRoundedRectangle (bounds.getCentreX() - w * 0.16f, bounds.getY(), w * 0.32f, w * 0.22f, w * 0.06f);
        g.strokePath (handle, juce::PathStrokeType (1.3f));

        // Lid.
        g.drawLine (bounds.getX(), lidY, bounds.getRight(), lidY, 1.6f);

        // Body - a simple tapered bin outline.
        const float inset = w * 0.1f;
        juce::Path body;
        body.startNewSubPath (bounds.getX() + inset, lidY + 2.0f);
        body.lineTo (bounds.getRight() - inset, lidY + 2.0f);
        body.lineTo (bounds.getRight() - inset * 1.7f, bounds.getBottom());
        body.lineTo (bounds.getX() + inset * 1.7f, bounds.getBottom());
        body.closeSubPath();
        g.strokePath (body, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Three vertical ridges inside the body.
        for (int i = -1; i <= 1; ++i)
        {
            const float x = bounds.getCentreX() + (float) i * w * 0.2f;
            g.drawLine (x, lidY + 5.0f, x, bounds.getBottom() - 3.0f, 1.1f);
        }
    }
};

/** The engraved nameplate riveted to the bottom of the panel: model,
    what the box does, and which build you're looking at. */
class ModelPlate : public juce::Component
{
public:
    ModelPlate() { setInterceptsMouseClicks (false, false); }

    void setTheme (const GearPalette::Theme& t) { theme = &t; repaint(); }
    void setText (const juce::String& newText) { text = newText; repaint(); }

    // The width this plate wants for its text — the editor centres it in
    // the footer margin at exactly this width rather than stretching it
    // across the panel, since a real nameplate is only as big as its
    // lettering.
    int getPreferredWidth() const
    {
        return juce::GlyphArrangement::getStringWidthInt (plateFont(), text) + 22;
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);

        g.setGradientFill ({ theme->metalLight.withAlpha (0.85f), bounds.getCentreX(), bounds.getY(),
                             theme->metalMid.withAlpha (0.7f), bounds.getCentreX(), bounds.getBottom(), false });
        g.fillRoundedRectangle (bounds, 2.0f);
        g.setColour (theme->metalDark.withAlpha (0.7f));
        g.drawRoundedRectangle (bounds, 2.0f, 0.8f);

        // Stamped lettering: dark, with the light catching the lower edge
        // of each stroke.
        g.setFont (plateFont());
        g.setColour (theme->metalLight.withAlpha (0.35f));
        g.drawFittedText (text, getLocalBounds().translated (0, 1), juce::Justification::centred, 1);
        g.setColour (theme->metalDark.withAlpha (0.9f));
        g.drawFittedText (text, getLocalBounds(), juce::Justification::centred, 1);
    }

private:
    static juce::Font plateFont()
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 9.5f, juce::Font::bold))
                   .withExtraKerningFactor (0.14f);
    }

    const GearPalette::Theme* theme = &GearPalette::defaultTheme();
    juce::String text;
};

/** A rotary slider with a silkscreened caption above it and a glass
    numeric readout below — the plugin's whole UI is a handful of these
    plus the section plates they sit on. */
class LabelledKnob : public juce::Component
{
public:
    LabelledKnob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& caption,
                  const juce::String& tooltip)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        // A tight glass chip rather than a full-width box — see
        // RetroLookAndFeel::createSliderTextBox()/drawLabel().
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 17);
        slider.setTooltip (tooltip);
        addAndMakeVisible (slider);

        label.setText (caption, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)).withExtraKerningFactor (0.14f));
        label.setTooltip (tooltip);
        addAndMakeVisible (label);

        // Colours set via setColour() are cached on the component, so a
        // later finish switch has to re-apply them explicitly — see
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

    // Every knob on the panel draws at the same diameter, whatever size
    // slot it was given: Warmth lays its knobs out two per row rather than
    // three, so its slots are wider, and without this cap its knobs would
    // read as a bigger pair of controls than the ones in Shift and Vibrato.
    // The editor works the cap out once per layout — see resized().
    void setKnobDiameterCap (int diameter)
    {
        if (knobDiameterCap == diameter)
            return;
        knobDiameterCap = diameter;
        resized();
    }

    void resized() override
    {
        auto area = getLocalBounds();
        label.setBounds (area.removeFromTop (17));
        area.removeFromTop (2);
        const int width = juce::jmin (area.getWidth(), juce::jmax (knobDiameterCap, 44));
        slider.setBounds (area.withSizeKeepingCentre (width, area.getHeight()));
    }

    // Re-applies every colour this component cached via setColour() —
    // called once from the constructor above (against whatever finish was
    // live at that moment) and again by
    // JJBreezeAudioProcessorEditor::applyTheme() on every switch.
    void applyTheme (const GearPalette::Theme& t)
    {
        theme = &t;
        slider.setColour (juce::Slider::rotarySliderFillColourId, theme->accent);
        slider.setColour (juce::Slider::rotarySliderOutlineColourId, theme->metalMid);
        slider.setColour (juce::Slider::textBoxTextColourId, theme->ledText);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, theme->ledBackground);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxHighlightColourId, theme->accent.withAlpha (0.4f));
        label.setColour (juce::Label::textColourId, theme->textLight.withAlpha (0.92f));
        repaint();
    }

private:
    const GearPalette::Theme* theme = &GearPalette::defaultTheme();
    int knobDiameterCap = 96;
    juce::Slider slider;
    EngravedLabel label;
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
    void setUpToggle (BatSwitchButton& button, const juce::String& tooltip);

    // Where the three section plates sit — computed once and shared by
    // resized() (which lays the controls out inside them) and
    // rebuildPanelImage() (which paints the plates themselves), so the
    // hardware and the controls on it can never drift apart.
    std::array<juce::Rectangle<int>, 3> sectionPlateBounds() const;

    // Everything static about the panel — hammered paint, rack ears,
    // section plates, the riveted base rail — rendered once into an image
    // and blitted by paint(). It is several hundred soft gradient dimples,
    // far too slow to redraw behind every knob movement; nothing in it
    // changes except on a resize or a finish switch, which is exactly when
    // this is called.
    void rebuildPanelImage();

    // Leaves the slot being switched away from holding whatever the user
    // last tweaked (so it isn't lost), makes a first-time target slot start
    // as a copy of the current sound (so the initial switch is silent), then
    // recalls the target slot and updates which A/B button reads as active.
    void switchCompareSlot (int targetSlot);
    void updateCompareButtonColours();
    void updateBypassToggleState();

    // Applies a finish: updates currentTheme, hands it to every child that
    // draws with one, re-applies the colours the rest of the editor's own
    // components cached via setColour(), re-renders the panel image and
    // repaints. Called from the constructor with the finish last chosen on
    // this machine (falling back to the build-time JJ_BREEZE_DEFAULT_THEME)
    // and from the finish selector in the header.
    void applyTheme (const juce::String& themeId, bool remember = false);
    void cycleTheme();

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
    // the host — automation, preset recall — dims/undims its plate too, not
    // just clicks made in this editor), the current program (so picking a
    // preset from the host's own menu updates presetBox too), whether the
    // live patch still matches the selected preset (the "modified"
    // indicator), and undo/redo availability.
    void timerCallback() override;

    // Dims and locks a section's knobs when it is switched off — the
    // plate, its name and its switch stay lit, so the panel keeps its
    // shape and the section can be switched back on.
    void setSectionEnabled (bool on, juce::Label& sectionLabel, JewelLampComponent& lamp,
                             std::initializer_list<LabelledKnob*> knobs);
    // Reads the three section-enabled parameters and applies the above to
    // each section — called on startup, on a finish switch, and whenever
    // timerCallback() sees one of them change.
    void updateSectionEnablement();

    // ComboBox item IDs: factory presets use 1..getNumPrograms() (their
    // host program index + 1); user preset items start here instead, well
    // clear of that range, indexed into JJBreezeAudioProcessor::getUserPresetNames().
    static constexpr int firstUserPresetItemId = 1000;

    JJBreezeAudioProcessor& processorRef;
    RetroLookAndFeel retroLookAndFeel;

    // The finish every themed component below currently draws with —
    // points into GearPalette::allThemes()'s static storage (permanent for
    // the plugin's lifetime), reassigned by applyTheme().
    const GearPalette::Theme* currentTheme = &GearPalette::defaultTheme();

    // See rebuildPanelImage().
    juce::Image panelImage;

    // Needed for any child component's setTooltip() text to actually pop up
    // as a tooltip; owns no visible bounds of its own.
    juce::TooltipWindow tooltipWindow { this, 500 };

    // Header: the finish selector, then the italic serif wordmark. The
    // strapline that used to sit beside it now lives on the nameplate at
    // the bottom of the panel, as it does on the hardware.
    FinishSelectorButton finishSelector;
    EngravedLabel titleLabel;
    ModelPlate modelPlate;

    EngravedLabel shiftSectionLabel;
    EngravedLabel vibratoSectionLabel;
    EngravedLabel warmthSectionLabel;
    JewelLampComponent shiftLamp, vibratoLamp, warmthLamp, powerLamp;

    // Preset picker: factory presets (JJBreezeAudioProcessor's existing
    // program list — previously only reachable through the host's own,
    // often buried, preset menu) plus any user presets on disk, refreshed
    // by refreshPresetBox().
    juce::ComboBox presetBox;
    juce::TextButton saveButton { "SAVE" };
    DeleteIconButton deleteButton;
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
    // updateBypassToggleState().
    BatSwitchButton bypassButton;
    EngravedLabel bypassCaptionLabel;

    // A/B compare — see JJBreezeAudioProcessor::storeCompareSnapshot/recallCompareSnapshot.
    juce::TextButton compareAButton { "A" }, compareBButton { "B" };

    BatSwitchButton shiftToggle, vibratoToggle, warmthToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> shiftToggleAttachment,
        vibratoToggleAttachment, warmthToggleAttachment;
    bool shiftWasOn = true, vibratoWasOn = false, warmthWasOn = false;

    LabelledKnob pitchLKnob, pitchRKnob, delayLKnob, delayRKnob, focusKnob, mixKnob;
    LabelledKnob vibratoRateKnob, vibratoDepthKnob, vibratoMixKnob;
    LabelledKnob warmthToneKnob, warmthDriveKnob, warmthBodyKnob, warmthMixKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JJBreezeAudioProcessorEditor)
};
