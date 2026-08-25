#pragma once

#include <array>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "DSP/PitchShifter.h"
#include "DSP/ModulatedDelay.h"
#include "DSP/SlapbackDelay.h"

namespace ParamIDs
{
    static const juce::String pitchL       = "pitchL";       // cents, bipolar
    static const juce::String pitchR       = "pitchR";       // cents, bipolar
    static const juce::String delayL       = "delayL";       // ms
    static const juce::String delayR       = "delayR";       // ms
    static const juce::String focus        = "focus";        // Hz, crossover point (below = always dry)
    static const juce::String mix          = "mix";          // 0..1
    static const juce::String slapTime     = "slapTime";     // ms
    static const juce::String slapFeedback = "slapFeedback"; // 0..1
    static const juce::String slapMix      = "slapMix";      // 0..1
    static const juce::String vibratoRate  = "vibratoRate";  // Hz
    static const juce::String vibratoDepth = "vibratoDepth"; // ms
    static const juce::String vibratoMix   = "vibratoMix";   // 0..1
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

    juce::AudioProcessorValueTreeState apvts;

    // Post-processing output peak (linear, 0..~1+), updated once per block
    // on the audio thread and read by the editor's VU meter on a timer —
    // relaxed ordering is fine, it's a display value, not a control signal.
    std::atomic<float> outputLevel { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Factory presets: a name plus a value for every ParamIDs entry, in the
    // same order createParameterLayout() adds them. Index 0 ("Default") is
    // the plugin's normal default sound (wide microshift, no slapback);
    // later entries are alternate starting points selectable from the
    // host's built-in preset menu.
    struct Preset
    {
        const char* name;
        float pitchL, pitchR, delayL, delayR, focus, mix, slapTime, slapFeedback, slapMix,
              vibratoRate, vibratoDepth, vibratoMix;
    };

    static const std::array<Preset, 3>& getPresets();
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
            pitchShifter.prepare (sampleRate);
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
    SlapbackDelay slapback; // mono, centered — separate from the L/R width voices above

    // Vibrato: continuous LFO-driven pitch modulation via a swept short
    // delay (the classic "delay-based vibrato" technique — reuses
    // ModulatedDelay, just with a user-controlled rate/depth and a fixed
    // small center delay instead of the width path's fixed subtle wobble).
    // Independent of the widener above — for a slow swirling character like
    // JJ Cale's "Cajun Moon", not a static micro-detune.
    ModulatedDelay vibratoL, vibratoR;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JJBreezeAudioProcessor)
};
