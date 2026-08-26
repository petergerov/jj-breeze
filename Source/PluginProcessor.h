#pragma once

#include <array>

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

    // Generic full-patch snapshot/compare, normalized (0..1) per parameter
    // in ParamIDs::all order — used by the editor to remember what the
    // currently-selected preset actually contains, so it can tell (and show)
    // when the user has since tweaked something away from it.
    std::array<float, ParamIDs::all.size()> captureNormalizedSnapshot() const;
    bool matchesNormalizedSnapshot (const std::array<float, ParamIDs::all.size()>& snapshot) const;

    // User presets: full-patch snapshots saved to disk (same XML shape as
    // the session state), independent of the fixed 8-entry factory preset
    // list below so the host-visible program count never changes at
    // runtime — VST3/AU hosts generally assume that list is fixed.
    juce::File getUserPresetsDirectory() const;
    juce::StringArray getUserPresetNames() const;
    void saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::String& name);
    void deleteUserPreset (const juce::String& name);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::array<float, ParamIDs::all.size()> compareSnapshots[2] {};
    bool hasCompareSnapshot[2] { false, false };

    bool bypassed = false;
    bool bypassSavedShiftOn = true, bypassSavedVibratoOn = false, bypassSavedWarmthOn = false;

    // Factory presets: a name plus a value for every ParamIDs entry, in the
    // same order createParameterLayout() adds them. Index 0 ("Default") is
    // the plugin's normal default sound; later entries are alternate
    // starting points selectable from the host's built-in preset menu.
    struct Preset
    {
        const char* name;
        float pitchL, pitchR, delayL, delayR, focus, mix,
              vibratoRate, vibratoDepth, vibratoMix,
              warmthTone, warmthDrive, warmthBody, warmthMix;
        bool shiftOn, vibratoOn, warmthOn;
    };

    static const std::array<Preset, 9>& getPresets();
    void applyPreset (int index);
    int currentProgram = 0;

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
