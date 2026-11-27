#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "DSP/PitchShifter.h"
#include "DSP/ModulatedDelay.h"
#include "DSP/Warmth.h"

namespace ParamIDs
{
    static const juce::String pitchL       = "pitchL";       // cents, bipolar
    static const juce::String pitchR       = "pitchR";       // cents, bipolar
    static const juce::String delayL       = "delayL";       // ms
    static const juce::String delayR       = "delayR";       // ms
    static const juce::String focus        = "focus";        // Hz, crossover point (below = always dry)
    static const juce::String mix          = "mix";          // 0..1
    static const juce::String vibratoRate  = "vibratoRate";  // Hz
    static const juce::String vibratoDepth = "vibratoDepth"; // ms
    static const juce::String vibratoMix   = "vibratoMix";   // 0..1

    // Warmth: a final tone stage (low-shelf body boost + low-pass + soft
    // saturation) applied to the fully-summed output, for a darker/rounder
    // character no other section provides.
    static const juce::String warmthTone   = "warmthTone";   // Hz, low-pass cutoff
    static const juce::String warmthDrive  = "warmthDrive";  // 0..1, saturation amount
    static const juce::String warmthBody   = "warmthBody";   // 0..1, fixed-150Hz low-shelf boost amount
    static const juce::String warmthMix    = "warmthMix";    // 0..1

    // Per-section on/off — bypasses that section's contribution to the
    // output entirely (its DSP still runs, so re-enabling is click-free)
    // and drives the UI's lit toggle + collapsed/expanded layout.
    static const juce::String shiftOn    = "shiftOn";
    static const juce::String vibratoOn  = "vibratoOn";
    static const juce::String warmthOn   = "warmthOn";

    // Every parameter ID above, in a fixed order — for code that needs to
    // iterate the whole patch generically (the editor's A/B compare
    // snapshot) rather than naming each one.
    static const std::array<juce::String, 16> all {
        pitchL, pitchR, delayL, delayR, focus, mix,
        vibratoRate, vibratoDepth, vibratoMix,
        warmthTone, warmthDrive, warmthBody, warmthMix,
        shiftOn, vibratoOn, warmthOn
    };
}

class JJBreezeAudioProcessor : public juce::AudioProcessor
{
public:
    JJBreezeAudioProcessor();
    ~JJBreezeAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.15; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Declared before apvts so it exists in time to be handed to apvts's
    // constructor below (member init order follows declaration order).
    // Gives every apvts-managed parameter real undo/redo — mainly for the
    // Standalone build, which (unlike being hosted in a DAW) has no
    // host-level undo of its own. Editor interactions call
    // beginNewTransaction() at the start of each logical action (a knob
    // drag, a preset load, an A/B recall) so that whole action undoes as
    // one step rather than every intermediate value.
    juce::UndoManager undoManager;

    juce::AudioProcessorValueTreeState apvts;

    // A/B compare: lets the editor snapshot the full patch into slot 0
    // ("A") or 1 ("B") and recall it later, so a user can quickly flip
    // between two variations while dialing in a sound. Editor-only
    // convenience, not part of the saved session state — a fresh project
    // reload starts with just slot A, holding whatever was last recalled.
    void storeCompareSnapshot (int slot);
    void recallCompareSnapshot (int slot);
    bool hasCompareSnapshotStored (int slot) const { return slot >= 0 && slot < 2 && hasCompareSnapshot[(size_t) slot]; }
    int activeCompareSlot = 0;

    // Bypass: forces every section off — the untouched dry signal — without
    // disturbing any knob value or which sections were individually on, and
    // restores them on un-bypass. A convenience layered on top of the three
    // section on/off parameters, not a separate DSP path or parameter of
    // its own.
    void setBypassed (bool shouldBeBypassed);
    bool isBypassed() const { return bypassed; }
    // Called by the editor when a section's on/off changes while bypassed
    // (e.g. the user clicks a section's own toggle directly) — bypass no
    // longer describes the current state, so just stop claiming it does,
    // without touching any parameter.
    void clearBypassedFlag() { bypassed = false; }

    // User presets: full-patch snapshots saved to disk (same XML shape as
    // the session state), independent of the fixed factory preset list
    // below so the host-visible program count never changes at runtime —
    // VST3/AU hosts generally assume that list is fixed.
    juce::File getUserPresetsDirectory() const;
    juce::StringArray getUserPresetNames() const;
    // Saves the current knobs under `name` and makes it the loaded preset
    // (so the edited dot goes out).
    void saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::String& name);
    // Deleting the loaded user preset leaves the sound alone and falls back
    // to the last factory program as the "loaded" preset.
    void deleteUserPreset (const juce::String& name);

    // --- The loaded preset -------------------------------------------------
    // Factory presets are host programs; a user preset is loaded on top of
    // whichever program was current, and its name is held here (empty while
    // a factory preset is the loaded one). Both, plus the values the loaded
    // preset holds, travel with the session (see getStateInformation()), so
    // a reopened project still knows what it was built from.
    juce::String getCurrentUserPresetName() const;
    bool isUserPresetLoaded() const { return getCurrentUserPresetName().isNotEmpty(); }
    juce::String getCurrentPresetName() const;
    // Bumped whenever the loaded preset changes (load, save, delete, host
    // program change, session restore) — the editor polls it to know when
    // to re-sync its picker.
    int getPresetChangeCount() const { return presetChangeCount.load(); }

    // The edited dot: true once any knob or switch has moved away from what
    // the loaded preset holds. Compared relative to each parameter's range
    // so a 20-10000 Hz crossover and a 0-8 ms depth knob need a comparable
    // nudge to count.
    bool isPresetModified() const;
    // Puts every knob back to how the loaded preset was loaded (or last
    // saved), as one undo step.
    void revertToLoadedPreset();
    // "Save": writes the knobs back into the loaded user preset under its
    // own name. False when a factory preset is loaded.
    bool overwriteCurrentUserPreset();

    // Factory preset menu metadata. Programs are listed to the host in
    // getPresets() order; the editor groups them by category instead.
    static const juce::StringArray& getPresetCategories();
    juce::String getProgramCategory (int index) const;
    juce::String getProgramSummary (int index) const;
    // What a new instance starts on: JJ Moon, the sound the retired
    // "Default" preset gave. Index 0, so a host recalling program 0 (as
    // projects saved with "Default" do) still gets the same sound.
    static constexpr int initialProgram = 0;

    // --- Pitch/Delay link switches ------------------------------------------
    // Whether the editor moves Pitch L/R (and Delay L/R) together. Panel
    // state rather than parameters: saved with the host project, not in
    // presets, and not automatable.
    std::atomic<bool> linkPitch { false }, linkDelay { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::array<float, ParamIDs::all.size()> compareSnapshots[2] {};
    bool hasCompareSnapshot[2] { false, false };

    bool bypassed = false;
    bool bypassSavedShiftOn = true, bypassSavedVibratoOn = false, bypassSavedWarmthOn = false;

    // Factory presets: a name, a menu category and one-line summary, plus a
    // value for every ParamIDs entry, in the same order createParameterLayout()
    // adds them.
    struct Preset
    {
        const char* name;
        const char* category;
        const char* summary;
        float pitchL, pitchR, delayL, delayR, focus, mix,
              vibratoRate, vibratoDepth, vibratoMix,
              warmthTone, warmthDrive, warmthBody, warmthMix;
        bool shiftOn, vibratoOn, warmthOn;
    };

    // Plain (denormalised) values for every ParamIDs entry, in ParamIDs::all
    // order — what the edited dot compares against.
    using PatchValues = std::array<float, ParamIDs::all.size()>;

    static const std::array<Preset, 18>& getPresets();
    static PatchValues valuesOf (const Preset&);
    PatchValues captureValues() const;
    void applyValues (const PatchValues&);
    // A user preset file's values, falling back to `fallback` for any
    // parameter the file doesn't mention.
    PatchValues readUserPresetValues (const juce::String& name, const PatchValues& fallback) const;
    juce::File userPresetFile (const juce::String& name) const;

    void applyPreset (int index);
    void setLoadedPreset (const juce::String& userPresetName, const PatchValues& values);

    int currentProgram = initialProgram;

    // Guards currentUserPresetName/loadedValues: hosts may restore state or
    // change programs off the message thread while the editor polls them.
    mutable juce::CriticalSection presetLock;
    juce::String currentUserPresetName;
    PatchValues loadedValues {};
    std::atomic<int> presetChangeCount { 0 };

    struct ChannelVoice
    {
        PitchShifter pitchShifter;
        ModulatedDelay delay;

        // Focus is a 2-band crossover (like Soundtoys MicroShift), not a
        // simple low-cut on the wet signal: everything below the crossover
        // point stays untouched/dry, and only the band above it is fed
        // through the pitch shifter + delay.
        juce::dsp::IIR::Filter<float> lowBandFilter;
        juce::dsp::IIR::Filter<float> highBandFilter;

        void prepare (double sampleRate)
        {
            // Longer than PitchShifter.h's original 35ms default: Pitch L/R
            // now goes up to +-1200 cents (see createParameterLayout()), and
            // a longer grain keeps the tap-crossfade rate down at that
            // shift size.
            pitchShifter.prepare (sampleRate, 70.0f);
            delay.prepare (sampleRate);
            lowBandFilter.reset();
            highBandFilter.reset();
        }

        void reset()
        {
            pitchShifter.reset();
            delay.reset();
            lowBandFilter.reset();
            highBandFilter.reset();
        }
    };

    ChannelVoice leftVoice, rightVoice;

    // Vibrato: continuous LFO-driven pitch modulation via a swept short
    // delay (the classic "delay-based vibrato" technique — reuses
    // ModulatedDelay, just with a user-controlled rate/depth and a fixed
    // small center delay instead of the width path's fixed subtle wobble).
    // Independent of the widener above — for a slow swirling character like
    // JJ Cale's "Cajun Moon", not a static micro-detune.
    ModulatedDelay vibratoL, vibratoR;

    // Warmth: low-pass + soft saturation applied to the final summed
    // output (see ParamIDs::warmthTone/warmthDrive/warmthMix above).
    WarmthStage warmthL, warmthR;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JJBreezeAudioProcessor)
};
