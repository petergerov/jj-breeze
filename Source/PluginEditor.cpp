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
    // Restore the persisted colourway before anything below sets a single
    // cached colour, so every child is born already themed rather than
    // painted once in the default theme and corrected a frame later. The
    // pitchLKnob..warmthMixKnob members above were already constructed
    // (in the init list, before this body runs) against GearPalette's
    // compile-time default — applyTheme() below re-applies to them too.
    applyTheme (p.uiThemeId);

    setLookAndFeel (&retroLookAndFeel);

    titleLabel.setText ("J.J.BREEZE", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions ("Avenir Next Condensed", 24.0f, juce::Font::bold))
                             .withExtraKerningFactor (0.05f));
    titleLabel.setColour (juce::Label::textColourId, currentTheme->textLight);
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    versionLabel.setText ("v" JucePlugin_VersionString, juce::dontSendNotification);
    versionLabel.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::plain)).withExtraKerningFactor (0.03f));
    versionLabel.setColour (juce::Label::textColourId, currentTheme->textMuted.withAlpha (0.55f));
    versionLabel.setJustificationType (juce::Justification::centred);
    versionLabel.setTooltip (JucePlugin_Name " " JucePlugin_VersionString);
    addAndMakeVisible (versionLabel);

    // Theme picker - switches the whole panel's colourway; see applyTheme().
    themeBox.setTooltip ("Change the panel's colourway.");
    for (auto& t : GearPalette::allThemes())
        themeBox.addItem (t.displayName, themeBox.getNumItems() + 1);
    themeBox.setSelectedId (1 + (int) std::distance (GearPalette::allThemes().begin(),
                                                       std::find_if (GearPalette::allThemes().begin(), GearPalette::allThemes().end(),
                                                                      [this] (const GearPalette::Theme& t) { return t.id == currentTheme->id; })),
                             juce::dontSendNotification);
    themeBox.onChange = [this]
    {
        const int idx = themeBox.getSelectedId() - 1;
        const auto& themes = GearPalette::allThemes();
        if (idx >= 0 && idx < (int) themes.size())
            applyTheme (themes[(size_t) idx].id);
    };
    addAndMakeVisible (themeBox);

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
    deleteButton.onClick = [this]
    {
        if (activeUserPresetName.isEmpty())
            return;

        processorRef.deleteUserPreset (activeUserPresetName);
        activeUserPresetName.clear();
        refreshPresetBox(); // falls back to showing the current factory program
    };
    addAndMakeVisible (deleteButton);

    // Undo/redo - mainly useful in the Standalone build, which (unlike
    // being hosted in a DAW) has no host-level undo of its own.
    undoButton.setTooltip ("Undo the last change.");
    redoButton.setTooltip ("Redo.");
    undoButton.onClick = [this] { processorRef.undoManager.undo(); };
    redoButton.onClick = [this] { processorRef.undoManager.redo(); };
    addAndMakeVisible (undoButton);
    addAndMakeVisible (redoButton);

    // Bypass - forces all three sections off (the untouched dry signal)
    // without touching any knob or which sections were individually on.
    bypassButton.setTooltip ("Bypass all processing - output the untouched dry signal without changing any knob or section state.");
    bypassButton.setClickingTogglesState (false);
    bypassButton.onClick = [this]
    {
        processorRef.setBypassed (! processorRef.isBypassed());
        updateBypassButtonColour();
    };
    addAndMakeVisible (bypassButton);
    updateBypassButtonColour();

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
    processorRef.uiThemeId = currentTheme->id;

    // Everything that draws itself with a `theme` pointer/member gets it
    // reassigned here; everything else just gets re-coloured to match
    // (titleLabel/versionLabel/presetBox/section labels/compare+bypass
    // buttons below), same as their original setup did.
    retroLookAndFeel.setTheme (*currentTheme);
    shiftToggle.setTheme (*currentTheme);
    vibratoToggle.setTheme (*currentTheme);
    warmthToggle.setTheme (*currentTheme);
    undoButton.setTheme (*currentTheme);
    redoButton.setTheme (*currentTheme);

    for (auto* knob : { &pitchLKnob, &pitchRKnob, &delayLKnob, &delayRKnob, &focusKnob, &mixKnob,
                         &vibratoRateKnob, &vibratoDepthKnob, &vibratoMixKnob,
                         &warmthToneKnob, &warmthDriveKnob, &warmthBodyKnob, &warmthMixKnob })
        knob->applyTheme (*currentTheme);

    titleLabel.setColour (juce::Label::textColourId, currentTheme->textLight);
    versionLabel.setColour (juce::Label::textColourId, currentTheme->textMuted.withAlpha (0.55f));
    for (auto* label : { &shiftSectionLabel, &vibratoSectionLabel, &warmthSectionLabel })
        label->setColour (juce::Label::textColourId, currentTheme->accent);

    presetBox.setColour (juce::ComboBox::backgroundColourId, currentTheme->ledBackground);
    presetBox.setColour (juce::ComboBox::textColourId, currentTheme->ledText);
    presetBox.setColour (juce::ComboBox::outlineColourId, currentTheme->metalDark);
    presetBox.setColour (juce::ComboBox::arrowColourId, currentTheme->accent);

    themeBox.setColour (juce::ComboBox::backgroundColourId, currentTheme->ledBackground);
    themeBox.setColour (juce::ComboBox::textColourId, currentTheme->ledText);
    themeBox.setColour (juce::ComboBox::outlineColourId, currentTheme->metalDark);
    themeBox.setColour (juce::ComboBox::arrowColourId, currentTheme->accent);

    updateCompareButtonColours();
    updateBypassButtonColour();
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

void JJBreezeAudioProcessorEditor::updateBypassButtonColour()
{
    const bool on = processorRef.isBypassed();
    bypassButton.setColour (juce::TextButton::buttonColourId, on ? currentTheme->accentDim : currentTheme->metalDark);
    bypassButton.setColour (juce::TextButton::textColourOffId, on ? currentTheme->accent : currentTheme->textMuted);
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
        updateBypassButtonColour();
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

void JJBreezeAudioProcessorEditor::drawScrew (juce::Graphics& g, juce::Point<float> centre) const
{
    constexpr float r = 8.0f;
    const auto bounds = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre);
    juce::ColourGradient grad (currentTheme->metalLight, bounds.getX(), bounds.getY(),
                                currentTheme->metalDark, bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (bounds);
    g.setColour (currentTheme->chassisBottom.withAlpha (0.8f));
    g.drawEllipse (bounds, 1.4f);
    g.drawLine ({ centre.translated (-5.0f, 2.6f), centre.translated (5.0f, -2.6f) }, 1.8f);
}

// Shared with resized() so panels, rules and knob rows all land in the same place.
static constexpr int headerHeight = 112; // title + preset/save/delete row + undo/bypass/A-B row
static constexpr int outerPadding = 20; // horizontal margin
static constexpr int topPadding = 12;
static constexpr int bottomPadding = 36; // extra clearance so the bottom corner screws stay visible
static constexpr int sectionLabelHeight = 26;
static constexpr int columnGap = 16; // horizontal gap between the Shift/Vibrato/Warmth columns
static constexpr int toggleWidth = 34;
static constexpr int numColumns = 3;
static constexpr int numColumnGaps = 2; // gaps: Shift-Vibrato, Vibrato-Warmth
static constexpr int maxKnobRows = 2; // the most any one column needs (Shift and Warmth both use 2)

void JJBreezeAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

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

    // Header strip with a machined seam underneath (a light line over a
    // dark one, like a panel edge catching the light).
    auto header = bounds.removeFromTop ((float) headerHeight);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRect (header);
    g.setColour (currentTheme->chassisBottom);
    g.drawHorizontalLine ((int) header.getBottom(), 0.0f, bounds.getWidth());
    g.setColour (currentTheme->metalLight.withAlpha (0.15f));
    g.drawHorizontalLine ((int) header.getBottom() - 1, 0.0f, bounds.getWidth());

    // Section panels - recessed metal cards that visually group each knob
    // row (or, when the section's toggle is off, just its collapsed
    // header bar).
    auto drawSection = [&] (const juce::Label& label, const juce::Rectangle<int>& panelBounds)
    {
        auto pf = panelBounds.toFloat();
        g.setColour (currentTheme->chassisBottom.withAlpha (0.6f));
        g.fillRoundedRectangle (pf.translated (0.0f, 2.0f), 8.0f);
        g.setColour (currentTheme->panelFill);
        g.fillRoundedRectangle (pf, 8.0f);
        g.setColour (currentTheme->chassisBottom.withAlpha (0.9f));
        g.drawRoundedRectangle (pf, 8.0f, 1.0f);
        g.setColour (currentTheme->metalLight.withAlpha (0.1f));
        g.drawLine (pf.getX() + 10.0f, pf.getY() + 1.0f, pf.getRight() - 10.0f, pf.getY() + 1.0f, 1.0f);

        g.setColour (currentTheme->accent.withAlpha (0.5f));
        g.drawHorizontalLine (label.getBottom() - 1, (float) label.getX(), (float) panelBounds.getRight());
    };

    drawSection (shiftSectionLabel, shiftPanelBounds);
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

    // Full header width for the centred title - the power LED is drawn
    // separately (in paint()) off to the left and doesn't need room carved
    // out of the text layout.
    auto header = area.removeFromTop (headerHeight).reduced (18, 10);
    titleLabel.setBounds (header.removeFromTop (26));
    header.removeFromTop (8); // gap before the preset row

    // Preset picker (left, flexible width) plus Save/Delete (right, fixed
    // width) share one row.
    auto presetRow = header.removeFromTop (26);
    deleteButton.setBounds (presetRow.removeFromRight (34));
    presetRow.removeFromRight (4);
    saveButton.setBounds (presetRow.removeFromRight (46));
    presetRow.removeFromRight (8);
    presetBox.setBounds (presetRow);

    header.removeFromTop (6); // gap before the actions row

    // Undo/redo (left) and bypass + A/B compare (right) share the last row.
    auto actionsRow = header;
    undoButton.setBounds (actionsRow.removeFromLeft (30));
    actionsRow.removeFromLeft (4);
    redoButton.setBounds (actionsRow.removeFromLeft (30));

    compareBButton.setBounds (actionsRow.removeFromRight (28));
    actionsRow.removeFromRight (4);
    compareAButton.setBounds (actionsRow.removeFromRight (28));
    actionsRow.removeFromRight (10);
    bypassButton.setBounds (actionsRow.removeFromRight (64));

    // Theme picker takes whatever's left in the middle of the row.
    themeBox.setBounds (actionsRow.withSizeKeepingCentre (juce::jmin (100, actionsRow.getWidth()), 22));

    area.removeFromTop (8); // gap before the knob sections
    area.removeFromLeft (outerPadding);
    area.removeFromRight (outerPadding);
    area.removeFromTop (topPadding);
    area.removeFromBottom (bottomPadding);

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
                              bool on, juce::Rectangle<int>& panelBounds,
                              std::initializer_list<std::initializer_list<LabelledKnob*>> rows)
    {
        auto fullLabelRow = column.removeFromTop (sectionLabelHeight);
        auto labelRow = fullLabelRow;
        toggle.setBounds (labelRow.removeFromRight (toggleWidth).reduced (0, 5));
        labelRow.removeFromRight (6);
        sectionLabel.setBounds (labelRow);

        for (auto& row : rows)
            for (auto* knob : row)
                knob->setVisible (on);

        if (! on)
        {
            panelBounds = fullLabelRow.expanded (6, 4);
            return;
        }

        auto usedRows = column;
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
        usedRows.setHeight (rowHeight * (int) rows.size());
        panelBounds = fullLabelRow.getUnion (usedRows).expanded (6, 4);
    };

    auto shiftColumn = area.removeFromLeft (columnWidth);
    area.removeFromLeft (columnGap);
    auto vibratoColumn = area.removeFromLeft (columnWidth);
    area.removeFromLeft (columnGap);
    auto warmthColumn = area; // takes whatever's left, absorbing rounding remainder

    layoutColumn (shiftColumn, shiftToggle, shiftSectionLabel, shiftOn, shiftPanelBounds,
                  { { &pitchLKnob, &pitchRKnob, &focusKnob }, { &delayLKnob, &delayRKnob, &mixKnob } });

    layoutColumn (vibratoColumn, vibratoToggle, vibratoSectionLabel, vibratoOn, vibratoPanelBounds,
                  { { &vibratoRateKnob, &vibratoDepthKnob, &vibratoMixKnob } });

    layoutColumn (warmthColumn, warmthToggle, warmthSectionLabel, warmthOn, warmthPanelBounds,
                  { { &warmthToneKnob, &warmthDriveKnob }, { &warmthBodyKnob, &warmthMixKnob } });

    // Version readout, centred in the bottom margin between the two bottom
    // corner screws — computed from the untouched full bounds rather than
    // the already-consumed `area`, same as drawScrew() in paint().
    {
        const auto full = getLocalBounds();
        constexpr int stripHeight = 14;
        constexpr int sideClearance = 50; // clears both bottom corner screws
        versionLabel.setBounds (full.getX() + sideClearance, full.getBottom() - stripHeight - 8,
                                 full.getWidth() - sideClearance * 2, stripHeight);
    }
}
