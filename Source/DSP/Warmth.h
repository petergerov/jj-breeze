#pragma once

#include <juce_dsp/juce_dsp.h>

/** A single-channel "warmth" tone stage: an optional low-shelf boost (for
    chest/body fullness), a low-pass filter (for a darker, rounder top
    end), and soft (tanh) saturation for gentle tape-like harmonic warmth —
    in that order. One instance per channel; the caller blends the result
    against dry via its own Mix parameter, the same way the other effect
    sections do it in PluginProcessor::processBlock — this class only
    produces the fully "wet" tone-shaped sample. */
class WarmthStage
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        juce::dsp::ProcessSpec spec { sampleRate, 1, 1 };
        lowShelf.prepare (spec);
        filter.prepare (spec);
        updateLowShelf();
        reset();
    }

    void reset()
    {
        lowShelf.reset();
        filter.reset();
    }

    void setToneHz (float hz)
    {
        *filter.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, hz, 0.707f);
    }

    // amount01: 0 = filter only (transparent, no added harmonics), 1 = heavy soft-clip warmth.
    void setDrive (float amount01) { drive = amount01; }

    // amount01: 0 = no low-shelf boost, 1 = +6dB at a fixed 150Hz corner —
    // the chest/body fullness a pitched-down "dark" voice has that a
    // low-pass alone doesn't add (see the vocal_1/vocal_2 analysis in
    // README.md: the dark take's 80-160Hz band sat ~13dB higher, relative
    // to its own peak, than the normal take's).
    void setBodyAmount (float amount01)
    {
        if (std::abs (amount01 - bodyAmount) < 1.0e-4f)
            return;
        bodyAmount = amount01;
        updateLowShelf();
    }

    float processSample (float x)
    {
        const float shelved = lowShelf.processSample (x);
        const float filtered = filter.processSample (shelved);

        if (drive < 1.0e-4f)
            return filtered;

        // Soft (tanh) saturation, pre-gained by drive and renormalised so
        // unity-level input stays close to unity output at any drive
        // setting — more drive means more harmonic warmth, not more level.
        const float k = drive * 10.0f;
        return std::tanh (filtered * k) / std::tanh (k);
    }

private:
    void updateLowShelf()
    {
        static constexpr float bodyHz = 150.0f;
        static constexpr float maxBoostDb = 6.0f;
        *lowShelf.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
            sampleRate, bodyHz, 0.707f, juce::Decibels::decibelsToGain (bodyAmount * maxBoostDb));
    }

    double sampleRate = 44100.0;
    juce::dsp::IIR::Filter<float> lowShelf;
    juce::dsp::IIR::Filter<float> filter;
    float drive = 0.0f;
    float bodyAmount = 0.0f;
};
