#pragma once

#include <vector>
#include <algorithm>
#include <cmath>

/**
    A short delay line with a slow, gentle built-in LFO wobble on top of the
    user-set base delay. This is the "time-varying delay" half of the
    microshift recipe: it adds a bit of organic chorus-like movement on top of
    the static pitch shift so the effect doesn't sound frozen/static.

    The LFO rate/depth are fixed (not user parameters) to keep the plugin's
    control surface small, per the "easy to use" brief.
*/
class ModulatedDelay
{
public:
    void prepare (double sampleRateIn)
    {
        sampleRate = sampleRateIn;
        // 300 ms headroom: Shift's per-channel Delay L/R now goes up to
        // 250ms (wide enough to cover the old dedicated Slapback section's
        // range, folded into Shift instead of kept separate — see
        // PluginProcessor.cpp), plus a little extra so the base delay can
        // still be nudged by the LFO wobble near the top of its range.
        // Vibrato's small swept delay reuses the same class with plenty of
        // room to spare.
        maxDelaySamples = std::max (64, (int) std::round (sampleRate * 0.3));
        buffer.assign ((size_t) maxDelaySamples, 0.0f);
        writePos = 0;
        lfoPhase = 0.0f;
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        lfoPhase = 0.0f;
    }

    /** Starting phase for the LFO in [0, 1); use different values per channel for stereo movement. */
    void setLfoStartPhase (float phase01) { lfoPhase = (float) (phase01 * 2.0 * M_PI); }

    void setBaseDelayMs (float ms)
    {
        baseDelaySamples = (float) (ms * 0.001 * sampleRate);
    }

    float processSample (float x) noexcept
    {
        buffer[(size_t) writePos] = x;

        const float modDepthSamples = (float) (lfoDepthMs * 0.001 * sampleRate);
        const float lfo = std::sin (lfoPhase);
        lfoPhase += (float) (2.0 * M_PI * lfoRateHz / sampleRate);
        if (lfoPhase > (float) (2.0 * M_PI))
            lfoPhase -= (float) (2.0 * M_PI);

        const float delaySamples = std::clamp (baseDelaySamples + lfo * modDepthSamples,
                                                1.0f, (float) (maxDelaySamples - 2));

        double idx = (double) writePos - delaySamples;
        while (idx < 0.0)
            idx += maxDelaySamples;

        const int i0 = (int) idx;
        const int i1 = (i0 + 1) % maxDelaySamples;
        const float frac = (float) (idx - i0);
        const float y = buffer[(size_t) i0] + frac * (buffer[(size_t) i1] - buffer[(size_t) i0]);

        writePos = (writePos + 1) % maxDelaySamples;
        return y;
    }

    float lfoRateHz = 0.13f;
    float lfoDepthMs = 1.5f;

private:
    std::vector<float> buffer;
    double sampleRate = 44100.0;
    int maxDelaySamples = 4410;
    int writePos = 0;
    float baseDelaySamples = 0.0f;
    float lfoPhase = 0.0f;
};
