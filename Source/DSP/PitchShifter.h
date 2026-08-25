#pragma once

#include <vector>
#include <algorithm>
#include <cmath>

/**
    A small, artifact-light pitch shifter for micro-detune ("microshift") effects.

    This is the classic dual-tap crossfaded delay-line pitch shifter: two read
    pointers trail the write pointer by a delay that ramps linearly (up or down
    depending on the desired pitch ratio), each weighted by a Hann window that
    is zero exactly when that tap wraps around. The two taps are offset by half
    a grain so their Hann windows sum to a constant 1.0 (50%-overlap COLA
    property), which is what keeps the output level steady and click-free as
    each tap resets.

    It is tuned for *small* shifts (a few cents to a semitone or so) — the use
    case this plugin needs — rather than being a general-purpose harmonizer.
*/
class PitchShifter
{
public:
    void prepare (double sampleRateIn, float grainLengthMs = 35.0f)
    {
        sampleRate = sampleRateIn;
        grainLenSamples = std::max (32, (int) std::round (sampleRate * grainLengthMs / 1000.0));
        bufferSize = grainLenSamples * 4;
        buffer.assign ((size_t) bufferSize, 0.0f);
        writePos = 0;
        delay1 = 0.0;
        delay2 = grainLenSamples * 0.5;
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        delay1 = 0.0;
        delay2 = grainLenSamples * 0.5;
    }

    /** Sets the target pitch shift in cents (100 cents = 1 semitone). Positive shifts up. */
    void setShiftCents (float cents)
    {
        shiftRatio = std::pow (2.0f, cents / 1200.0f);
    }

    float processSample (float x) noexcept
    {
        buffer[(size_t) writePos] = x;

        const float y = readTap (delay1) * windowFor (delay1)
                       + readTap (delay2) * windowFor (delay2);

        advance (delay1);
        advance (delay2);

        writePos = (writePos + 1) % bufferSize;
        return y;
    }

private:
    void advance (double& d) const noexcept
    {
        d += (1.0 - (double) shiftRatio);
        if (d < 0.0)
            d += grainLenSamples;
        else if (d >= (double) grainLenSamples)
            d -= grainLenSamples;
    }

    float windowFor (double d) const noexcept
    {
        const double phase = d / grainLenSamples;
        return (float) (0.5 - 0.5 * std::cos (2.0 * M_PI * phase));
    }

    float readTap (double d) const noexcept
    {
        double idx = (double) writePos - d;
        while (idx < 0.0)
            idx += bufferSize;
        while (idx >= (double) bufferSize)
            idx -= bufferSize;

        const int i0 = (int) idx;
        const int i1 = (i0 + 1) % bufferSize;
        const float frac = (float) (idx - i0);
        return buffer[(size_t) i0] + frac * (buffer[(size_t) i1] - buffer[(size_t) i0]);
    }

    std::vector<float> buffer;
    double sampleRate = 44100.0;
    int grainLenSamples = 1024;
    int bufferSize = 4096;
    int writePos = 0;
    double delay1 = 0.0, delay2 = 0.0;
    float shiftRatio = 1.0f;
};
