#include "PluginEditor.h"

namespace
{
    // Which finish the panel is wearing is a per-machine preference, not
    // part of the patch: it lives in a small settings file of its own rather
    // than in the plugin state, so it neither travels with a session nor
    // shows up in the preset list — the same split the AUv3 sibling makes
    // (ThemeStore keeps its choice in UserDefaults, well away from its user
    // presets). The PropertiesFile is opened on the stack for each read and
    // write rather than held as a static: a static would still be alive when
    // JUCE's leak detector runs at plugin unload, and report itself as a
    // leak in a debug build.
    constexpr const char* themeSettingKey = "finish";

    juce::PropertiesFile::Options uiSettingsOptions()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "j.j.breeze";
        options.filenameSuffix = "settings";
        options.folderName = "Gerov";
        options.osxLibrarySubFolder = "Application Support";
        return options;
    }

    juce::String storedFinishId (const juce::String& fallback)
    {
        juce::PropertiesFile settings (uiSettingsOptions());
        return settings.getValue (themeSettingKey, fallback);
    }

    void storeFinishId (const juce::String& id)
    {
        juce::PropertiesFile settings (uiSettingsOptions());
        settings.setValue (themeSettingKey, id);
        settings.saveIfNeeded();
    }
}

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
    #define JJ_BREEZE_DEFAULT_THEME "green" // guards a non-CMake build of this file
#endif

    // Fits whichever finish was last selected on this machine, falling back
    // to the build-time JJ_BREEZE_DEFAULT_THEME the first time the plugin
    // runs. Done before anything below sets a single cached colour, so every
    // child is born already themed rather than painted once in GearPalette's
    // compile-time default and corrected a frame later. The
    // pitchLKnob..warmthMixKnob members above were already constructed (in
    // the init list, before this body runs) against that same default —
    // applyTheme() below re-applies to them too.
    applyTheme (storedFinishId (JJ_BREEZE_DEFAULT_THEME));

    setLookAndFeel (&retroLookAndFeel);

    // The finish selector: a small rotary switch left of the wordmark, the
    // way the panel colour would be a fitted option on the real thing —
    // deliberately not a preset parameter, see uiSettings() above.
    finishSelector.setTooltip ("Panel finish - turns to the next colourway.");
    finishSelector.onClick = [this] { cycleTheme(); };
    addAndMakeVisible (finishSelector);

    // Nameplate: the italic serif wordmark. Its strapline now lives on the
    // stamped nameplate at the bottom of the panel (modelPlate below), as
    // it does on the hardware this is drawn from.
    titleLabel.setText ("j.j.breeze", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions ("Georgia", 22.0f, juce::Font::italic | juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, currentTheme->textLight);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    modelPlate.setText ("MODEL JJB-1  -  STEREO MICRO-PITCH WIDENER  -  v" JucePlugin_VersionString);
    modelPlate.setTheme (*currentTheme);
    addAndMakeVisible (modelPlate);

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
    // A bat switch (see BatSwitchButton), not a click-toggling button - its
    // drawn state always comes from processorRef.isBypassed() via
    // updateBypassToggleState(), same idea as the section on/off switches
    // but with no apvts parameter of its own to attach to. The jewel lamp
    // above it is the pilot light, lit whenever the unit is processing.
    bypassButton.setTooltip ("Power off to bypass all processing (the untouched dry signal) without changing any knob or section state; power back on to resume.");
    bypassButton.setClickingTogglesState (false);
    bypassButton.onClick = [this]
    {
        processorRef.setBypassed (! processorRef.isBypassed());
        updateBypassToggleState();
    };
    addAndMakeVisible (bypassButton);

    powerLamp.setColourway (currentTheme->lampRed);
    addAndMakeVisible (powerLamp);
    updateBypassToggleState();

    bypassCaptionLabel.setText ("POWER", juce::dontSendNotification);
    bypassCaptionLabel.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)).withExtraKerningFactor (0.2f));
    bypassCaptionLabel.setColour (juce::Label::textColourId, currentTheme->textLight.withAlpha (0.9f));
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

    for (auto* lamp : { &shiftLamp, &vibratoLamp, &warmthLamp })
    {
        lamp->setColourway (currentTheme->accent);
        addAndMakeVisible (*lamp);
    }

    // Bat switches for each section - flipping one off both bypasses that
    // section's contribution to the sound and dims its knobs (see
    // updateSectionEnablement()), so a disabled section can't confuse the
    // user into thinking its knobs still matter, while the module itself
    // stays on the panel where it was.
    setUpToggle (shiftToggle,   "Turn the Shift section (pitch + delay widener) on or off.");
    setUpToggle (vibratoToggle, "Turn the Vibrato section on or off.");
    setUpToggle (warmthToggle,  "Turn the Warmth tone stage on or off.");
    shiftToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, ParamIDs::shiftOn, shiftToggle);
    vibratoToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, ParamIDs::vibratoOn, vibratoToggle);
    warmthToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, ParamIDs::warmthOn, warmthToggle);
    updateSectionEnablement();

    // A toggle can also change from host automation or preset recall, not
    // just a click here - poll and re-dim so the panel always matches.
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
    // three sub-plates (see resized()) rather than stacked as three rows,
    // which used to make the window tall and narrow instead.
    constexpr int defaultWidth = 920, defaultHeight = 480;
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
    label.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)).withExtraKerningFactor (0.2f));
    label.setColour (juce::Label::textColourId, currentTheme->textLight);
    label.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (label);
}

void JJBreezeAudioProcessorEditor::setUpToggle (BatSwitchButton& button, const juce::String& tooltip)
{
    button.setTooltip (tooltip);
    addAndMakeVisible (button);
}

void JJBreezeAudioProcessorEditor::applyTheme (const juce::String& themeId, bool remember)
{
    currentTheme = &GearPalette::findTheme (themeId);

    if (remember)
        storeFinishId (currentTheme->id);

    // Everything that draws itself with a `theme` pointer/member gets it
    // reassigned here; everything else just gets re-coloured to match,
    // same as their original setup did.
    retroLookAndFeel.setTheme (*currentTheme);
    finishSelector.setTheme (*currentTheme);
    modelPlate.setTheme (*currentTheme);
    shiftToggle.setTheme (*currentTheme);
    vibratoToggle.setTheme (*currentTheme);
    warmthToggle.setTheme (*currentTheme);
    bypassButton.setTheme (*currentTheme);
    undoButton.setTheme (*currentTheme);
    redoButton.setTheme (*currentTheme);
    deleteButton.setTheme (*currentTheme);

    for (auto* lamp : { &shiftLamp, &vibratoLamp, &warmthLamp })
    {
        lamp->setTheme (*currentTheme);
        lamp->setColourway (currentTheme->accent);
    }
    powerLamp.setTheme (*currentTheme);
    powerLamp.setColourway (currentTheme->lampRed);

    for (auto* knob : { &pitchLKnob, &pitchRKnob, &delayLKnob, &delayRKnob, &focusKnob, &mixKnob,
                         &vibratoRateKnob, &vibratoDepthKnob, &vibratoMixKnob,
                         &warmthToneKnob, &warmthDriveKnob, &warmthBodyKnob, &warmthMixKnob })
        knob->applyTheme (*currentTheme);

    titleLabel.setColour (juce::Label::textColourId, currentTheme->textLight);
    bypassCaptionLabel.setColour (juce::Label::textColourId, currentTheme->textLight.withAlpha (0.9f));

    presetBox.setColour (juce::ComboBox::backgroundColourId, currentTheme->ledBackground);
    presetBox.setColour (juce::ComboBox::textColourId, currentTheme->ledText);
    presetBox.setColour (juce::ComboBox::outlineColourId, currentTheme->metalDark);
    presetBox.setColour (juce::ComboBox::arrowColourId, currentTheme->accent);

    saveButton.setColour (juce::TextButton::buttonColourId, currentTheme->metalDark);
    saveButton.setColour (juce::TextButton::textColourOffId, currentTheme->textLight);

    // The dropdown, the save/delete dialogs and their text field are
    // separate windows drawn by this LookAndFeel too — left in the default
    // grey they'd be the one part of the plugin that isn't the fitted
    // finish.
    retroLookAndFeel.setColour (juce::PopupMenu::backgroundColourId, currentTheme->ledBackground);
    retroLookAndFeel.setColour (juce::PopupMenu::textColourId, currentTheme->textLight);
    retroLookAndFeel.setColour (juce::PopupMenu::headerTextColourId, currentTheme->accent);
    retroLookAndFeel.setColour (juce::PopupMenu::highlightedBackgroundColourId, currentTheme->accentDim);
    retroLookAndFeel.setColour (juce::PopupMenu::highlightedTextColourId, currentTheme->textLight);
    retroLookAndFeel.setColour (juce::AlertWindow::backgroundColourId, currentTheme->panelFill);
    retroLookAndFeel.setColour (juce::AlertWindow::textColourId, currentTheme->textLight);
    retroLookAndFeel.setColour (juce::AlertWindow::outlineColourId, currentTheme->metalDark);
    retroLookAndFeel.setColour (juce::TextEditor::backgroundColourId, currentTheme->ledBackground);
    retroLookAndFeel.setColour (juce::TextEditor::textColourId, currentTheme->ledText);
    retroLookAndFeel.setColour (juce::TextEditor::highlightColourId, currentTheme->accentDim);
    retroLookAndFeel.setColour (juce::TextEditor::outlineColourId, currentTheme->metalDark);
    retroLookAndFeel.setColour (juce::TextEditor::focusedOutlineColourId, currentTheme->accent);

    updateCompareButtonColours();
    updateBypassToggleState();
    updateSectionEnablement(); // re-dims whatever is switched off in the new finish's colours
    rebuildPanelImage();
    repaint();
}

void JJBreezeAudioProcessorEditor::cycleTheme()
{
    const auto& themes = GearPalette::allThemes();
    const int next = (GearPalette::indexOfTheme (currentTheme->id) + 1) % (int) themes.size();
    applyTheme (themes[(size_t) next].id, true);
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
    const bool powered = ! processorRef.isBypassed();
    bypassButton.setToggleState (powered, juce::dontSendNotification);
    powerLamp.setOn (powered);
}

// Dims and locks a section's knobs when its switch is off, rather than
// hiding them: the module keeps its sub-plate, its name and its switch, so
// the panel doesn't reshuffle every time a section is turned off - the same
// behaviour as the AUv3 sibling (see JJBreezeMainView's .opacity/
// .allowsHitTesting on each column).
void JJBreezeAudioProcessorEditor::setSectionEnabled (bool on, juce::Label& sectionLabel, JewelLampComponent& lamp,
                                                       std::initializer_list<LabelledKnob*> knobs)
{
    sectionLabel.setColour (juce::Label::textColourId, on ? currentTheme->textLight : currentTheme->textMuted);
    sectionLabel.repaint();
    lamp.setOn (on);

    for (auto* knob : knobs)
    {
        knob->setAlpha (on ? 1.0f : 0.42f);
        knob->setEnabled (on);
    }
}

// Reads the three section-enabled parameters and dims/undims accordingly,
// caching what it saw so timerCallback() can tell when one of them changes
// from outside this editor (host automation, preset recall) without
// re-applying — and repainting — every tick.
void JJBreezeAudioProcessorEditor::updateSectionEnablement()
{
    shiftWasOn   = processorRef.apvts.getRawParameterValue (ParamIDs::shiftOn)->load()   > 0.5f;
    vibratoWasOn = processorRef.apvts.getRawParameterValue (ParamIDs::vibratoOn)->load() > 0.5f;
    warmthWasOn  = processorRef.apvts.getRawParameterValue (ParamIDs::warmthOn)->load()  > 0.5f;

    setSectionEnabled (shiftWasOn, shiftSectionLabel, shiftLamp,
                        { &pitchLKnob, &pitchRKnob, &focusKnob, &delayLKnob, &delayRKnob, &mixKnob });
    setSectionEnabled (vibratoWasOn, vibratoSectionLabel, vibratoLamp,
                        { &vibratoRateKnob, &vibratoDepthKnob, &vibratoMixKnob });
    setSectionEnabled (warmthWasOn, warmthSectionLabel, warmthLamp,
                        { &warmthToneKnob, &warmthDriveKnob, &warmthBodyKnob, &warmthMixKnob });
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
        updateSectionEnablement();

    // A section coming back on while we still think we're bypassed means
    // it was flipped directly (a click on its own toggle, or host
    // automation) - bypass no longer describes the actual state, so stop
    // showing it as active rather than let the pilot lamp lie.
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

// Shared by resized() and rebuildPanelImage() so the hardware and the
// controls mounted on it always land in the same place — see the AUv3
// sibling's earWidth/footerHeight/contentGutter, which these mirror at
// desktop scale.
static constexpr int headerHeight = 64; // one row: wordmark, presets, undo/redo, A/B, power
static constexpr int headerPadding = 8; // above and below that row
static constexpr int headerControlHeight = 26; // preset picker and the buttons beside it
static constexpr int earWidth = 44; // rack-ear side rails
static constexpr int contentGutter = 16; // gap between an ear and the content it flanks
static constexpr int sideMargin = earWidth + contentGutter;
static constexpr int grooveGap = 8; // gap between the header and the engraved groove under it
static constexpr int topPadding = 12; // gap between that groove and the section plates
static constexpr int footerStripHeight = 18; // riveted steel base rail
static constexpr int modelPlateHeight = 18;
static constexpr int modelPlateGap = 7; // clearance above and below the nameplate
static constexpr int columnGap = 18; // gap between the Shift/Vibrato/Warmth plates
static constexpr int numColumns = 3;
static constexpr int numColumnGaps = 2; // gaps: Shift-Vibrato, Vibrato-Warmth
static constexpr int maxKnobRows = 2; // the most any one section needs (Shift and Warmth both use 2)
static constexpr int platePaddingX = 19; // inside a sub-plate, clear of its corner screws
static constexpr int platePaddingTop = 9;
static constexpr int platePaddingBottom = 12;
static constexpr int sectionLabelHeight = 34; // room for the lamp, the engraved name and the bat switch
static constexpr int toggleWidth = 26;
static constexpr int lampSize = 10;
static constexpr int knobSlotMargin = 5; // breathing room each side of a knob within its slot

std::array<juce::Rectangle<int>, 3> JJBreezeAudioProcessorEditor::sectionPlateBounds() const
{
    auto area = getLocalBounds();
    area.removeFromTop (headerHeight + grooveGap + topPadding);
    area.removeFromLeft (sideMargin);
    area.removeFromRight (sideMargin);
    area.removeFromBottom (footerStripHeight + modelPlateHeight + modelPlateGap * 2);

    const int columnWidth = (area.getWidth() - numColumnGaps * columnGap) / numColumns;

    std::array<juce::Rectangle<int>, 3> plates;
    plates[0] = area.removeFromLeft (columnWidth);
    area.removeFromLeft (columnGap);
    plates[1] = area.removeFromLeft (columnWidth);
    area.removeFromLeft (columnGap);
    plates[2] = area; // takes whatever's left, absorbing the rounding remainder
    return plates;
}

void JJBreezeAudioProcessorEditor::rebuildPanelImage()
{
    const auto bounds = getLocalBounds();

    if (bounds.isEmpty())
    {
        panelImage = juce::Image();
        return;
    }

    // Rendered at the display's own pixel density rather than in logical
    // units, so the screws, plate bevels and lettering stay crisp on a
    // Retina/scaled display instead of being blitted up from a smaller
    // bitmap.
    float scale = 1.0f;
    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        scale = juce::jlimit (1.0f, 3.0f, (float) display->scale);

    panelImage = juce::Image (juce::Image::RGB,
                               juce::roundToInt ((float) bounds.getWidth() * scale),
                               juce::roundToInt ((float) bounds.getHeight() * scale), false);

    juce::Graphics g (panelImage);
    g.addTransform (juce::AffineTransform::scale (scale));

    const auto full = bounds.toFloat();

    // Hammered enamel front plate, lit from the front centre.
    RetroDraw::chassisPaint (g, *currentTheme, full);

    // Engraved groove under the header: a scored line with the light
    // catching its lower lip, rather than a flat hairline.
    const int grooveY = headerHeight + grooveGap;
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawHorizontalLine (grooveY, (float) sideMargin, full.getRight() - (float) sideMargin);
    g.setColour (currentTheme->panelEdgeLight.withAlpha (0.35f));
    g.drawHorizontalLine (grooveY + 1, (float) sideMargin, full.getRight() - (float) sideMargin);

    // One screwed-on sub-plate per section, the way 70s consoles bolted a
    // separate engraved plate over every module.
    for (const auto& plate : sectionPlateBounds())
        RetroDraw::panelPlate (g, *currentTheme, plate.toFloat());

    // Rack ears and the riveted base rail are bolted on over everything
    // else, same as on the real thing.
    RetroDraw::rackEar (g, *currentTheme, { full.getX(), full.getY(), (float) earWidth, full.getHeight() });
    RetroDraw::rackEar (g, *currentTheme, { full.getRight() - (float) earWidth, full.getY(),
                                             (float) earWidth, full.getHeight() });
    RetroDraw::footerRivetStrip (g, *currentTheme,
                                  { full.getX(), full.getBottom() - (float) footerStripHeight,
                                    full.getWidth(), (float) footerStripHeight }, 16);
}

void JJBreezeAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Everything static about the panel was rendered once by
    // rebuildPanelImage(); only the controls on top of it are live.
    if (panelImage.isValid())
    {
        g.drawImage (panelImage, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
        return;
    }

    g.fillAll (currentTheme->paintBottom);
}

void JJBreezeAudioProcessorEditor::resized()
{
    rebuildPanelImage();

    auto area = getLocalBounds();

    // Inset by sideMargin (not just a flat margin) so nothing sits under
    // the rack ears.
    auto header = area.removeFromTop (headerHeight);
    header.removeFromLeft (sideMargin);
    header.removeFromRight (sideMargin);
    header = header.reduced (0, headerPadding);

    // Everything the header carries sits on one row, as it does on the
    // AUv3 sibling's panel: wordmark, presets, undo/redo, A/B, power. The
    // flat controls are shorter than the row (which is as tall as the
    // power switch needs), so each is centred in the height it's given.
    auto centred = [] (juce::Rectangle<int> slot)
    {
        return slot.withSizeKeepingCentre (slot.getWidth(), headerControlHeight);
    };

    // Right-hand end first, so the preset picker in the middle can take
    // whatever width is actually left over.
    auto powerBlock = header.removeFromRight (26);
    powerLamp.setBounds (powerBlock.removeFromTop (10).withSizeKeepingCentre (10, 10));
    powerBlock.removeFromTop (2);
    bypassButton.setBounds (powerBlock);
    header.removeFromRight (6);
    bypassCaptionLabel.setBounds (header.removeFromRight (46));
    header.removeFromRight (14);

    compareBButton.setBounds (centred (header.removeFromRight (28)));
    header.removeFromRight (4);
    compareAButton.setBounds (centred (header.removeFromRight (28)));
    header.removeFromRight (12);

    redoButton.setBounds (centred (header.removeFromRight (30)));
    header.removeFromRight (4);
    undoButton.setBounds (centred (header.removeFromRight (30)));
    header.removeFromRight (14);

    // Left-hand end: the finish selector and the italic wordmark.
    finishSelector.setBounds (header.removeFromLeft (26).withSizeKeepingCentre (26, 26));
    header.removeFromLeft (10);
    const int titleWidth = juce::GlyphArrangement::getStringWidthInt (titleLabel.getFont(), titleLabel.getText()) + 6;
    titleLabel.setBounds (header.removeFromLeft (titleWidth));
    header.removeFromLeft (14);

    // The preset picker fills the gap between the two, capped so it doesn't
    // stretch right across a resized-up window, with Save/Delete after it.
    constexpr int saveAndDeleteWidth = 46 + 4 + 28 + 8; // both buttons, plus the gaps around them
    presetBox.setBounds (centred (header.removeFromLeft (
        juce::jlimit (110, 260, header.getWidth() - saveAndDeleteWidth))));
    header.removeFromLeft (8);
    saveButton.setBounds (centred (header.removeFromLeft (46)));
    header.removeFromLeft (4);
    deleteButton.setBounds (centred (header.removeFromLeft (28)));
    // Whatever's left of the row stays blank - reserved space.

    // Each section's controls, mounted on the sub-plate painted for it by
    // rebuildPanelImage(). A section that's switched off keeps its plate,
    // its name and its switch - only its knobs dim (see
    // updateSectionEnablement()), so nothing on the panel moves.
    auto layoutSection = [&] (juce::Rectangle<int> plate, BatSwitchButton& toggle, juce::Label& sectionLabel,
                               JewelLampComponent& lamp,
                               std::initializer_list<std::initializer_list<LabelledKnob*>> rows)
    {
        auto inner = plate.reduced (platePaddingX, 0);
        inner.removeFromTop (platePaddingTop);
        inner.removeFromBottom (platePaddingBottom);

        auto labelRow = inner.removeFromTop (sectionLabelHeight);
        lamp.setBounds (labelRow.removeFromLeft (lampSize).withSizeKeepingCentre (lampSize, lampSize));
        labelRow.removeFromLeft (7);
        toggle.setBounds (labelRow.removeFromRight (toggleWidth));
        labelRow.removeFromRight (6);
        sectionLabel.setBounds (labelRow);

        // Every section uses the same row height, sized for the tallest one
        // (Shift and Warmth both need two rows) - so a single-row section's
        // knobs draw exactly as big as everyone else's, as they would if
        // all three modules were cut from the same rack panel.
        const int rowHeight = inner.getHeight() / maxKnobRows;
        for (auto& row : rows)
        {
            auto rowArea = inner.removeFromTop (rowHeight);
            const int knobWidth = rowArea.getWidth() / (int) row.size();
            for (size_t i = 0; i < row.size(); ++i)
            {
                auto* knob = *(row.begin() + (long) i);
                // The last knob in the row takes whatever's left, absorbing
                // any rounding remainder, rather than one more equal slice.
                auto slot = (i + 1 == row.size()) ? rowArea : rowArea.removeFromLeft (knobWidth);
                knob->setBounds (slot.reduced (knobSlotMargin, 4));
            }
        }
    };

    const auto plates = sectionPlateBounds();

    // The diameter every knob draws at: the width one knob gets in a
    // three-across row, so Warmth's two-across rows don't draw bigger ones.
    // Mirrors the AUv3 sibling's knobDiameter(forPanelWidth:).
    const int knobSlot = (plates[0].getWidth() - platePaddingX * 2) / 3 - knobSlotMargin * 2;
    for (auto* knob : { &pitchLKnob, &pitchRKnob, &delayLKnob, &delayRKnob, &focusKnob, &mixKnob,
                         &vibratoRateKnob, &vibratoDepthKnob, &vibratoMixKnob,
                         &warmthToneKnob, &warmthDriveKnob, &warmthBodyKnob, &warmthMixKnob })
        knob->setKnobDiameterCap (juce::jlimit (44, 104, knobSlot));

    layoutSection (plates[0], shiftToggle, shiftSectionLabel, shiftLamp,
                    { { &pitchLKnob, &pitchRKnob, &focusKnob }, { &delayLKnob, &delayRKnob, &mixKnob } });

    layoutSection (plates[1], vibratoToggle, vibratoSectionLabel, vibratoLamp,
                    { { &vibratoRateKnob, &vibratoDepthKnob, &vibratoMixKnob } });

    layoutSection (plates[2], warmthToggle, warmthSectionLabel, warmthLamp,
                    { { &warmthToneKnob, &warmthDriveKnob }, { &warmthBodyKnob, &warmthMixKnob } });

    // The stamped nameplate, riveted centrally between the plates and the
    // base rail - only as wide as its own lettering, like the real thing.
    {
        const auto full = getLocalBounds();
        const int plateWidth = juce::jmin (modelPlate.getPreferredWidth(), full.getWidth() - sideMargin * 2);
        modelPlate.setBounds (juce::Rectangle<int> (plateWidth, modelPlateHeight)
                                  .withCentre ({ full.getCentreX(),
                                                 full.getBottom() - footerStripHeight - modelPlateGap - modelPlateHeight / 2 }));
    }
}
