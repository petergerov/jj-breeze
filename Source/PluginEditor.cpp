#include "PluginEditor.h"

using namespace GearPalette;

JJBreezeAudioProcessorEditor::JJBreezeAudioProcessorEditor (JJBreezeAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      dropAmountKnob (p.apvts, ParamIDs::dropAmount, "SEMITONES"),
      dropMixKnob    (p.apvts, ParamIDs::dropMix,    "MIX"),
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
      vibratoMixKnob   (p.apvts, ParamIDs::vibratoMix,   "MIX"),
      warmthToneKnob   (p.apvts, ParamIDs::warmthTone,   "TONE"),
      warmthDriveKnob  (p.apvts, ParamIDs::warmthDrive,  "DRIVE"),
      warmthBodyKnob   (p.apvts, ParamIDs::warmthBody,   "BODY"),
      warmthMixKnob    (p.apvts, ParamIDs::warmthMix,    "MIX")
{
    setLookAndFeel (&retroLookAndFeel);

    titleLabel.setText ("J.J. BREEZE", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions ("Avenir Next Condensed", 24.0f, juce::Font::bold))
                             .withExtraKerningFactor (0.05f));
    titleLabel.setColour (juce::Label::textColourId, textLight);
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("PITCH DROP \xc2\xb7 STEREO WIDENER \xc2\xb7 SLAPBACK ECHO \xc2\xb7 VIBRATO \xc2\xb7 WARMTH", juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font (juce::FontOptions ("Menlo", 10.5f, juce::Font::plain)).withExtraKerningFactor (0.04f));
    subtitleLabel.setColour (juce::Label::textColourId, textMuted);
    subtitleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (subtitleLabel);

    setUpSectionLabel (dropSectionLabel, "DROP");
    addAndMakeVisible (dropAmountKnob);
    addAndMakeVisible (dropMixKnob);

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

    setUpSectionLabel (warmthSectionLabel, "WARMTH");
    addAndMakeVisible (warmthToneKnob);
    addAndMakeVisible (warmthDriveKnob);
    addAndMakeVisible (warmthBodyKnob);
    addAndMakeVisible (warmthMixKnob);

    // Lit IN/OUT switches for each section — flipping one off both bypasses
    // that section's contribution to the sound and collapses its knob row
    // (in resized()) so a disabled section can't confuse the user into
    // thinking its knobs still matter.
    setUpToggle (dropToggle);
    setUpToggle (shiftToggle);
    setUpToggle (slapToggle);
    setUpToggle (vibratoToggle);
    setUpToggle (warmthToggle);
    dropToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, ParamIDs::dropOn, dropToggle);
    shiftToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, ParamIDs::shiftOn, shiftToggle);
    slapToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, ParamIDs::slapOn, slapToggle);
    vibratoToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, ParamIDs::vibratoOn, vibratoToggle);
    warmthToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, ParamIDs::warmthOn, warmthToggle);

    // A toggle can also change from host automation or preset recall, not
    // just a click here — poll and relayout so the collapse always matches.
    startTimerHz (15);

    setResizable (false, false);
    setSize (480, 850);
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
    const bool dropOn    = processorRef.apvts.getRawParameterValue (ParamIDs::dropOn)->load()    > 0.5f;
    const bool shiftOn   = processorRef.apvts.getRawParameterValue (ParamIDs::shiftOn)->load()   > 0.5f;
    const bool slapOn    = processorRef.apvts.getRawParameterValue (ParamIDs::slapOn)->load()    > 0.5f;
    const bool vibratoOn = processorRef.apvts.getRawParameterValue (ParamIDs::vibratoOn)->load() > 0.5f;
    const bool warmthOn  = processorRef.apvts.getRawParameterValue (ParamIDs::warmthOn)->load()  > 0.5f;

    if (dropOn != dropWasOn || shiftOn != shiftWasOn || slapOn != slapWasOn
        || vibratoOn != vibratoWasOn || warmthOn != warmthWasOn)
    {
        dropWasOn = dropOn;
        shiftWasOn = shiftOn;
        slapWasOn = slapOn;
        vibratoWasOn = vibratoOn;
        warmthWasOn = warmthOn;
        resized();
        repaint();
    }
}

void JJBreezeAudioProcessorEditor::drawScrew (juce::Graphics& g, juce::Point<float> centre) const
{
    constexpr float r = 8.0f;
    const auto bounds = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre);
    juce::ColourGradient grad (metalLight, bounds.getX(), bounds.getY(),
                                metalDark, bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (bounds);
    g.setColour (chassisBottom.withAlpha (0.8f));
    g.drawEllipse (bounds, 1.4f);
    g.drawLine ({ centre.translated (-5.0f, 2.6f), centre.translated (5.0f, -2.6f) }, 1.8f);
}

// Shared with resized() so panels, rules and knob rows all land in the same place.
static constexpr int headerHeight = 68;
static constexpr int outerPadding = 20; // horizontal margin
static constexpr int topPadding = 12;
static constexpr int bottomPadding = 36; // extra clearance so the bottom corner screws stay visible
static constexpr int sectionLabelHeight = 26;
static constexpr int sectionGap = 14; // vertical gap between one section's card and the next
static constexpr int toggleWidth = 34;
static constexpr int numSectionLabels = 5;    // Drop, Shift, Slapback, Vibrato, Warmth each get one
static constexpr int numSectionGaps = 4;      // gaps: Drop-Shift, Shift-Slapback, Slapback-Vibrato, Vibrato-Warmth

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
        g.drawHorizontalLine (label.getBottom() - 1, (float) label.getX(), (float) panelBounds.getRight());
    };

    drawSection (dropSectionLabel, dropPanelBounds);
    drawSection (shiftSectionLabel, shiftPanelBounds);
    drawSection (slapSectionLabel, slapPanelBounds);
    drawSection (vibratoSectionLabel, vibratoPanelBounds);
    drawSection (warmthSectionLabel, warmthPanelBounds);

    // Mounting screws at the four corners of the chassis.
    const float inset = 17.0f;
    const auto full = getLocalBounds().toFloat();
    drawScrew (g, { inset, inset });
    drawScrew (g, { full.getRight() - inset, inset });
    drawScrew (g, { inset, full.getBottom() - inset });
    drawScrew (g, { full.getRight() - inset, full.getBottom() - inset });
}

void JJBreezeAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // Full header width for the centred title/subtitle — the power LED is
    // drawn separately (in paint()) off to the left and doesn't need room
    // carved out of the text layout.
    auto header = area.removeFromTop (headerHeight).reduced (18, 10);
    titleLabel.setBounds (header.removeFromTop (28));
    subtitleLabel.setBounds (header);

    area.removeFromTop (8); // gap before the knob sections
    area.removeFromLeft (outerPadding);
    area.removeFromRight (outerPadding);
    area.removeFromTop (topPadding);
    area.removeFromBottom (bottomPadding);

    const bool dropOn    = processorRef.apvts.getRawParameterValue (ParamIDs::dropOn)->load()    > 0.5f;
    const bool shiftOn   = processorRef.apvts.getRawParameterValue (ParamIDs::shiftOn)->load()   > 0.5f;
    const bool slapOn    = processorRef.apvts.getRawParameterValue (ParamIDs::slapOn)->load()    > 0.5f;
    const bool vibratoOn = processorRef.apvts.getRawParameterValue (ParamIDs::vibratoOn)->load() > 0.5f;
    const bool warmthOn  = processorRef.apvts.getRawParameterValue (ParamIDs::warmthOn)->load()  > 0.5f;

    const int dropRows = dropOn ? 1 : 0;
    const int shiftRows = shiftOn ? 2 : 0;
    const int slapRows = slapOn ? 1 : 0;
    const int vibratoRows = vibratoOn ? 1 : 0;
    const int warmthRows = warmthOn ? 1 : 0;
    const int activeRows = juce::jmax (1, dropRows + shiftRows + slapRows + vibratoRows + warmthRows);

    // Every section's label bar (with its toggle) always stays visible, so
    // the user can always switch it back on. Only the knob rows collapse —
    // and whatever height that frees up goes to sections still expanded, so
    // e.g. Shift alone gets noticeably bigger knobs when Slap and Vibrato
    // are both off, rather than leaving dead space.
    const int rowHeight = (area.getHeight() - numSectionLabels * sectionLabelHeight - numSectionGaps * sectionGap) / activeRows;

    // DROP — one row, two knobs (Amount, Mix). Runs first in both the UI
    // and the signal chain: a static pitch shift on the voice itself,
    // ahead of Shift's micro-detune widener.
    {
        auto fullLabelRow = area.removeFromTop (sectionLabelHeight);
        auto labelRow = fullLabelRow;
        dropToggle.setBounds (labelRow.removeFromRight (toggleWidth).reduced (0, 5));
        labelRow.removeFromRight (6);
        dropSectionLabel.setBounds (labelRow);

        dropAmountKnob.setVisible (dropOn);
        dropMixKnob.setVisible (dropOn);

        if (dropOn)
        {
            auto dropRow = area.removeFromTop (rowHeight);
            dropPanelBounds = fullLabelRow.getUnion (dropRow).expanded (6, 4);

            const int knobWidth = dropRow.getWidth() / 2;
            dropAmountKnob.setBounds (dropRow.removeFromLeft (knobWidth).reduced (10));
            dropMixKnob.setBounds    (dropRow.reduced (10));
        }
        else
        {
            dropPanelBounds = fullLabelRow.expanded (6, 4);
        }
    }

    area.removeFromTop (sectionGap);

    // SHIFT — pitch + delay rows (2 rows worth of height when on).
    {
        auto fullLabelRow = area.removeFromTop (sectionLabelHeight);
        auto labelRow = fullLabelRow;
        shiftToggle.setBounds (labelRow.removeFromRight (toggleWidth).reduced (0, 5));
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

    area.removeFromTop (sectionGap);

    // SLAPBACK — one row.
    {
        auto fullLabelRow = area.removeFromTop (sectionLabelHeight);
        auto labelRow = fullLabelRow;
        slapToggle.setBounds (labelRow.removeFromRight (toggleWidth).reduced (0, 5));
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

    area.removeFromTop (sectionGap);

    // VIBRATO — one row.
    {
        auto fullLabelRow = area.removeFromTop (sectionLabelHeight);
        auto labelRow = fullLabelRow;
        vibratoToggle.setBounds (labelRow.removeFromRight (toggleWidth).reduced (0, 5));
        labelRow.removeFromRight (6);
        vibratoSectionLabel.setBounds (labelRow);

        vibratoRateKnob.setVisible (vibratoOn);
        vibratoDepthKnob.setVisible (vibratoOn);
        vibratoMixKnob.setVisible (vibratoOn);

        if (vibratoOn)
        {
            auto vibratoRow = area.removeFromTop (rowHeight);
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

    area.removeFromTop (sectionGap);

    // WARMTH — one row, takes whatever's left (absorbs any rounding
    // remainder from the integer row-height division above).
    {
        auto fullLabelRow = area.removeFromTop (sectionLabelHeight);
        auto labelRow = fullLabelRow;
        warmthToggle.setBounds (labelRow.removeFromRight (toggleWidth).reduced (0, 5));
        labelRow.removeFromRight (6);
        warmthSectionLabel.setBounds (labelRow);

        warmthToneKnob.setVisible (warmthOn);
        warmthDriveKnob.setVisible (warmthOn);
        warmthBodyKnob.setVisible (warmthOn);
        warmthMixKnob.setVisible (warmthOn);

        if (warmthOn)
        {
            auto warmthRow = area;
            warmthPanelBounds = fullLabelRow.getUnion (warmthRow).expanded (6, 4);

            const int knobWidth = warmthRow.getWidth() / 4;
            warmthToneKnob.setBounds  (warmthRow.removeFromLeft (knobWidth).reduced (10));
            warmthDriveKnob.setBounds (warmthRow.removeFromLeft (knobWidth).reduced (10));
            warmthBodyKnob.setBounds  (warmthRow.removeFromLeft (knobWidth).reduced (10));
            warmthMixKnob.setBounds   (warmthRow.reduced (10));
        }
        else
        {
            warmthPanelBounds = fullLabelRow.expanded (6, 4);
        }
    }
}
