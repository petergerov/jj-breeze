#include "PluginEditor.h"
#include "BinaryData.h"

using namespace GearPalette;

JJBreezeAudioProcessorEditor::JJBreezeAudioProcessorEditor (JJBreezeAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
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

    setResizable (false, false);
    setSize (480, 640);
}

JJBreezeAudioProcessorEditor::~JJBreezeAudioProcessorEditor()
{
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
static constexpr int headerHeight = 76;
static constexpr int outerPadding = 20; // horizontal margin
static constexpr int topBottomPadding = 12;
static constexpr int sectionLabelHeight = 22;
static constexpr int numRows = 4;             // pitch, delay, slapback, vibrato
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

    // Section panels — recessed metal cards that visually group each knob row.
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

    auto header = area.removeFromTop (headerHeight).reduced (18, 10);
    header.removeFromLeft (18); // room for the power LED
    constexpr int portholeSize = 48;
    portholeBounds = header.removeFromRight (portholeSize).withSizeKeepingCentre (portholeSize, portholeSize);
    header.removeFromRight (14); // gap between title block and porthole

    titleLabel.setBounds (header.removeFromTop (30));
    subtitleLabel.setBounds (header);

    area.reduce (outerPadding, topBottomPadding);

    // A uniform grid — pitch/delay/slapback/vibrato rows, all the same
    // height, so every knob ends up the same size.
    const int rowHeight = (area.getHeight() - numSectionLabels * sectionLabelHeight) / numRows;

    shiftSectionLabel.setBounds (area.removeFromTop (sectionLabelHeight));
    auto pitchRow = area.removeFromTop (rowHeight);
    auto delayRow = area.removeFromTop (rowHeight);
    shiftPanelBounds = pitchRow.getUnion (delayRow).expanded (6, 4);

    slapSectionLabel.setBounds (area.removeFromTop (sectionLabelHeight));
    auto slapRow = area.removeFromTop (rowHeight);
    slapPanelBounds = slapRow.expanded (6, 4);

    vibratoSectionLabel.setBounds (area.removeFromTop (sectionLabelHeight));
    auto vibratoRow = area;
    vibratoPanelBounds = vibratoRow.expanded (6, 4);

    const int knobWidth = pitchRow.getWidth() / 3;

    pitchLKnob.setBounds (pitchRow.removeFromLeft (knobWidth).reduced (10));
    pitchRKnob.setBounds (pitchRow.removeFromLeft (knobWidth).reduced (10));
    focusKnob.setBounds  (pitchRow.reduced (10));

    delayLKnob.setBounds (delayRow.removeFromLeft (knobWidth).reduced (10));
    delayRKnob.setBounds (delayRow.removeFromLeft (knobWidth).reduced (10));
    mixKnob.setBounds    (delayRow.reduced (10));

    slapTimeKnob.setBounds     (slapRow.removeFromLeft (knobWidth).reduced (10));
    slapFeedbackKnob.setBounds (slapRow.removeFromLeft (knobWidth).reduced (10));
    slapMixKnob.setBounds      (slapRow.reduced (10));

    vibratoRateKnob.setBounds  (vibratoRow.removeFromLeft (knobWidth).reduced (10));
    vibratoDepthKnob.setBounds (vibratoRow.removeFromLeft (knobWidth).reduced (10));
    vibratoMixKnob.setBounds   (vibratoRow.reduced (10));
}
