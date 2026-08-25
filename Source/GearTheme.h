#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** The panel's colourways, and the hardware-drawing primitives every part
    of the UI is built from — ported from the AUv3 sibling
    (../jj-breeze-auv3, JJBreezeExtension/UI/GearTheme.swift and
    RackPanel.swift) so both builds are recognisably the same piece of gear.

    Everything here is drawn procedurally rather than from bitmaps, so the
    panel stays sharp at whatever size the host resizes the editor to, and
    every primitive takes the colourway it should paint with as an argument
    rather than reading a global.
*/
namespace GearPalette
{
    // One complete panel colourway: the steel chassis the plate is bolted
    // into, the painted front plate and its screwed-on sub-plates, the
    // chrome hardware (knob collars, bat switches, screws), the bakelite
    // knob body, the silkscreen lettering, the glass readout windows and
    // the jewel lamps. Everything that draws itself (RetroLookAndFeel,
    // BatSwitchButton, the editor's own paint()) is handed a `const Theme&`
    // rather than reading fixed colours, so the whole plugin can be
    // reskinned live.
    struct Theme
    {
        juce::String id, displayName;

        // Steel chassis: the rack ears and the base rail.
        juce::Colour chassisTop, chassisBottom;

        // The painted front plate, and the sub-plates screwed onto it.
        juce::Colour paintTop, paintBottom;
        juce::Colour panelFill, panelEdgeLight, panelEdgeDark;

        // Chrome / nickel hardware.
        juce::Colour metalLight, metalMid, metalDark;

        // Knob body.
        juce::Colour bakeliteLight, bakeliteDark;

        // Pointer/arc accent and its switched-off version.
        juce::Colour accent, accentDim;

        // Silkscreen lettering.
        juce::Colour textLight, textMuted;

        // Glass readout windows.
        juce::Colour ledBackground, ledText;

        // Jewel lamps.
        juce::Colour lampRed, lampRedDim;
    };

    // The two finishes the panel ships in — same drawing code throughout,
    // just a different palette. Menu/detent order == this order.
    inline const std::array<Theme, 2>& allThemes()
    {
        static const std::array<Theme, 2> themes { {
            // 1960s-70s military-green outboard gear: hammered enamel over
            // steel, cream silkscreen, black bakelite knobs, amber lamps.
            { "green", "Field Green",
              juce::Colour (0xff2b2c28), juce::Colour (0xff121311),
              juce::Colour (0xff5b6950), juce::Colour (0xff333e2d),
              juce::Colour (0xff3d4936), juce::Colour (0xff7d8a70), juce::Colour (0xff1b2118),
              juce::Colour (0xffe4e6df), juce::Colour (0xff9b9f96), juce::Colour (0xff30332e),
              juce::Colour (0xff4a4c48), juce::Colour (0xff0b0c0b),
              juce::Colour (0xffe8a33d), juce::Colour (0xff7d5a24),
              juce::Colour (0xfff1e9d4), juce::Colour (0xffa9ad95),
              juce::Colour (0xff120d08), juce::Colour (0xffffb24e),
              juce::Colour (0xffff3b2a), juce::Colour (0xff4a120d) },

            // The blue-grey rack finish this build wore before: gunmetal
            // plate, nickel hardware, brass accent.
            { "slate", "Slate",
              juce::Colour (0xff2a2c2e), juce::Colour (0xff141618),
              juce::Colour (0xff51616a), juce::Colour (0xff2c363c),
              juce::Colour (0xff3a464e), juce::Colour (0xff7f8d97), juce::Colour (0xff1a2126),
              juce::Colour (0xffdde1e5), juce::Colour (0xff8f959a), juce::Colour (0xff2a2c2e),
              juce::Colour (0xff474d51), juce::Colour (0xff0c0e10),
              juce::Colour (0xffc9974a), juce::Colour (0xff7a5b2c),
              juce::Colour (0xfff2efe4), juce::Colour (0xffaab0ac),
              juce::Colour (0xff111310), juce::Colour (0xffe0b876),
              juce::Colour (0xffff4436), juce::Colour (0xff451512) },
        } };
        return themes;
    }

    inline const Theme& defaultTheme() { return allThemes()[0]; }

    // Falls back to defaultTheme() for an unrecognised id — e.g. a
    // settings file written by a later version of the plugin naming a
    // finish this build doesn't have — rather than refusing to load.
    inline const Theme& findTheme (const juce::String& id)
    {
        for (auto& t : allThemes())
            if (t.id == id)
                return t;
        return defaultTheme();
    }

    inline int indexOfTheme (const juce::String& id)
    {
        for (int i = 0; i < (int) allThemes().size(); ++i)
            if (allThemes()[(size_t) i].id == id)
                return i;
        return 0;
    }
}

/** The hardware pieces the panel is assembled from: screws, hammered
    enamel, rack ears, sub-plates, glass windows, jewel lamps and the
    riveted base rail. Each one is a free function taking the Graphics it
    should draw into, so the editor can paint them straight onto its
    cached panel image and the components can use the same primitives. */
namespace RetroDraw
{
    // A radial gradient in the shorthand every primitive below wants:
    // `inner` at `centre`, fading to `outer` at `radius`.
    inline juce::ColourGradient radial (juce::Colour inner, juce::Colour outer,
                                        juce::Point<float> centre, float radius)
    {
        return juce::ColourGradient (inner, centre, outer, centre.translated (radius, 0.0f), true);
    }

    /** A slotted steel screw. `angle` varies per screw so a panel doesn't
        look like every screw was driven home by a machine. */
    inline void screw (juce::Graphics& g, const GearPalette::Theme& theme,
                       juce::Point<float> centre, float radius, float angle)
    {
        const auto box = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);

        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (box.translated (0.0f, radius * 0.28f));

        auto head = radial (theme.metalLight, theme.metalDark,
                            centre.translated (-radius * 0.35f, -radius * 0.4f), radius * 1.6f);
        head.addColour (0.5, theme.metalMid);
        g.setGradientFill (head);
        g.fillEllipse (box);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawEllipse (box, 0.8f);

        // The driver slot, turned to this screw's own angle.
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::rotation (angle, centre.x, centre.y));

        const float slotHeight = juce::jmax (1.2f, radius * 0.3f);
        const juce::Rectangle<float> slot (centre.x - radius * 0.78f, centre.y - slotHeight * 0.5f,
                                           radius * 1.56f, slotHeight);
        g.setColour (juce::Colours::black.withAlpha (0.62f));
        g.fillRoundedRectangle (slot, slotHeight * 0.5f);
        g.setColour (theme.metalLight.withAlpha (0.22f));
        g.fillRoundedRectangle (slot.translated (0.0f, slotHeight * 0.55f), slotHeight * 0.5f);
    }

    /** Hammered enamel: overlapping soft light/dark dimples over a flat
        coat, the finish on most 60s-70s rack gear. Deterministic (a fixed
        `seed`) so the texture doesn't crawl between repaints. */
    inline void hammeredPaint (juce::Graphics& g, juce::Rectangle<float> area, int seed, int dimples)
    {
        juce::Random rng (seed);

        for (int i = 0; i < dimples; ++i)
        {
            const float x = area.getX() + rng.nextFloat() * area.getWidth();
            const float y = area.getY() + rng.nextFloat() * area.getHeight();
            const float r = 4.0f + rng.nextFloat() * 6.0f;
            const bool lit = rng.nextBool();
            const float strength = 0.025f + rng.nextFloat() * 0.04f;

            const auto tint = (lit ? juce::Colours::white : juce::Colours::black).withAlpha (strength);
            g.setGradientFill (radial (tint, tint.withAlpha (0.0f),
                                       { x - r * 0.3f, y - r * 0.35f }, r));
            g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
        }
    }

    /** The front plate: hammered enamel, a fine roller grain, and a
        vignette because the plate is lit from the front centre. */
    inline void chassisPaint (juce::Graphics& g, const GearPalette::Theme& theme, juce::Rectangle<float> area)
    {
        g.setGradientFill ({ theme.paintTop, area.getCentreX(), area.getY(),
                             theme.paintBottom, area.getCentreX(), area.getBottom(), false });
        g.fillRect (area);

        hammeredPaint (g, area, 0xBEE2E,
                       juce::jmin (900, (int) (area.getWidth() * area.getHeight() / 700.0f)));

        juce::Random grain (12345);
        for (float y = area.getY(); y < area.getBottom(); y += 3.0f)
        {
            g.setColour (juce::Colours::white.withAlpha (grain.nextFloat() * 0.022f));
            g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
        }

        const float endRadius = juce::jmax (area.getWidth(), area.getHeight()) * 0.78f;
        const float startRadius = juce::jmin (area.getWidth(), area.getHeight()) * 0.28f;
        auto vignette = radial (juce::Colours::transparentBlack, juce::Colours::black.withAlpha (0.42f),
                                { area.getCentreX(), area.getY() + area.getHeight() * 0.34f }, endRadius);
        vignette.addColour (juce::jlimit (0.01, 0.99, (double) (startRadius / endRadius)),
                            juce::Colours::transparentBlack);
        g.setGradientFill (vignette);
        g.fillRect (area);
    }

    /** A rack-mount ear: the dark steel side rail the plate is bolted to,
        with two mounting screws. */
    inline void rackEar (juce::Graphics& g, const GearPalette::Theme& theme, juce::Rectangle<float> area)
    {
        g.setGradientFill ({ theme.chassisTop, area.getX(), area.getY(),
                             theme.chassisBottom, area.getRight(), area.getBottom(), false });
        g.fillRect (area);

        // Rolled edge catching the light.
        g.setColour (theme.metalMid.withAlpha (0.35f));
        g.drawVerticalLine ((int) area.getX() + 1, area.getY(), area.getBottom());

        const float radius = area.getWidth() * 0.2f;
        screw (g, theme, { area.getCentreX(), area.getY() + area.getHeight() * 0.16f }, radius, 0.6f);
        screw (g, theme, { area.getCentreX(), area.getY() + area.getHeight() * 0.84f }, radius, -0.9f);
    }

    /** A sub-plate screwed onto the front panel — each effect section sits
        on one, the way 70s consoles bolted a separate engraved plate over
        every module. */
    inline void panelPlate (juce::Graphics& g, const GearPalette::Theme& theme, juce::Rectangle<float> area,
                            float cornerRadius = 6.0f, float screwInset = 11.0f, float screwRadius = 4.0f)
    {
        const auto plate = area.reduced (0.5f);

        g.setColour (juce::Colours::black.withAlpha (0.38f));
        g.fillRoundedRectangle (plate.translated (0.0f, 2.5f), cornerRadius);

        g.setGradientFill ({ theme.panelFill.withAlpha (0.98f), plate.getCentreX(), plate.getY(),
                             theme.panelEdgeDark.withAlpha (0.92f), plate.getCentreX(), plate.getBottom(), false });
        g.fillRoundedRectangle (plate, cornerRadius);

        {
            juce::Path clip;
            clip.addRoundedRectangle (plate, cornerRadius);
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (clip);
            hammeredPaint (g, plate, 0x51DE91A,
                           juce::jmin (420, (int) (plate.getWidth() * plate.getHeight() / 900.0f)));
        }

        // Bevel: a dark scribe line all round, and a bright top lip.
        g.setColour (theme.panelEdgeDark);
        g.drawRoundedRectangle (plate, cornerRadius, 1.0f);
        g.setGradientFill ({ theme.panelEdgeLight.withAlpha (0.55f), plate.getCentreX(), plate.getY(),
                             juce::Colours::transparentBlack, plate.getCentreX(), plate.getCentreY(), false });
        g.drawRoundedRectangle (plate.reduced (1.0f), cornerRadius - 1.0f, 1.0f);

        juce::Random rng (0x5C4E4);
        for (auto corner : { juce::Point<float> (plate.getX() + screwInset,     plate.getY() + screwInset),
                             juce::Point<float> (plate.getRight() - screwInset, plate.getY() + screwInset),
                             juce::Point<float> (plate.getX() + screwInset,     plate.getBottom() - screwInset),
                             juce::Point<float> (plate.getRight() - screwInset, plate.getBottom() - screwInset) })
            screw (g, theme, corner, screwRadius, (rng.nextFloat() * 2.4f - 1.2f));
    }

    /** The steel base rail along the bottom edge, riveted to the plate. */
    inline void footerRivetStrip (juce::Graphics& g, const GearPalette::Theme& theme,
                                  juce::Rectangle<float> area, int rivetCount)
    {
        g.setGradientFill ({ theme.chassisTop, area.getCentreX(), area.getY(),
                             theme.chassisBottom, area.getCentreX(), area.getBottom(), false });
        g.fillRect (area);

        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawHorizontalLine ((int) area.getY(), area.getX(), area.getRight());

        const float step = area.getWidth() / (float) (rivetCount + 1);
        const float radius = juce::jmin (2.6f, area.getHeight() * 0.18f);
        for (int i = 1; i <= rivetCount; ++i)
        {
            const juce::Point<float> centre (area.getX() + step * (float) i, area.getCentreY());
            g.setGradientFill (radial (theme.metalLight.withAlpha (0.8f), theme.metalDark,
                                       centre.translated (-radius * 0.3f, -radius * 0.4f), radius * 1.5f));
            g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
        }
    }

    /** A recessed readout window: smoked glass in a chrome bezel, with the
        light falling into it from above. Sits behind every numeric
        readout, the preset display and the nameplate's neighbours. */
    inline void ledWindow (juce::Graphics& g, const GearPalette::Theme& theme,
                           juce::Rectangle<float> area, float cornerRadius = 3.0f)
    {
        g.setColour (theme.ledBackground);
        g.fillRoundedRectangle (area, cornerRadius);

        juce::ColourGradient recess (juce::Colours::black.withAlpha (0.65f), area.getCentreX(), area.getY(),
                                     juce::Colours::white.withAlpha (0.05f), area.getCentreX(), area.getBottom(), false);
        recess.addColour (0.5, juce::Colours::transparentBlack);
        g.setGradientFill (recess);
        g.fillRoundedRectangle (area, cornerRadius);

        g.setGradientFill ({ theme.metalDark, area.getCentreX(), area.getY(),
                             theme.metalMid.withAlpha (0.75f), area.getCentreX(), area.getBottom(), false });
        g.drawRoundedRectangle (area.reduced (0.5f), cornerRadius, 1.0f);
    }

    /** A jewel indicator lamp in a chrome bezel. */
    inline void jewelLamp (juce::Graphics& g, const GearPalette::Theme& theme,
                           juce::Rectangle<float> area, bool isOn, juce::Colour colour)
    {
        const float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
        const auto centre = area.getCentre();
        const auto bezel = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);

        g.setGradientFill (radial (theme.metalMid, theme.metalDark,
                                   centre.translated (-radius * 0.4f, -radius * 0.5f), radius * 1.4f));
        g.fillEllipse (bezel);

        const float glassRadius = radius * 0.72f;
        const auto glass = juce::Rectangle<float> (glassRadius * 2.0f, glassRadius * 2.0f).withCentre (centre);

        if (isOn)
        {
            g.setGradientFill (radial (colour.withAlpha (0.45f), colour.withAlpha (0.0f), centre, glassRadius * 2.0f));
            g.fillEllipse (glass.expanded (glassRadius * 0.9f));
        }

        auto glassFill = isOn ? radial (juce::Colours::white.withAlpha (0.9f), colour.withAlpha (0.85f),
                                        centre.translated (-glassRadius * 0.3f, -glassRadius * 0.35f), glassRadius * 1.5f)
                              : radial (theme.lampRedDim, juce::Colours::black.withAlpha (0.9f),
                                        centre.translated (-glassRadius * 0.3f, -glassRadius * 0.35f), glassRadius * 1.5f);
        if (isOn)
            glassFill.addColour (0.5, colour);
        g.setGradientFill (glassFill);
        g.fillEllipse (glass);

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (glass, 0.8f);
    }

    /** A chrome bat switch: a mounting nut sunk into the panel and a
        tapered nickel lever thrown up (on) or down (off), like the toggles
        on the reference gear the design is drawn from. */
    inline void batSwitch (juce::Graphics& g, const GearPalette::Theme& theme,
                           juce::Rectangle<float> area, bool isOn)
    {
        const float unit = juce::jmin (area.getWidth(), area.getHeight() * 0.5f);
        const auto pivot = area.getCentre();

        // Escutcheon: the dark plate the switch body is mounted through, so
        // the lever sits on hardware rather than straight on the paint.
        const juce::Rectangle<float> escutcheon (unit * 0.95f, unit * 0.62f);
        g.setGradientFill (radial (theme.metalDark, juce::Colours::black.withAlpha (0.85f),
                                   pivot, unit * 0.665f));
        g.fillEllipse (escutcheon.withCentre (pivot));

        // Lever: a tapered nickel blade thrown to the top or the bottom.
        const float length = area.getHeight() * 0.42f;
        const float tipY = isOn ? pivot.y - length : pivot.y + length;
        const float baseHalf = unit * 0.17f, tipHalf = unit * 0.115f;

        juce::Path blade;
        blade.startNewSubPath (pivot.x - baseHalf, pivot.y);
        blade.lineTo (pivot.x + baseHalf, pivot.y);
        blade.lineTo (pivot.x + tipHalf, tipY);
        blade.lineTo (pivot.x - tipHalf, tipY);
        blade.closeSubPath();

        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillPath (blade, juce::AffineTransform::translation (unit * 0.1f, 0.0f));

        juce::ColourGradient bladeFill (juce::Colours::black.withAlpha (0.85f), pivot.x - baseHalf, pivot.y,
                                        juce::Colours::black.withAlpha (0.8f), pivot.x + baseHalf, pivot.y, false);
        bladeFill.addColour (0.35, theme.metalLight);
        bladeFill.addColour (0.7, theme.metalMid);
        g.setGradientFill (bladeFill);
        g.fillPath (blade);

        // Ball tip.
        const float ballRadius = unit * 0.18f;
        const auto ball = juce::Rectangle<float> (ballRadius * 2.0f, ballRadius * 2.0f)
                              .withCentre ({ pivot.x, tipY });
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillEllipse (ball.translated (ballRadius * 0.25f, ballRadius * 0.15f));
        auto ballFill = radial (theme.metalLight, theme.metalDark,
                                ball.getCentre().translated (-ballRadius * 0.35f, -ballRadius * 0.4f),
                                ballRadius * 1.6f);
        ballFill.addColour (0.5, theme.metalMid);
        g.setGradientFill (ballFill);
        g.fillEllipse (ball);

        // Hex mounting nut clamping the body to the panel.
        const float nutRadius = unit * 0.27f;
        juce::Path nut;
        for (int i = 0; i < 6; ++i)
        {
            const float a = (float) i / 6.0f * juce::MathConstants<float>::twoPi + juce::MathConstants<float>::pi / 6.0f;
            const juce::Point<float> p (pivot.x + std::cos (a) * nutRadius,
                                        pivot.y + std::sin (a) * nutRadius * 0.8f);
            if (i == 0) nut.startNewSubPath (p); else nut.lineTo (p);
        }
        nut.closeSubPath();
        g.setGradientFill ({ theme.metalMid, pivot.x, pivot.y - nutRadius,
                             theme.metalDark, pivot.x, pivot.y + nutRadius, false });
        g.fillPath (nut);
        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.strokePath (nut, juce::PathStrokeType (0.8f));
    }

    /** Silkscreen lettering: the text with a hard one-pixel shadow under
        it, the way paint sits proud of the plate. */
    inline void engravedText (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                              const juce::Font& font, juce::Justification justification, juce::Colour colour)
    {
        g.setFont (font);
        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.drawFittedText (text, area.translated (0, 1), justification, 1);
        g.setColour (colour);
        g.drawFittedText (text, area, justification, 1);
    }
}
