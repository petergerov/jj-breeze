#pragma once

#include <vector>
#include <algorithm>
#include <cmath>

/**
    A short mono slapback echo — a single, distinct repeat (or a few, with
    feedback) of the input, damped in the feedback path for a warm, vintage
    tape-echo character rather than a bright digital repeat.

    This is deliberately separate from PitchShifter/ModulatedDelay: those
    exist to make the source *wider*, while a slapback is meant to stay
    centered/mono and be heard as a discrete echo, not a width effect. The
    caller feeds it a mono-summed input and adds its output equally to both
    output channels.
*/
class SlapbackDelay
{
public:
    void prepare (double sampleRateIn)
    {
        sampleRate = sampleRateIn;
        maxDelaySamples = std::max (64, (int) std::round (sampleRate * 0.35)); // 350 ms headroom
        buffer.assign ((size_t) maxDelaySamples, 0.0f);
        writePos = 0;
        dampState = 0.0f;
        dampCoeff = (float) (1.0 - std::exp (-2.0 * M_PI * dampCutoffHz / sampleRate));
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        dampState = 0.0f;
    }

    void setTimeMs (float ms)
    {
        delaySamples = std::clamp ((float) (ms * 0.001 * sampleRate), 1.0f, (float) (maxDelaySamples - 2));
    }

    /** 0..1 — how much of the (damped) repeat feeds back in for further repeats. */
    void setFeedback (float fb) { feedback = std::clamp (fb, 0.0f, 0.9f); }

    float processSample (float x) noexcept
    {
        double idx = (double) writePos - delaySamples;
        while (idx < 0.0)
            idx += maxDelaySamples;

        const int i0 = (int) idx;
        const int i1 = (i0 + 1) % maxDelaySamples;
        const float frac = (float) (idx - i0);
        const float tap = buffer[(size_t) i0] + frac * (buffer[(size_t) i1] - buffer[(size_t) i0]);

        dampState += dampCoeff * (tap - dampState); // one-pole lowpass -> warm, damped repeats

        buffer[(size_t) writePos] = x + dampState * feedback;
        writePos = (writePos + 1) % maxDelaySamples;

        return tap;
    }

private:
    static constexpr float dampCutoffHz = 3500.0f; // fixed; not exposed as a control

    std::vector<float> buffer;
    double sampleRate = 44100.0;
    int maxDelaySamples = 15435;
    int writePos = 0;
    float delaySamples = 4410.0f;
    float feedback = 0.0f;
    float dampState = 0.0f;
    float dampCoeff = 0.3f;
};
