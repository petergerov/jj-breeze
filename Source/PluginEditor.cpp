#include "PluginEditor.h"

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
    titleLabel.setText ("J.J. BREEZE", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("stereo widener + slapback echo + vibrato", juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.5f));
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (subtitleLabel);

    addAndMakeVisible (pitchLKnob);
    addAndMakeVisible (pitchRKnob);
    addAndMakeVisible (delayLKnob);
    addAndMakeVisible (delayRKnob);
    addAndMakeVisible (focusKnob);
    addAndMakeVisible (mixKnob);

    auto setUpSectionLabel = [this] (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.4f));
        label.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (label);
    };

    setUpSectionLabel (slapSectionLabel, "SLAPBACK");
    addAndMakeVisible (slapTimeKnob);
    addAndMakeVisible (slapFeedbackKnob);
    addAndMakeVisible (slapMixKnob);

    setUpSectionLabel (vibratoSectionLabel, "VIBRATO");
    addAndMakeVisible (vibratoRateKnob);
    addAndMakeVisible (vibratoDepthKnob);
    addAndMakeVisible (vibratoMixKnob);

    setResizable (false, false);
    setSize (480, 600);
}

// Shared with resized() so the divider lines land exactly on the row boundaries.
static constexpr int headerHeight = 64;
static constexpr int outerPadding = 20; // matches area.reduce(20, 10) below (horizontal amount)
static constexpr int topBottomPadding = 10;
static constexpr int sectionLabelHeight = 18;
static constexpr int numRows = 4;             // pitch, delay, slapback, vibrato
static constexpr int numSectionLabels = 2;    // Slapback + Vibrato each get one

void JJBreezeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c1f24));

    auto header = getLocalBounds().removeFromTop (headerHeight).toFloat();
    g.setColour (juce::Colour (0xff20242b));
    g.fillRect (header);

    // All four rows (widener x2, slapback x1, vibrato x1) are the same
    // height, so every knob ends up the same size — matches resized().
    const float top = (float) (headerHeight + topBottomPadding);
    const float usableHeight = (float) getHeight() - top - topBottomPadding;
    const float rowHeight = (usableHeight - numSectionLabels * sectionLabelHeight) / (float) numRows;

    const float slapBoundary    = top + rowHeight * 2.0f;
    const float vibratoBoundary = slapBoundary + sectionLabelHeight + rowHeight;

    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colours::white.withAlpha (0.08f));

    // Dividers separate the widener controls from Slapback, and Slapback
    // from Vibrato — each is a deliberately separate, independent effect.
    g.drawHorizontalLine ((int) slapBoundary,    (float) outerPadding, bounds.getWidth() - (float) outerPadding);
    g.drawHorizontalLine ((int) vibratoBoundary, (float) outerPadding, bounds.getWidth() - (float) outerPadding);
}

void JJBreezeAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    auto header = area.removeFromTop (headerHeight).reduced (16, 8);
    titleLabel.setBounds (header.removeFromTop (28));
    subtitleLabel.setBounds (header);

    area.reduce (outerPadding, topBottomPadding);

    // A uniform grid — pitch/delay/slapback/vibrato rows, all the same
    // height, so every knob (including Slapback's and Vibrato's) ends up
    // the same size.
    const int rowHeight = (area.getHeight() - numSectionLabels * sectionLabelHeight) / numRows;

    auto pitchRow = area.removeFromTop (rowHeight);
    auto delayRow = area.removeFromTop (rowHeight);
    slapSectionLabel.setBounds (area.removeFromTop (sectionLabelHeight));
    auto slapRow = area.removeFromTop (rowHeight);
    vibratoSectionLabel.setBounds (area.removeFromTop (sectionLabelHeight));
    auto vibratoRow = area;

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
