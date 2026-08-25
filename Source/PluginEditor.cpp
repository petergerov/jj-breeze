#include "PluginEditor.h"
#include "BinaryData.h"

using namespace GearPalette;

JJBreezeAudioProcessorEditor::JJBreezeAudioProcessorEditor (JJBreezeAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      vuMeter (p.outputLevel),
      pitchLKnob (p.apvts, ParamIDs::pitchL, "PITCH L"),
      pitchRKnob (p.apvts, ParamIDs::pitchR, "PITCH R"),
      delayLKnob (p.apvts, ParamIDs::delayL, "DELAY L"),
      delayRKnob (p.apvts, ParamIDs::delayR, "DELAY R"),
      focusKnob  (p.apvts, ParamIDs::focus,  "FOCUS"),
      mixKnob    (p.apvts, ParamIDs::mix,    "MIX"),
      slapTimeKnob     (p.apvts, ParamIDs::slapTime,     "TIME"),
      slapFeedbackKnob (p.apvts, ParamIDs::slapFeedback, "FEEDBACK"),
      slapMixKnob      (p.apvts, ParamIDs::slapMix,      "MIX"),
      vibratoRateKnob  (p.apvts, ParamIDs::vibratoRate,  "RATE"),
      vibratoDepthKnob (p.apvts, ParamIDs::vibratoDepth, "DEPTH"),
      vibratoMixKnob   (p.apvts, ParamIDs::vibratoMix,   "MIX")
{
    setLookAndFeel (&retroLookAndFeel);

    portholeImage = juce::ImageCache::getFromMemory (BinaryData::vinyl_png, BinaryData::vinyl_pngSize);

    titleLabel.setText ("J.J. BREEZE", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions ("Avenir Next Condensed", 24.0f, juce::Font::bold))
                             .withExtraKerningFactor (0.05f));
    titleLabel.setColour (juce::Label::textColourId, textLight);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("STEREO WIDENER \xc2\xb7 SLAPBACK ECHO \xc2\xb7 VIBRATO", juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font (juce::FontOptions ("Menlo", 10.5f, juce::Font::plain)).withExtraKerningFactor (0.04f));
    subtitleLabel.setColour (juce::Label::textColourId, textMuted);
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (subtitleLabel);

    addAndMakeVisible (vuMeter);

    addAndMakeVisible (pitchLKnob);
    addAndMakeVisible (pitchRKnob);
    addAndMakeVisible (delayLKnob);
    addAndMakeVisible (delayRKnob);
    addAndMakeVisible (focusKnob);
    addAndMakeVisible (mixKnob);

    setUpSectionLabel (shiftSectionLabel, "SHIFT");
    setUpSectionLabel (slapSectionLabel, "SLAPBACK");
    addAndMakeVisible (slapTimeKnob);
    addAndMakeVisible (slapFeedbackKnob);
    addAndMakeVisible (slapMixKnob);

    setUpSectionLabel (vibratoSectionLabel, "VIBRATO");
    addAndMakeVisible (vibratoRateKnob);
    addAndMakeVisible (vibratoDepthKnob);
    addAndMakeVisible (vibratoMixKnob);

    // Lit IN/OUT switches for each section — flipping one off both bypasses
    // that section's contribution to the sound and collapses its knob row
    // (in resized()) so a disabled section can't confuse the user into
    // thinking its knobs still matter.
    setUpToggle (shiftToggle);
    setUpToggle (slapToggle);
    setUpToggle (vibratoToggle);
    shiftToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, ParamIDs::shiftOn, shiftToggle);
    slapToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, ParamIDs::slapOn, slapToggle);
    vibratoToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, ParamIDs::vibratoOn, vibratoToggle);

    // A toggle can also change from host automation or preset recall, not
    // just a click here — poll and relayout so the collapse always matches.
    startTimerHz (15);

    setResizable (false, false);
    setSize (480, 720);
}

JJBreezeAudioProcessorEditor::~JJBreezeAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void JJBreezeAudioProcessorEditor::setUpSectionLabel (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)).withExtraKerningFactor (0.14f));
    label.setColour (juce::Label::textColourId, accent);
    label.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (label);
}

void JJBreezeAudioProcessorEditor::setUpToggle (LedToggleButton& button)
{
    addAndMakeVisible (button);
}

void JJBreezeAudioProcessorEditor::timerCallback()
{
    const bool shiftOn   = processorRef.apvts.getRawParameterValue (ParamIDs::shiftOn)->load()   > 0.5f;
    const bool slapOn    = processorRef.apvts.getRawParameterValue (ParamIDs::slapOn)->load()    > 0.5f;
    const bool vibratoOn = processorRef.apvts.getRawParameterValue (ParamIDs::vibratoOn)->load() > 0.5f;

    if (shiftOn != shiftWasOn || slapOn != slapWasOn || vibratoOn != vibratoWasOn)
    {
        shiftWasOn = shiftOn;
        slapWasOn = slapOn;
        vibratoWasOn = vibratoOn;
        resized();
        repaint();
    }
}

void JJBreezeAudioProcessorEditor::drawScrew (juce::Graphics& g, juce::Point<float> centre) const
{
    constexpr float r = 5.0f;
    const auto bounds = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre);
    juce::ColourGradient grad (metalLight, bounds.getX(), bounds.getY(),
                                metalDark, bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (bounds);
    g.setColour (chassisBottom.withAlpha (0.8f));
    g.drawEllipse (bounds, 1.0f);
    g.drawLine ({ centre.translated (-3.2f, 1.6f), centre.translated (3.2f, -1.6f) }, 1.2f);
}

// Shared with resized() so panels, rules and knob rows all land in the same place.
static constexpr int headerHeight = 60;
static constexpr int meterStripHeight = 110;
static constexpr int outerPadding = 20; // horizontal margin
static constexpr int topBottomPadding = 12;
static constexpr int sectionLabelHeight = 22;
static constexpr int toggleWidth = 34;
static constexpr int numSectionLabels = 3;    // Shift, Slapback, Vibrato each get one

void JJBreezeAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Dark brushed-metal chassis, lit from the top like a rack unit under
    // studio lighting.
    juce::ColourGradient backdrop (chassisTop, bounds.getCentreX(), bounds.getY(),
                                    chassisBottom, bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill (backdrop);
    g.fillRect (bounds);

    // Faint brushed-aluminium grain.
    juce::Random grain (12345);
    for (float ly = bounds.getY(); ly < bounds.getBottom(); ly += 3.0f)
    {
        g.setColour (juce::Colours::white.withAlpha (grain.nextFloat() * 0.035f));
        g.drawHorizontalLine ((int) ly, 0.0f, bounds.getWidth());
    }

    // Header strip with a machined seam underneath (a light line over a
    // dark one, like a panel edge catching the light).
    auto header = bounds.removeFromTop ((float) headerHeight);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRect (header);
    g.setColour (chassisBottom);
    g.drawHorizontalLine ((int) header.getBottom(), 0.0f, bounds.getWidth());
    g.setColour (metalLight.withAlpha (0.15f));
    g.drawHorizontalLine ((int) header.getBottom() - 1, 0.0f, bounds.getWidth());

    // Power LED next to the title.
    const auto ledCentre = juce::Point<float> (24.0f, header.getCentreY() - 2.0f);
    g.setColour (accent.withAlpha (0.25f));
    g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (ledCentre));
    g.setColour (accent);
    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (ledCentre));

    // The meter strip — a recessed metal card holding the VU meter and the
    // porthole, styled after the metering section on an LA-2A/1176-style
    // hardware compressor.
    {
        auto mp = meterPanelBounds.toFloat();
        g.setColour (chassisBottom.withAlpha (0.6f));
        g.fillRoundedRectangle (mp.translated (0.0f, 2.0f), 8.0f);
        g.setColour (panelFill);
        g.fillRoundedRectangle (mp, 8.0f);
        g.setColour (chassisBottom.withAlpha (0.9f));
        g.drawRoundedRectangle (mp, 8.0f, 1.0f);
        g.setColour (metalLight.withAlpha (0.1f));
        g.drawLine (mp.getX() + 10.0f, mp.getY() + 1.0f, mp.getRight() - 10.0f, mp.getY() + 1.0f, 1.0f);
    }

    // The porthole — a metal-bezelled viewport onto vinyl.png, like a small
    // maker's plate riveted onto a vintage amp, dimmed to match the chassis.
    if (portholeImage.isValid())
    {
        auto porthole = portholeBounds.toFloat();
        auto bezel = porthole.expanded (3.0f);

        juce::ColourGradient bezelGrad (metalLight, bezel.getX(), bezel.getY(),
                                         metalDark, bezel.getRight(), bezel.getBottom(), false);
        g.setGradientFill (bezelGrad);
        g.fillEllipse (bezel);

        juce::Path circle;
        circle.addEllipse (porthole);
        g.saveState();
        g.reduceClipRegion (circle);
        g.drawImage (portholeImage, porthole, juce::RectanglePlacement::fillDestination);
        g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, porthole.getCentre(),
                                                   chassisBottom.withAlpha (0.55f), porthole.getBottomRight(), true));
        g.fillEllipse (porthole);
        g.restoreState();

        g.setColour (chassisBottom);
        g.drawEllipse (porthole, 1.2f);
    }

    // Section panels — recessed metal cards that visually group each knob
    // row (or, when the section's toggle is off, just its collapsed
    // header bar).
    auto drawSection = [&] (const juce::Label& label, const juce::Rectangle<int>& panelBounds)
    {
        auto pf = panelBounds.toFloat();
        g.setColour (chassisBottom.withAlpha (0.6f));
        g.fillRoundedRectangle (pf.translated (0.0f, 2.0f), 8.0f);
        g.setColour (panelFill);
        g.fillRoundedRectangle (pf, 8.0f);
        g.setColour (chassisBottom.withAlpha (0.9f));
        g.drawRoundedRectangle (pf, 8.0f, 1.0f);
        g.setColour (metalLight.withAlpha (0.1f));
        g.drawLine (pf.getX() + 10.0f, pf.getY() + 1.0f, pf.getRight() - 10.0f, pf.getY() + 1.0f, 1.0f);

        g.setColour (accent.withAlpha (0.5f));
        g.drawHorizontalLine (label.getBottom() - 3, (float) label.getX(), (float) panelBounds.getRight());
    };

    drawSection (shiftSectionLabel, shiftPanelBounds);
    drawSection (slapSectionLabel, slapPanelBounds);
    drawSection (vibratoSectionLabel, vibratoPanelBounds);

    // Mounting screws at the four corners of the chassis.
    const float inset = 14.0f;
    const auto full = getLocalBounds().toFloat();
    drawScrew (g, { inset, inset });
    drawScrew (g, { full.getRight() - inset, inset });
    drawScrew (g, { inset, full.getBottom() - inset });
    drawScrew (g, { full.getRight() - inset, full.getBottom() - inset });
}

void JJBreezeAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    auto header = area.removeFromTop (headerHeight).reduced (18, 8);
    header.removeFromLeft (18); // room for the power LED
    titleLabel.setBounds (header.removeFromTop (26));
    subtitleLabel.setBounds (header);

    area.removeFromTop (6); // gap before the meter strip
    meterPanelBounds = area.removeFromTop (meterStripHeight).reduced (outerPadding, 0);
    auto meterInner = meterPanelBounds.reduced (14, 14);
    constexpr int portholeSize = 62;
    portholeBounds = meterInner.removeFromRight (portholeSize).withSizeKeepingCentre (portholeSize, portholeSize);
    meterInner.removeFromRight (16); // gap between meter and porthole
    vuMeter.setBounds (meterInner);
    area.removeFromTop (10); // gap before the knob sections

    area.reduce (outerPadding, topBottomPadding);

    const bool shiftOn   = processorRef.apvts.getRawParameterValue (ParamIDs::shiftOn)->load()   > 0.5f;
    const bool slapOn    = processorRef.apvts.getRawParameterValue (ParamIDs::slapOn)->load()    > 0.5f;
    const bool vibratoOn = processorRef.apvts.getRawParameterValue (ParamIDs::vibratoOn)->load() > 0.5f;

    const int shiftRows = shiftOn ? 2 : 0;
    const int slapRows = slapOn ? 1 : 0;
    const int vibratoRows = vibratoOn ? 1 : 0;
    const int activeRows = juce::jmax (1, shiftRows + slapRows + vibratoRows);

    // Every section's label bar (with its toggle) always stays visible, so
    // the user can always switch it back on. Only the knob rows collapse —
    // and whatever height that frees up goes to sections still expanded, so
    // e.g. Shift alone gets noticeably bigger knobs when Slap and Vibrato
    // are both off, rather than leaving dead space.
    const int rowHeight = (area.getHeight() - numSectionLabels * sectionLabelHeight) / activeRows;

    // SHIFT — pitch + delay rows (2 rows worth of height when on).
    {
        auto fullLabelRow = area.removeFromTop (sectionLabelHeight);
        auto labelRow = fullLabelRow;
        shiftToggle.setBounds (labelRow.removeFromRight (toggleWidth).reduced (0, 3));
        labelRow.removeFromRight (6);
        shiftSectionLabel.setBounds (labelRow);

        pitchLKnob.setVisible (shiftOn);
        pitchRKnob.setVisible (shiftOn);
        focusKnob.setVisible (shiftOn);
        delayLKnob.setVisible (shiftOn);
        delayRKnob.setVisible (shiftOn);
        mixKnob.setVisible (shiftOn);

        if (shiftOn)
        {
            auto pitchRow = area.removeFromTop (rowHeight);
            auto delayRow = area.removeFromTop (rowHeight);
            shiftPanelBounds = fullLabelRow.getUnion (pitchRow).getUnion (delayRow).expanded (6, 4);

            const int knobWidth = pitchRow.getWidth() / 3;
            pitchLKnob.setBounds (pitchRow.removeFromLeft (knobWidth).reduced (10));
            pitchRKnob.setBounds (pitchRow.removeFromLeft (knobWidth).reduced (10));
            focusKnob.setBounds  (pitchRow.reduced (10));

            delayLKnob.setBounds (delayRow.removeFromLeft (knobWidth).reduced (10));
            delayRKnob.setBounds (delayRow.removeFromLeft (knobWidth).reduced (10));
            mixKnob.setBounds    (delayRow.reduced (10));
        }
        else
        {
            shiftPanelBounds = fullLabelRow.expanded (6, 4);
        }
    }

    // SLAPBACK — one row.
    {
        auto fullLabelRow = area.removeFromTop (sectionLabelHeight);
        auto labelRow = fullLabelRow;
        slapToggle.setBounds (labelRow.removeFromRight (toggleWidth).reduced (0, 3));
        labelRow.removeFromRight (6);
        slapSectionLabel.setBounds (labelRow);

        slapTimeKnob.setVisible (slapOn);
        slapFeedbackKnob.setVisible (slapOn);
        slapMixKnob.setVisible (slapOn);

        if (slapOn)
        {
            auto slapRow = area.removeFromTop (rowHeight);
            slapPanelBounds = fullLabelRow.getUnion (slapRow).expanded (6, 4);

            const int knobWidth = slapRow.getWidth() / 3;
            slapTimeKnob.setBounds     (slapRow.removeFromLeft (knobWidth).reduced (10));
            slapFeedbackKnob.setBounds (slapRow.removeFromLeft (knobWidth).reduced (10));
            slapMixKnob.setBounds      (slapRow.reduced (10));
        }
        else
        {
            slapPanelBounds = fullLabelRow.expanded (6, 4);
        }
    }

    // VIBRATO — one row, takes whatever's left (absorbs any rounding
    // remainder from the integer row-height division above).
    {
        auto fullLabelRow = area.removeFromTop (sectionLabelHeight);
        auto labelRow = fullLabelRow;
        vibratoToggle.setBounds (labelRow.removeFromRight (toggleWidth).reduced (0, 3));
        labelRow.removeFromRight (6);
        vibratoSectionLabel.setBounds (labelRow);

        vibratoRateKnob.setVisible (vibratoOn);
        vibratoDepthKnob.setVisible (vibratoOn);
        vibratoMixKnob.setVisible (vibratoOn);

        if (vibratoOn)
        {
            auto vibratoRow = area;
            vibratoPanelBounds = fullLabelRow.getUnion (vibratoRow).expanded (6, 4);

            const int knobWidth = vibratoRow.getWidth() / 3;
            vibratoRateKnob.setBounds  (vibratoRow.removeFromLeft (knobWidth).reduced (10));
            vibratoDepthKnob.setBounds (vibratoRow.removeFromLeft (knobWidth).reduced (10));
            vibratoMixKnob.setBounds   (vibratoRow.reduced (10));
        }
        else
        {
            vibratoPanelBounds = fullLabelRow.expanded (6, 4);
        }
    }
}
