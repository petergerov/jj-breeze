#pragma once

#include <juce_dsp/juce_dsp.h>

/** A single-channel "warmth" tone stage: a low-pass filter (for a darker,
    rounder top end) followed by soft (tanh) saturation for gentle
    tape-like harmonic warmth. One instance per channel; the caller blends
    the result against dry via its own Mix parameter, the same way the
    other effect sections do it in PluginProcessor::processBlock — this
    class only produces the fully "wet" tone-shaped sample. */
class WarmthStage
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        juce::dsp::ProcessSpec spec { sampleRate, 1, 1 };
        filter.prepare (spec);
        reset();
    }

    void reset() { filter.reset(); }

    void setToneHz (float hz)
    {
        *filter.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, hz, 0.707f);
    }

    // amount01: 0 = filter only (transparent, no added harmonics), 1 = heavy soft-clip warmth.
    void setDrive (float amount01) { drive = amount01; }

    float processSample (float x)
    {
        const float filtered = filter.processSample (x);

        if (drive < 1.0e-4f)
            return filtered;

        // Soft (tanh) saturation, pre-gained by drive and renormalised so
        // unity-level input stays close to unity output at any drive
        // setting — more drive means more harmonic warmth, not more level.
        const float k = drive * 10.0f;
        return std::tanh (filtered * k) / std::tanh (k);
    }

private:
    double sampleRate = 44100.0;
    juce::dsp::IIR::Filter<float> filter;
    float drive = 0.0f;
};
