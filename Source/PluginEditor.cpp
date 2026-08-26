#include "PluginEditor.h"

JJBreezeAudioProcessorEditor::JJBreezeAudioProcessorEditor (JJBreezeAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      pitchLKnob (p.apvts, ParamIDs::pitchL, "PITCH L",
                  "Left-channel pitch shift 1200 ct = 1 octave. A few cents gives classic"
                  "microshift width; larger values (with Focus turned down) give a big pitch-shifted voice."),
      pitchRKnob (p.apvts, ParamIDs::pitchR, "PITCH R",
                  "Right-channel pitch shift, independent of Pitch L - opposite signs widen, matching "
                  "signs shift the whole signal up or down."),
      delayLKnob (p.apvts, ParamIDs::delayL, "DELAY L",
                  "Left-channel delay time (0-250ms) for the modulated delay tap. Short times add "
                  "subtle width; times above ~80ms read as a slapback echo."),
      delayRKnob (p.apvts, ParamIDs::delayR, "DELAY R",
                  "Right-channel delay time, independent of Delay L."),
      focusKnob  (p.apvts, ParamIDs::focus,  "FOCUS",
                  "Crossover point: everything below this frequency stays completely dry. Only the band "
                  "above is fed through the pitch shifter + delay - turning Focus up protects more low "
                  "end from being processed at all, it doesn't just cut lows from the wet signal."),
      mixKnob    (p.apvts, ParamIDs::mix,    "MIX",
                  "Blend between dry and the Shift section's (crossover-recombined) wet signal."),
      vibratoRateKnob  (p.apvts, ParamIDs::vibratoRate,  "RATE",
                        "Speed of the vibrato's pitch-wobble LFO."),
      vibratoDepthKnob (p.apvts, ParamIDs::vibratoDepth, "DEPTH",
                        "How far the swept delay moves - controls the intensity of the pitch wobble."),
      vibratoMixKnob   (p.apvts, ParamIDs::vibratoMix,   "MIX",
                        "Blend between dry and the vibrato-modulated signal. Independent of Shift's Mix - "
                        "the two are summed, not crossfaded against each other."),
      warmthToneKnob   (p.apvts, ParamIDs::warmthTone,   "TONE",
                        "Low-pass cutoff of the final tone stage - lower values roll off more top end "
                        "for a darker, more rolled-off character."),
      warmthDriveKnob  (p.apvts, ParamIDs::warmthDrive,  "DRIVE",
                        "Amount of gentle tanh soft-saturation applied after the low-pass, for a warmer, "
                        "mildly overdriven character."),
      warmthBodyKnob   (p.apvts, ParamIDs::warmthBody,   "BODY",
                        "Fixed 150Hz low-shelf boost - adds low-mid fullness/chest without touching "
                        "pitch or top end."),
      warmthMixKnob    (p.apvts, ParamIDs::warmthMix,    "MIX",
                        "Blend between the unprocessed sum and the Warmth-shaped output.")
{
#ifndef JJ_BREEZE_DEFAULT_THEME
    #define JJ_BREEZE_DEFAULT_THEME "slate" // guards a non-CMake build of this file
#endif

    // Applies the build-time colourway (JJ_BREEZE_DEFAULT_THEME in
    // CMakeLists.txt — not user-switchable at runtime) before anything
    // below sets a single cached colour, so every child is born already
    // themed rather than painted once in GearPalette's compile-time
    // default and corrected a frame later. The pitchLKnob..warmthMixKnob
    // members above were already constructed (in the init list, before
    // this body runs) against that same default — applyTheme() below
    // re-applies to them too.
    applyTheme (JJ_BREEZE_DEFAULT_THEME);

    setLookAndFeel (&retroLookAndFeel);

    // Nameplate: an italic serif wordmark ("j.j.breeze") with a small
    // tracked-caps subtitle sharing its baseline, left-aligned - matches
    // the design mockup's brand plate.
    titleLabel.setText ("j.j.breeze", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions ("Georgia", 22.0f, juce::Font::italic | juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, currentTheme->textLight);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("STEREO MICRO-PITCH WIDENER", juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)).withExtraKerningFactor (0.2f));
    subtitleLabel.setColour (juce::Label::textColourId, currentTheme->textMuted);
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (subtitleLabel);

    versionLabel.setText ("v" JucePlugin_VersionString, juce::dontSendNotification);
    versionLabel.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::plain)).withExtraKerningFactor (0.03f));
    versionLabel.setColour (juce::Label::textColourId, currentTheme->textMuted.withAlpha (0.55f));
    versionLabel.setJustificationType (juce::Justification::centred);
    versionLabel.setTooltip (JucePlugin_Name " " JucePlugin_VersionString);
    addAndMakeVisible (versionLabel);

    // Preset picker: factory presets (JJBreezeAudioProcessor::getPresets())
    // plus user presets saved from this editor - previously only the
    // factory list existed, and only reachable through the host's own
    // preset menu, which in a host like Logic Pro is easy to miss entirely.
    presetBox.setTooltip ("Load a preset - a starting point for the knobs below.");
    presetBox.setTextWhenNothingSelected ("Preset...");
    presetBox.setColour (juce::ComboBox::backgroundColourId, currentTheme->ledBackground);
    presetBox.setColour (juce::ComboBox::textColourId, currentTheme->ledText);
    presetBox.setColour (juce::ComboBox::outlineColourId, currentTheme->metalDark);
    presetBox.setColour (juce::ComboBox::arrowColourId, currentTheme->accent);
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();

        if (id >= firstUserPresetItemId)
        {
            const auto names = processorRef.getUserPresetNames();
            const int idx = id - firstUserPresetItemId;
            if (idx >= 0 && idx < names.size() && processorRef.loadUserPreset (names[idx]))
            {
                activeUserPresetName = names[idx];
                activePresetSnapshot = processorRef.captureNormalizedSnapshot();
                deleteButton.setEnabled (true);
            }
        }
        else if (id >= 1)
        {
            processorRef.setCurrentProgram (id - 1);
            lastKnownProgram = id - 1;
            activeUserPresetName.clear();
            activePresetSnapshot = processorRef.captureNormalizedSnapshot();
            deleteButton.setEnabled (false);
        }
    };
    refreshPresetBox();
    addAndMakeVisible (presetBox);

    saveButton.setTooltip ("Save the current knob settings as a new user preset.");
    saveButton.onClick = [this] { promptAndSaveUserPreset(); };
    addAndMakeVisible (saveButton);

    deleteButton.setTooltip ("Delete the selected user preset. Only enabled for user presets - factory presets can't be deleted.");
    deleteButton.onClick = [this] { promptAndDeleteUserPreset(); };
    addAndMakeVisible (deleteButton);

    // Undo/redo - mainly useful in the Standalone build, which (unlike
    // being hosted in a DAW) has no host-level undo of its own.
    undoButton.setTooltip ("Undo the last change.");
    redoButton.setTooltip ("Redo.");
    undoButton.onClick = [this] { processorRef.undoManager.undo(); };
    redoButton.onClick = [this] { processorRef.undoManager.redo(); };
    addAndMakeVisible (undoButton);
    addAndMakeVisible (redoButton);

    // POWER switch - drives processorRef's bypass flag, but shown the
    // opposite way round from it (on = powered/processing, off = bypassed,
    // the untouched dry signal) since that's what a power switch means.
    // A switch (see LedToggleButton), not a click-toggling button - its
    // drawn state always comes from processorRef.isBypassed() via
    // updateBypassToggleState(), same idea as the section on/off switches
    // but with no apvts parameter of its own to attach to.
    bypassButton.setTooltip ("Power off to bypass all processing (the untouched dry signal) without changing any knob or section state; power back on to resume.");
    bypassButton.setClickingTogglesState (false);
    bypassButton.onClick = [this]
    {
        processorRef.setBypassed (! processorRef.isBypassed());
        updateBypassToggleState();
    };
    addAndMakeVisible (bypassButton);
    updateBypassToggleState();

    bypassCaptionLabel.setText ("POWER", juce::dontSendNotification);
    bypassCaptionLabel.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)).withExtraKerningFactor (0.14f));
    bypassCaptionLabel.setColour (juce::Label::textColourId, currentTheme->accent);
    bypassCaptionLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (bypassCaptionLabel);

    // A/B compare - see JJBreezeAudioProcessor::storeCompareSnapshot/recallCompareSnapshot.
    compareAButton.setTooltip ("Compare slot A. Switching away stores your current tweaks here first.");
    compareBButton.setTooltip ("Compare slot B. Starts as a copy of A the first time you switch to it.");
    compareAButton.setClickingTogglesState (false);
    compareBButton.setClickingTogglesState (false);
    compareAButton.onClick = [this] { switchCompareSlot (0); };
    compareBButton.onClick = [this] { switchCompareSlot (1); };
    addAndMakeVisible (compareAButton);
    addAndMakeVisible (compareBButton);
    updateCompareButtonColours();

    addAndMakeVisible (pitchLKnob);
    addAndMakeVisible (pitchRKnob);
    addAndMakeVisible (delayLKnob);
    addAndMakeVisible (delayRKnob);
    addAndMakeVisible (focusKnob);
    addAndMakeVisible (mixKnob);

    setUpSectionLabel (shiftSectionLabel, "SHIFT");

    setUpSectionLabel (vibratoSectionLabel, "VIBRATO");
    addAndMakeVisible (vibratoRateKnob);
    addAndMakeVisible (vibratoDepthKnob);
    addAndMakeVisible (vibratoMixKnob);

    setUpSectionLabel (warmthSectionLabel, "WARMTH");
    addAndMakeVisible (warmthToneKnob);
    addAndMakeVisible (warmthDriveKnob);
    addAndMakeVisible (warmthBodyKnob);
    addAndMakeVisible (warmthMixKnob);

    // Lit IN/OUT switches for each section - flipping one off both bypasses
    // that section's contribution to the sound and collapses its knob row
    // (in resized()) so a disabled section can't confuse the user into
    // thinking its knobs still matter.
    setUpToggle (shiftToggle,   "Turn the Shift section (pitch + delay widener) on or off.");
    setUpToggle (vibratoToggle, "Turn the Vibrato section on or off.");
    setUpToggle (warmthToggle,  "Turn the Warmth tone stage on or off.");
    shiftToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, ParamIDs::shiftOn, shiftToggle);
    vibratoToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, ParamIDs::vibratoOn, vibratoToggle);
    warmthToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, ParamIDs::warmthOn, warmthToggle);

    // A toggle can also change from host automation or preset recall, not
    // just a click here - poll and relayout so the collapse always matches.
    startTimerHz (15);

    // Lets keyPressed() below receive Cmd+Z/Cmd+Shift+Z once this editor has
    // focus - mainly useful in the Standalone build, which has no
    // host-supplied Edit menu of its own.
    setWantsKeyboardFocus (true);

    // Resizable (host-driven, plus a drag corner) rather than fixed, so the
    // UI isn't stuck too small on a hi-DPI/scaled display. Locked to the
    // design's own aspect ratio so knobs, panels and text all scale
    // together instead of the layout stretching oddly in one direction.
    // A wide rack-panel shape - Shift/Vibrato/Warmth sit side by side as
    // three columns (see resized()) rather than stacked as three rows,
    // which used to make the window tall and narrow instead.
    constexpr int defaultWidth = 920, defaultHeight = 460;
    setResizable (true, true);
    setResizeLimits (defaultWidth * 3 / 4, defaultHeight * 3 / 4, defaultWidth * 2, defaultHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) defaultWidth / (double) defaultHeight);
    setSize (defaultWidth, defaultHeight);
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
    label.setColour (juce::Label::textColourId, currentTheme->accent);
    label.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (label);
}

void JJBreezeAudioProcessorEditor::setUpToggle (LedToggleButton& button, const juce::String& tooltip)
{
    button.setTooltip (tooltip);
    addAndMakeVisible (button);
}

void JJBreezeAudioProcessorEditor::switchCompareSlot (int targetSlot)
{
    if (targetSlot == processorRef.activeCompareSlot)
        return;

    // Save the tweaks made in the slot we're leaving, then - if the target
    // slot has never been touched - seed it as a copy of what's currently
    // live, so the very first switch to it is silent and only later edits
    // to that slot start to diverge.
    processorRef.storeCompareSnapshot (processorRef.activeCompareSlot);
    if (! processorRef.hasCompareSnapshotStored (targetSlot))
        processorRef.storeCompareSnapshot (targetSlot);

    processorRef.recallCompareSnapshot (targetSlot);
    processorRef.activeCompareSlot = targetSlot;
    updateCompareButtonColours();
}

void JJBreezeAudioProcessorEditor::applyTheme (const juce::String& themeId)
{
    currentTheme = &GearPalette::findTheme (themeId);

    // Everything that draws itself with a `theme` pointer/member gets it
    // reassigned here; everything else just gets re-coloured to match
    // (titleLabel/versionLabel/presetBox/section labels/compare+bypass
    // buttons below), same as their original setup did.
    retroLookAndFeel.setTheme (*currentTheme);
    shiftToggle.setTheme (*currentTheme);
    vibratoToggle.setTheme (*currentTheme);
    warmthToggle.setTheme (*currentTheme);
    bypassButton.setTheme (*currentTheme);
    bypassCaptionLabel.setColour (juce::Label::textColourId, currentTheme->accent);
    undoButton.setTheme (*currentTheme);
    redoButton.setTheme (*currentTheme);
    deleteButton.setTheme (*currentTheme);

    for (auto* knob : { &pitchLKnob, &pitchRKnob, &delayLKnob, &delayRKnob, &focusKnob, &mixKnob,
                         &vibratoRateKnob, &vibratoDepthKnob, &vibratoMixKnob,
                         &warmthToneKnob, &warmthDriveKnob, &warmthBodyKnob, &warmthMixKnob })
        knob->applyTheme (*currentTheme);

    titleLabel.setColour (juce::Label::textColourId, currentTheme->textLight);
    subtitleLabel.setColour (juce::Label::textColourId, currentTheme->textMuted);
    versionLabel.setColour (juce::Label::textColourId, currentTheme->textMuted.withAlpha (0.55f));
    for (auto* label : { &shiftSectionLabel, &vibratoSectionLabel, &warmthSectionLabel })
        label->setColour (juce::Label::textColourId, currentTheme->accent);

    presetBox.setColour (juce::ComboBox::backgroundColourId, currentTheme->ledBackground);
    presetBox.setColour (juce::ComboBox::textColourId, currentTheme->ledText);
    presetBox.setColour (juce::ComboBox::outlineColourId, currentTheme->metalDark);
    presetBox.setColour (juce::ComboBox::arrowColourId, currentTheme->accent);

    updateCompareButtonColours();
    updateBypassToggleState();
    repaint();
}

void JJBreezeAudioProcessorEditor::updateCompareButtonColours()
{
    const bool onA = processorRef.activeCompareSlot == 0;
    compareAButton.setColour (juce::TextButton::buttonColourId, onA ? currentTheme->accentDim : currentTheme->metalDark);
    compareAButton.setColour (juce::TextButton::textColourOffId, onA ? currentTheme->accent : currentTheme->textMuted);
    compareBButton.setColour (juce::TextButton::buttonColourId, onA ? currentTheme->metalDark : currentTheme->accentDim);
    compareBButton.setColour (juce::TextButton::textColourOffId, onA ? currentTheme->textMuted : currentTheme->accent);
}

void JJBreezeAudioProcessorEditor::updateBypassToggleState()
{
    // Inverted from the underlying flag - see the comment on bypassButton's
    // setup. Not a click-toggling button (bypass can also change from
    // outside a click on this switch - see timerCallback()), so its drawn
    // state is always synced from the actual bypass flag rather than assumed.
    bypassButton.setToggleState (! processorRef.isBypassed(), juce::dontSendNotification);
}

void JJBreezeAudioProcessorEditor::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);

    presetBox.addSectionHeading ("FACTORY");
    for (int i = 0; i < processorRef.getNumPrograms(); ++i)
        presetBox.addItem (processorRef.getProgramName (i), i + 1); // ComboBox item IDs are 1-based

    const auto userPresetNames = processorRef.getUserPresetNames();
    if (! userPresetNames.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("USER");
        for (int i = 0; i < userPresetNames.size(); ++i)
            presetBox.addItem (userPresetNames[i], firstUserPresetItemId + i);
    }

    if (activeUserPresetName.isNotEmpty())
    {
        const int idx = userPresetNames.indexOf (activeUserPresetName);
        if (idx >= 0)
            presetBox.setSelectedId (firstUserPresetItemId + idx, juce::dontSendNotification);
        else
            activeUserPresetName.clear(); // it was just deleted - nothing left to select under that name
    }

    if (activeUserPresetName.isEmpty())
    {
        lastKnownProgram = processorRef.getCurrentProgram();
        presetBox.setSelectedId (lastKnownProgram + 1, juce::dontSendNotification);
        deleteButton.setEnabled (false);
    }
    else
    {
        deleteButton.setEnabled (true);
    }

    activePresetSnapshot = processorRef.captureNormalizedSnapshot();
}

void JJBreezeAudioProcessorEditor::promptAndSaveUserPreset()
{
    auto* aw = new juce::AlertWindow ("Save Preset", "Name this preset:", juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", activeUserPresetName);
    aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    // deleteWhenDismissed is left false: enterModalState would delete aw
    // *before* invoking the callback in that mode, which would make reading
    // its text editor below a use-after-free. Instead the callback takes
    // ownership itself and reads it first.
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int result)
    {
        std::unique_ptr<juce::AlertWindow> ownedWindow (aw);

        if (result != 1)
            return;

        const auto name = ownedWindow->getTextEditorContents ("name").trim();
        if (name.isEmpty())
            return;

        processorRef.saveUserPreset (name);
        activeUserPresetName = name;
        refreshPresetBox();
    }));
}

void JJBreezeAudioProcessorEditor::promptAndDeleteUserPreset()
{
    if (activeUserPresetName.isEmpty())
        return;

    auto* aw = new juce::AlertWindow ("Delete Preset",
                                       "Delete \"" + activeUserPresetName + "\"? This can't be undone.",
                                       juce::MessageBoxIconType::WarningIcon);
    aw->addButton ("Delete", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    // Same ownership pattern as promptAndSaveUserPreset() above.
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int result)
    {
        std::unique_ptr<juce::AlertWindow> ownedWindow (aw);

        if (result != 1)
            return;

        processorRef.deleteUserPreset (activeUserPresetName);
        activeUserPresetName.clear();
        refreshPresetBox(); // falls back to showing the current factory program
    }));
}

bool JJBreezeAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0))
    {
        processorRef.undoManager.undo();
        return true;
    }

    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        processorRef.undoManager.redo();
        return true;
    }

    return false;
}

void JJBreezeAudioProcessorEditor::timerCallback()
{
    const bool shiftOn   = processorRef.apvts.getRawParameterValue (ParamIDs::shiftOn)->load()   > 0.5f;
    const bool vibratoOn = processorRef.apvts.getRawParameterValue (ParamIDs::vibratoOn)->load() > 0.5f;
    const bool warmthOn  = processorRef.apvts.getRawParameterValue (ParamIDs::warmthOn)->load()  > 0.5f;

    if (shiftOn != shiftWasOn || vibratoOn != vibratoWasOn || warmthOn != warmthWasOn)
    {
        shiftWasOn = shiftOn;
        vibratoWasOn = vibratoOn;
        warmthWasOn = warmthOn;
        resized();
        repaint();
    }

    // A section coming back on while we still think we're bypassed means
    // it was flipped directly (a click on its own toggle, or host
    // automation) - bypass no longer describes the actual state, so stop
    // showing it as active rather than let the LED lie.
    if (processorRef.isBypassed() && (shiftOn || vibratoOn || warmthOn))
    {
        processorRef.clearBypassedFlag();
        updateBypassToggleState();
    }

    // The current program can also change from outside this editor - the
    // host's own preset menu, automation, or a saved session reloading -
    // keep the picker in sync without re-triggering onChange (which would
    // otherwise just re-apply the same preset). An external program change
    // always means a factory preset is now active, even if a user preset
    // was showing a moment ago.
    const int currentProgram = processorRef.getCurrentProgram();
    if (currentProgram != lastKnownProgram)
    {
        lastKnownProgram = currentProgram;
        activeUserPresetName.clear();
        presetBox.setSelectedId (currentProgram + 1, juce::dontSendNotification);
        activePresetSnapshot = processorRef.captureNormalizedSnapshot();
        deleteButton.setEnabled (false);
    }

    // "Modified" indicator: dim the preset picker's text once the live
    // patch no longer matches what the selected preset actually contains.
    const bool modified = ! processorRef.matchesNormalizedSnapshot (activePresetSnapshot);
    presetBox.setColour (juce::ComboBox::textColourId, modified ? currentTheme->textMuted : currentTheme->ledText);

    undoButton.setEnabled (processorRef.undoManager.canUndo());
    redoButton.setEnabled (processorRef.undoManager.canRedo());
}

void JJBreezeAudioProcessorEditor::drawBolt (juce::Graphics& g, juce::Point<float> centre, float radius) const
{
    const auto bounds = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);
    juce::ColourGradient grad (currentTheme->metalLight, centre.x - radius * 0.3f, centre.y - radius * 0.4f,
                                currentTheme->metalDark, centre.x, centre.y, true);
    g.setGradientFill (grad);
    g.fillEllipse (bounds);
    g.setColour (currentTheme->chassisBottom.withAlpha (0.5f));
    g.drawEllipse (bounds, 1.0f);
}

// Shared with resized() so the header, dividers, ears and knob columns all
// land in exactly the same place — see the mockup this matches.
static constexpr int headerHeight = 90; // nameplate+power row, then one combined controls row
static constexpr int earWidth = 44; // rack-ear side strips (see paint())
static constexpr int contentGutter = 16; // gap between an ear and the content it flanks
static constexpr int sideMargin = earWidth + contentGutter;
static constexpr int topPadding = 12;
static constexpr int footerStripHeight = 18;
static constexpr int bottomPadding = 14; // clearance above the footer strip
static constexpr int sectionLabelHeight = 34; // taller than a text row alone needs, so the section on/off switches (below) have real room
static constexpr int columnGap = 16; // horizontal gap between the Shift/Vibrato/Warmth columns
static constexpr int toggleWidth = 34;
static constexpr int numColumns = 3;
static constexpr int numColumnGaps = 2; // gaps: Shift-Vibrato, Vibrato-Warmth
static constexpr int maxKnobRows = 2; // the most any one column needs (Shift and Warmth both use 2)

void JJBreezeAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    auto bounds = full;

    // Chassis, lit from the top like a rack unit under studio lighting.
    // Colours (and everything else below) come from currentTheme rather
    // than a fixed palette — see applyTheme().
    juce::ColourGradient backdrop (currentTheme->chassisTop, bounds.getCentreX(), bounds.getY(),
                                    currentTheme->chassisBottom, bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill (backdrop);
    g.fillRect (bounds);

    // Faint brushed-aluminium grain.
    juce::Random grain (12345);
    for (float ly = bounds.getY(); ly < bounds.getBottom(); ly += 3.0f)
    {
        g.setColour (juce::Colours::white.withAlpha (grain.nextFloat() * 0.035f));
        g.drawHorizontalLine ((int) ly, 0.0f, bounds.getWidth());
    }

    // Rack ears — full-height side strips carrying two mounting bolts
    // each, matching the design mockup exactly (replaces the old four
    // corner screws).
    auto drawEar = [&] (const juce::Rectangle<float>& ear)
    {
        juce::ColourGradient earGrad (currentTheme->metalMid, ear.getX(), ear.getY(),
                                       currentTheme->metalDark, ear.getX(), ear.getBottom(), false);
        g.setGradientFill (earGrad);
        g.fillRect (ear);

        const float boltRadius = (float) earWidth * 0.17f;
        drawBolt (g, { ear.getCentreX(), ear.getY() + ear.getHeight() * 0.28f }, boltRadius);
        drawBolt (g, { ear.getCentreX(), ear.getY() + ear.getHeight() * 0.72f }, boltRadius);
    };
    drawEar ({ full.getX(), full.getY(), (float) earWidth, full.getHeight() });
    drawEar ({ full.getRight() - (float) earWidth, full.getY(), (float) earWidth, full.getHeight() });

    // Header strip with a machined seam underneath (a light line over a
    // dark one, like a panel edge catching the light).
    auto header = bounds.removeFromTop ((float) headerHeight);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRect (header);

    // One thin divider under the header, then two more between the three
    // knob columns — flat hairlines rather than rounded cards, matching
    // the mockup. Same inset/removal amounts as resized() so these always
    // land exactly between the knob columns they're separating.
    auto content = bounds.reduced ((float) sideMargin, 0.0f);
    content.removeFromTop (8.0f);
    g.setColour (currentTheme->textLight.withAlpha (0.14f));
    g.drawHorizontalLine ((int) content.getY(), content.getX(), content.getRight());

    content.removeFromTop ((float) topPadding);
    content.removeFromBottom ((float) (bottomPadding + footerStripHeight));
    const float columnWidthF = (content.getWidth() - (float) (numColumnGaps * columnGap)) / (float) numColumns;
    for (int i = 1; i < numColumns; ++i)
    {
        const float x = content.getX() + (float) i * columnWidthF + ((float) i - 0.5f) * (float) columnGap;
        g.drawVerticalLine ((int) x, content.getY(), content.getBottom());
    }

    // Footer strip with small rivets, like the perforated base strip on a
    // real rack unit.
    auto footer = juce::Rectangle<float> (full.getX(), full.getBottom() - (float) footerStripHeight,
                                           full.getWidth(), (float) footerStripHeight);
    g.setColour (juce::Colours::black.withAlpha (0.12f));
    g.fillRect (footer);
    constexpr int numRivets = 10;
    for (int i = 0; i < numRivets; ++i)
    {
        const float t = (float) (i + 1) / (float) (numRivets + 1);
        g.setColour (currentTheme->textLight.withAlpha (0.18f));
        g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre ({ footer.getX() + t * footer.getWidth(), footer.getCentreY() }));
    }
}

void JJBreezeAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // Inset by sideMargin (not just a flat margin) so nothing sits under
    // the rack ears painted in paint().
    auto header = area.removeFromTop (headerHeight);
    header.removeFromLeft (sideMargin);
    header.removeFromRight (sideMargin);
    header = header.reduced (0, 10);

    // Nameplate row: the italic wordmark and its tracked-caps subtitle,
    // left-aligned, plus the bypass switch on the right - same row as the
    // header text, like the design mockup's POWER switch.
    auto titleRow = header.removeFromTop (32);

    auto bypassBlock = titleRow.removeFromRight (90);
    bypassButton.setBounds (bypassBlock.removeFromRight (24).reduced (0, 1));
    bypassBlock.removeFromRight (8);
    bypassCaptionLabel.setBounds (bypassBlock);
    titleRow.removeFromRight (16); // gap before the nameplate

    const int titleWidth = juce::GlyphArrangement::getStringWidthInt (titleLabel.getFont(), titleLabel.getText()) + 4;
    titleLabel.setBounds (titleRow.removeFromLeft (titleWidth));
    titleRow.removeFromLeft (12);
    subtitleLabel.setBounds (titleRow);
    header.removeFromTop (8); // gap before the controls row

    // Undo/redo, the preset picker, Save/Delete and A/B compare all share
    // one row now (used to be two) - frees a whole row's height for the
    // knob columns below. Left to right: preset picker (capped width, not
    // flexible - the whole point is to leave room in the middle for other
    // things later), Save, Delete; then, right-aligned, undo/redo and A/B
    // compare.
    auto controlsRow = header;

    compareBButton.setBounds (controlsRow.removeFromRight (28));
    controlsRow.removeFromRight (4);
    compareAButton.setBounds (controlsRow.removeFromRight (28));
    controlsRow.removeFromRight (10);

    redoButton.setBounds (controlsRow.removeFromRight (30));
    controlsRow.removeFromRight (4);
    undoButton.setBounds (controlsRow.removeFromRight (30));

    presetBox.setBounds (controlsRow.removeFromLeft (220));
    controlsRow.removeFromLeft (8);
    saveButton.setBounds (controlsRow.removeFromLeft (46));
    controlsRow.removeFromLeft (4);
    deleteButton.setBounds (controlsRow.removeFromLeft (30));
    // Whatever's left of controlsRow stays blank - reserved space.

    // Same insets as paint()'s divider-line geometry above, so the hairlines
    // between columns always land exactly at each column's edge.
    area.removeFromTop (8); // gap before the divider
    area.removeFromLeft (sideMargin);
    area.removeFromRight (sideMargin);
    area.removeFromTop (topPadding);
    area.removeFromBottom (bottomPadding + footerStripHeight);

    const bool shiftOn   = processorRef.apvts.getRawParameterValue (ParamIDs::shiftOn)->load()   > 0.5f;
    const bool vibratoOn = processorRef.apvts.getRawParameterValue (ParamIDs::vibratoOn)->load() > 0.5f;
    const bool warmthOn  = processorRef.apvts.getRawParameterValue (ParamIDs::warmthOn)->load()  > 0.5f;

    // Shift / Vibrato / Warmth sit side by side as three equal-width
    // columns (a wide rack-panel layout) rather than stacked rows. Every
    // column gets the same fixed row height, sized for the tallest column
    // (Shift and Warmth both need two knob rows; Vibrato's single row just
    // leaves the rest of its column's height empty below it) - so, unlike
    // the old stacked layout, turning a section off doesn't hand its space
    // to its neighbours (they're not sharing a vertical run any more), it
    // just collapses that column to its label bar.
    const int columnWidth = (area.getWidth() - numColumnGaps * columnGap) / numColumns;
    const int rowHeight = (area.getHeight() - sectionLabelHeight) / maxKnobRows;

    auto layoutColumn = [&] (juce::Rectangle<int> column, LedToggleButton& toggle, juce::Label& sectionLabel,
                              bool on, std::initializer_list<std::initializer_list<LabelledKnob*>> rows)
    {
        auto fullLabelRow = column.removeFromTop (sectionLabelHeight);
        auto labelRow = fullLabelRow;
        toggle.setBounds (labelRow.removeFromRight (toggleWidth).reduced (0, 1));
        labelRow.removeFromRight (6);
        sectionLabel.setBounds (labelRow);

        for (auto& row : rows)
            for (auto* knob : row)
                knob->setVisible (on);

        if (! on)
            return;

        for (auto& row : rows)
        {
            auto rowArea = column.removeFromTop (rowHeight);
            const int knobWidth = rowArea.getWidth() / (int) row.size();
            for (size_t i = 0; i < row.size(); ++i)
            {
                auto* knob = *(row.begin() + (long) i);
                // The last knob in the row takes whatever's left, absorbing
                // any rounding remainder, rather than one more equal slice.
                auto slot = (i + 1 == row.size()) ? rowArea : rowArea.removeFromLeft (knobWidth);
                knob->setBounds (slot.reduced (8));
            }
        }
    };

    auto shiftColumn = area.removeFromLeft (columnWidth);
    area.removeFromLeft (columnGap);
    auto vibratoColumn = area.removeFromLeft (columnWidth);
    area.removeFromLeft (columnGap);
    auto warmthColumn = area; // takes whatever's left, absorbing rounding remainder

    layoutColumn (shiftColumn, shiftToggle, shiftSectionLabel, shiftOn,
                  { { &pitchLKnob, &pitchRKnob, &focusKnob }, { &delayLKnob, &delayRKnob, &mixKnob } });

    layoutColumn (vibratoColumn, vibratoToggle, vibratoSectionLabel, vibratoOn,
                  { { &vibratoRateKnob, &vibratoDepthKnob, &vibratoMixKnob } });

    layoutColumn (warmthColumn, warmthToggle, warmthSectionLabel, warmthOn,
                  { { &warmthToneKnob, &warmthDriveKnob }, { &warmthBodyKnob, &warmthMixKnob } });

    // Version readout, centred in the thin gap just above the rivet strip
    // — computed from the untouched full bounds rather than the already-
    // consumed `area`, same as the rack ears/footer strip in paint().
    {
        const auto full = getLocalBounds();
        versionLabel.setBounds (full.getX() + sideMargin, full.getBottom() - footerStripHeight - bottomPadding,
                                 full.getWidth() - sideMargin * 2, bottomPadding);
    }
}
