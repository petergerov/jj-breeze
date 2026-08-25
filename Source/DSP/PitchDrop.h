#pragma once

#include <vector>
#include <algorithm>
#include <cmath>

/**
    A pitch shifter for a static, semitone-scale drop — the "make the voice
    itself darker/deeper" move behind the JJ Dark Vocal preset, as opposed
    to PitchShifter.h's micro-detune widener (which is explicitly tuned for
    shifts of a few cents to about a semitone).

    Same underlying technique as PitchShifter.h — two delay-line read taps
    trailing the write pointer, crossfaded with 50%-overlapping Hann windows
    so each tap's wraparound is inaudible — just with a longer default grain
    (70ms vs 35ms). A bigger shift ratio makes the taps wrap around faster
    (roughly |1 - ratio| * sampleRate / grainLength times per second); a
    longer grain slows that back down, trading a little transient smearing
    for less audible warble at shifts of a few semitones. This still isn't a
    formant-corrected shifter — a few-semitone drop will naturally also
    darken formants, which is a feature here, not a bug (see the analysis
    in README.md that motivated this class).
*/
class PitchDropShifter
{
public:
    void prepare (double sampleRateIn, float grainLengthMs = 70.0f)
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

    /** Sets the target pitch shift in semitones (12 = one octave up). Negative shifts down. */
    void setShiftSemitones (float semitones)
    {
        shiftRatio = std::pow (2.0f, semitones / 12.0f);
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
