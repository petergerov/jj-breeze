#include "PluginProcessor.h"
#include "PluginEditor.h"

JJBreezeAudioProcessor::JJBreezeAudioProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout JJBreezeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Range widened from the original +-50 cents (still the default-preset
    // territory, and still a skewed range so that fine micro-detune territory
    // isn't crammed into a sliver of the knob) to +-1200 (a full octave) so
    // Shift's independent per-channel Pitch *and* Delay controls can also
    // dial in a big, semitone-scale drop directly — an alternative to the
    // single-amount, both-channels-together Drop section (see PitchDrop.h).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::pitchL, 1 }, "Pitch L",
        juce::NormalisableRange<float> (-1200.0f, 1200.0f, 0.01f, 0.4f, true), 12.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("cents")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " ct"; })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::pitchR, 1 }, "Pitch R",
        juce::NormalisableRange<float> (-1200.0f, 1200.0f, 0.01f, 0.4f, true), -12.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("cents")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " ct"; })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::delayL, 1 }, "Delay L",
        juce::NormalisableRange<float> (0.0f, 40.0f, 0.01f), 15.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " ms"; })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::delayR, 1 }, "Delay R",
        juce::NormalisableRange<float> (0.0f, 40.0f, 0.01f), 15.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " ms"; })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::focus, 1 }, "Focus",
        juce::NormalisableRange<float> (20.0f, 10000.0f, 1.0f, 0.25f), 150.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("Hz")
            .withStringFromValueFunction ([] (float v, int) { return juce::String ((int) v) + " Hz"; })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::mix, 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String ((int) v) + " %"; })));

    // Slapback echo: a separate, mono/centered discrete repeat, independent
    // of the L/R width controls above. Off by default (Slap Mix = 0%) so it
    // doesn't change the existing sound unless deliberately dialed in.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::slapTime, 1 }, "Slap Time",
        juce::NormalisableRange<float> (30.0f, 300.0f, 0.1f), 110.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float v, int) { return juce::String ((int) v) + " ms"; })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::slapFeedback, 1 }, "Slap Feedback",
        juce::NormalisableRange<float> (0.0f, 70.0f, 0.1f), 15.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String ((int) v) + " %"; })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::slapMix, 1 }, "Slap Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String ((int) v) + " %"; })));

    // Vibrato: continuous LFO-driven pitch wobble (delay-based, like a
    // Uni-Vibe's vibrato mode), independent of the static Width detune
    // above. Off by default (Vibrato Mix = 0%).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::vibratoRate, 1 }, "Vibrato Rate",
        juce::NormalisableRange<float> (0.1f, 8.0f, 0.01f, 0.4f), 1.2f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("Hz")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + " Hz"; })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::vibratoDepth, 1 }, "Vibrato Depth",
        juce::NormalisableRange<float> (0.0f, 8.0f, 0.01f), 3.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " ms"; })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::vibratoMix, 1 }, "Vibrato Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String ((int) v) + " %"; })));

    // Drop: a static, semitone-scale pitch shift on the voice itself —
    // distinct from Shift's ±50-cent micro-detune widener, and the main
    // driver of the "JJ Dark Vocal" character (see README.md). Off by
    // default (Drop Mix = 0%), like Slapback/Vibrato/Warmth.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::dropAmount, 1 }, "Drop Amount",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), -3.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("st")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " st"; })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::dropMix, 1 }, "Drop Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String ((int) v) + " %"; })));

    // Warmth: a final tone stage (low-shelf body boost + low-pass + soft
    // saturation) on the fully summed output — for a darker, rounder
    // character none of the other sections provide. Off by default
    // (Warmth Mix = 0%), like Slapback, Vibrato and Drop.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::warmthTone, 1 }, "Warmth Tone",
        juce::NormalisableRange<float> (500.0f, 12000.0f, 1.0f, 0.3f), 3500.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("Hz")
            .withStringFromValueFunction ([] (float v, int) { return juce::String ((int) v) + " Hz"; })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::warmthDrive, 1 }, "Warmth Drive",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 20.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String ((int) v) + " %"; })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::warmthBody, 1 }, "Warmth Body",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String ((int) v) + " %"; })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::warmthMix, 1 }, "Warmth Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String ((int) v) + " %"; })));

    // Per-section on/off, one per UI section — bypasses that section's
    // contribution without touching its knob values, so re-enabling it
    // brings back exactly what was dialed in.
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::shiftOn, 1 }, "Shift On", true));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::slapOn, 1 }, "Slapback On", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::vibratoOn, 1 }, "Vibrato On", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::dropOn, 1 }, "Drop On", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::warmthOn, 1 }, "Warmth On", false));

    return { params.begin(), params.end() };
}

void JJBreezeAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;

    leftVoice.prepare (sampleRate);
    rightVoice.prepare (sampleRate);

    // Offset the two channels' delay-modulation LFOs so the width has some
    // independent left/right movement rather than moving in lockstep.
    leftVoice.delay.setLfoStartPhase (0.0f);
    rightVoice.delay.setLfoStartPhase (0.5f);

    juce::dsp::ProcessSpec spec { sampleRate, 1, 1 };
    leftVoice.lowBandFilter.prepare (spec);
    leftVoice.highBandFilter.prepare (spec);
    rightVoice.lowBandFilter.prepare (spec);
    rightVoice.highBandFilter.prepare (spec);

    slapback.prepare (sampleRate);

    // Longer grain than the Shift widener's PitchShifter (70ms vs 35ms) —
    // Drop moves by whole semitones rather than cents, and the longer grain
    // keeps the tap-crossfade rate down at that shift size (see PitchDrop.h).
    dropL.prepare (sampleRate);
    dropR.prepare (sampleRate);

    vibratoL.prepare (sampleRate);
    vibratoR.prepare (sampleRate);
    // Fixed small center delay so the LFO-swept delay can move both up and
    // down without ever reaching zero; rate/depth are set from parameters
    // each block in processBlock().
    vibratoL.setBaseDelayMs (9.0f);
    vibratoR.setBaseDelayMs (9.0f);
    vibratoL.setLfoStartPhase (0.0f);
    vibratoR.setLfoStartPhase (0.5f);

    warmthL.prepare (sampleRate);
    warmthR.prepare (sampleRate);
}

void JJBreezeAudioProcessor::releaseResources()
{
    leftVoice.reset();
    rightVoice.reset();
    slapback.reset();
    dropL.reset();
    dropR.reset();
    vibratoL.reset();
    vibratoR.reset();
    warmthL.reset();
    warmthR.reset();
}

bool JJBreezeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();

    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != stereo)
        return false;

    return in == stereo || in == mono;
}

void JJBreezeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numOutChannels = getTotalNumOutputChannels();

    // Read control values once per block (these are slow, "macro" controls;
    // per-sample updates aren't needed and would just cost CPU).
    const float pitchLCents = apvts.getRawParameterValue (ParamIDs::pitchL)->load();
    const float pitchRCents = apvts.getRawParameterValue (ParamIDs::pitchR)->load();
    const float delayLMs    = apvts.getRawParameterValue (ParamIDs::delayL)->load();
    const float delayRMs    = apvts.getRawParameterValue (ParamIDs::delayR)->load();
    const float focusHz     = apvts.getRawParameterValue (ParamIDs::focus)->load();
    const float mix         = apvts.getRawParameterValue (ParamIDs::mix)->load() * 0.01f;
    const float slapTimeMs  = apvts.getRawParameterValue (ParamIDs::slapTime)->load();
    const float slapFeedbk  = apvts.getRawParameterValue (ParamIDs::slapFeedback)->load() * 0.01f;
    const float slapMixAmt  = apvts.getRawParameterValue (ParamIDs::slapMix)->load() * 0.01f;
    const float vibRateHz   = apvts.getRawParameterValue (ParamIDs::vibratoRate)->load();
    const float vibDepthMs  = apvts.getRawParameterValue (ParamIDs::vibratoDepth)->load();
    const float vibMixAmt   = apvts.getRawParameterValue (ParamIDs::vibratoMix)->load() * 0.01f;
    const float dropSemitones = apvts.getRawParameterValue (ParamIDs::dropAmount)->load();
    const float dropMixAmt    = apvts.getRawParameterValue (ParamIDs::dropMix)->load() * 0.01f;
    const float warmthToneHz  = apvts.getRawParameterValue (ParamIDs::warmthTone)->load();
    const float warmthDriveAmt = apvts.getRawParameterValue (ParamIDs::warmthDrive)->load() * 0.01f;
    const float warmthBodyAmt  = apvts.getRawParameterValue (ParamIDs::warmthBody)->load() * 0.01f;
    const float warmthMixAmt  = apvts.getRawParameterValue (ParamIDs::warmthMix)->load() * 0.01f;

    // Section on/off — each section's DSP still runs below (for click-free
    // re-enabling), only its contribution to the output is gated here.
    const bool shiftIsOn   = apvts.getRawParameterValue (ParamIDs::shiftOn)->load() > 0.5f;
    const bool slapIsOn    = apvts.getRawParameterValue (ParamIDs::slapOn)->load() > 0.5f;
    const bool vibratoIsOn = apvts.getRawParameterValue (ParamIDs::vibratoOn)->load() > 0.5f;
    const bool dropIsOn    = apvts.getRawParameterValue (ParamIDs::dropOn)->load() > 0.5f;
    const bool warmthIsOn  = apvts.getRawParameterValue (ParamIDs::warmthOn)->load() > 0.5f;

    leftVoice.pitchShifter.setShiftCents (pitchLCents);
    rightVoice.pitchShifter.setShiftCents (pitchRCents);
    leftVoice.delay.setBaseDelayMs (delayLMs);
    rightVoice.delay.setBaseDelayMs (delayRMs);

    // Focus crossover coefficients (shared shape for both channels).
    auto lowCoeffs  = juce::dsp::IIR::Coefficients<float>::makeLowPass  (currentSampleRate, focusHz);
    auto highCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, focusHz);
    *leftVoice.lowBandFilter.coefficients   = *lowCoeffs;
    *leftVoice.highBandFilter.coefficients  = *highCoeffs;
    *rightVoice.lowBandFilter.coefficients  = *lowCoeffs;
    *rightVoice.highBandFilter.coefficients = *highCoeffs;

    slapback.setTimeMs (slapTimeMs);
    slapback.setFeedback (slapFeedbk);

    dropL.setShiftSemitones (dropSemitones);
    dropR.setShiftSemitones (dropSemitones);

    vibratoL.lfoRateHz  = vibRateHz;
    vibratoL.lfoDepthMs = vibDepthMs;
    vibratoR.lfoRateHz  = vibRateHz;
    vibratoR.lfoDepthMs = vibDepthMs;

    warmthL.setToneHz (warmthToneHz);
    warmthR.setToneHz (warmthToneHz);
    warmthL.setDrive (warmthDriveAmt);
    warmthR.setDrive (warmthDriveAmt);
    warmthL.setBodyAmount (warmthBodyAmt);
    warmthR.setBodyAmount (warmthBodyAmt);

    // If the host feeds us a mono input duplicated across the stereo bus (or
    // an actual mono bus), both channels below already point at valid data
    // because we only support mono-in-stereo-out or stereo-in-stereo-out.
    float* left  = buffer.getWritePointer (0);
    float* right = numOutChannels > 1 ? buffer.getWritePointer (1) : buffer.getWritePointer (0);

    for (int n = 0; n < numSamples; ++n)
    {
        const float dryL = left[n];
        const float dryR = right[n];

        // Drop runs first, ahead of every other section: a static,
        // semitone-scale pitch shift on the voice itself (see PitchDrop.h),
        // crossfaded against the plain dry signal by Drop Mix. Everything
        // below treats the result — "voiceL/voiceR" — as its input, the
        // same way it previously treated dryL/dryR directly, so with Drop
        // off (the default) voiceL/voiceR are just dryL/dryR again.
        const float droppedL = dropL.processSample (dryL);
        const float droppedR = dropR.processSample (dryR);
        const float voiceL = dropIsOn ? dryL + dropMixAmt * (droppedL - dryL) : dryL;
        const float voiceR = dropIsOn ? dryR + dropMixAmt * (droppedR - dryR) : dryR;

        // Focus is a crossover, not a filter on the wet signal: everything
        // below the Focus point passes through untouched, and only the band
        // above it goes through the pitch shifter + delay (matches how
        // Soundtoys MicroShift's Focus control works).
        const float lowL  = leftVoice.lowBandFilter.processSample (voiceL);
        float highL = leftVoice.highBandFilter.processSample (voiceL);
        highL = leftVoice.pitchShifter.processSample (highL);
        highL = leftVoice.delay.processSample (highL);
        const float wetL = lowL + highL;

        const float lowR  = rightVoice.lowBandFilter.processSample (voiceR);
        float highR = rightVoice.highBandFilter.processSample (voiceR);
        highR = rightVoice.pitchShifter.processSample (highR);
        highR = rightVoice.delay.processSample (highR);
        const float wetR = lowR + highR;

        // Slapback is a separate, mono/centered echo — it's added on top of
        // the width-processed output rather than crossfaded by Mix, so it
        // reads as a distinct discrete repeat rather than part of the width.
        const float slapEcho = slapback.processSample (0.5f * (voiceL + voiceR)) * slapMixAmt;

        // Vibrato: continuous LFO-swept delay (pitch wobble), independent of
        // the static Width detune — the "Cajun Moon"-style swirl.
        const float vibL = vibratoL.processSample (voiceL);
        const float vibR = vibratoR.processSample (voiceR);

        // Width and Vibrato are two independent "lenses" on the same voice
        // signal, each crossfaded against that same baseline and then
        // summed (rather than sequentially crossfaded), so turning one up
        // doesn't eat into the other. With vibratoMix at 0 this reduces
        // exactly to the previous voice*(1-mix) + wet*mix formula.
        const float shiftMixL = shiftIsOn ? mix * (wetL - voiceL) : 0.0f;
        const float shiftMixR = shiftIsOn ? mix * (wetR - voiceR) : 0.0f;
        const float vibMixL   = vibratoIsOn ? vibMixAmt * (vibL - voiceL) : 0.0f;
        const float vibMixR   = vibratoIsOn ? vibMixAmt * (vibR - voiceR) : 0.0f;
        const float slapOut   = slapIsOn ? slapEcho : 0.0f;

        const float outL = voiceL + shiftMixL + vibMixL + slapOut;
        const float outR = voiceR + shiftMixR + vibMixR + slapOut;

        // Warmth is a final tone stage on the fully-summed output (low-pass
        // + soft saturation), not another independent "lens" on dry — it's
        // meant to darken/round off whatever the rest of the chain produced.
        const float warmL = warmthL.processSample (outL);
        const float warmR = warmthR.processSample (outR);
        const float warmthBlend = warmthIsOn ? warmthMixAmt : 0.0f;

        left[n]  = outL + warmthBlend * (warmL - outL);
        right[n] = outR + warmthBlend * (warmR - outR);
    }
}

juce::AudioProcessorEditor* JJBreezeAudioProcessor::createEditor()
{
    return new JJBreezeAudioProcessorEditor (*this);
}

const std::array<JJBreezeAudioProcessor::Preset, 4>& JJBreezeAudioProcessor::getPresets()
{
    // pitchL, pitchR, delayL, delayR, focus, mix, slapTime, slapFeedback, slapMix, vibratoRate, vibratoDepth, vibratoMix, dropAmount, dropMix, warmthTone, warmthDrive, warmthBody, warmthMix, shiftOn, slapOn, vibratoOn, dropOn, warmthOn
    static const std::array<Preset, 4> presets { {
        { "Default",       12.0f, -12.0f, 15.0f, 15.0f, 150.0f, 50.0f, 110.0f, 15.0f,  0.0f, 1.2f, 3.0f,  0.0f, -3.0f, 0.0f,  3500.0f, 20.0f, 0.0f, 0.0f, true,  false, false, false, false },

        // A laid-back, intimate vocal in the JJ Cale direction: the width
        // is turned way down (a few cents, low mix) rather than off, so
        // there's still some doubling glue, plus a single, low-feedback
        // slapback repeat instead of the wide microshift being the star.
        { "JJ Cale Vocal",  4.0f,  -4.0f,  8.0f, 10.0f, 300.0f, 18.0f, 100.0f, 12.0f, 20.0f, 1.2f, 3.0f,  0.0f, -3.0f, 0.0f,  3500.0f, 20.0f, 0.0f, 0.0f, true,  true,  false, false, false },

        // "Cajun Moon"-style warmth: retuned against an actual reference
        // recording (see example/cajunmoon_vocal.mp3) rather than guessed.
        // That analysis found (1) no discrete slapback echo at all, (2) no
        // strong deliberate vibrato — the measured pitch movement was just
        // natural vocal phrasing, not a steady ~1-2Hz wobble, and (3) the
        // one clear, dominant trait: a heavily rolled-off, dark/warm tone
        // (only ~1% of spectral energy above 2kHz). So width and slapback
        // stay off, vibrato is now a light touch rather than the main
        // event, and Warmth — not vibrato — is what actually carries the
        // "Cajun Moon" character here.
        { "Cajun Moon Vocal", 0.0f, 0.0f, 15.0f, 15.0f, 150.0f, 0.0f, 100.0f, 10.0f, 0.0f, 1.1f, 3.5f, 15.0f, -3.0f, 0.0f, 2800.0f, 25.0f, 0.0f, 70.0f, false, false, true, false, true },

        // "JJ Dark Vocal": built by comparing two takes of the same
        // performance (example/cajunmoon_vocal_vocal_1.mp3, the dark one,
        // vs _vocal_2.mp3, the normal one). Unlike the Cajun Moon analysis
        // above, overall top-end rolloff was nearly identical between the
        // two (~-6.2 dB/oct either way) — the dark take's defining trait
        // here was instead (1) a fundamental pitch ~3 semitones lower
        // (measured median F0 160Hz vs 193Hz) and (2) far more low-mid
        // "chest" body: its 80-160Hz band sat only ~3dB below its spectral
        // peak, vs ~16dB down in the normal take. So Drop does the actual
        // pitch-down (-3 semitones, full mix, ahead of everything else in
        // the chain), Warmth's new Body control restores the chest
        // fullness a pitch-down alone doesn't add, and width/slapback stay
        // off with the same light Cajun-Moon-style vibrato as a finishing
        // touch.
        { "JJ Dark Vocal", 0.0f, 0.0f, 15.0f, 15.0f, 150.0f, 0.0f, 100.0f, 10.0f, 0.0f, 1.1f, 3.5f, 15.0f, -3.0f, 100.0f, 2800.0f, 25.0f, 70.0f, 70.0f, false, false, true, true, true },
    } };
    return presets;
}

int JJBreezeAudioProcessor::getNumPrograms()
{
    return (int) getPresets().size();
}

const juce::String JJBreezeAudioProcessor::getProgramName (int index)
{
    const auto& presets = getPresets();
    if (index >= 0 && index < (int) presets.size())
        return presets[(size_t) index].name;
    return {};
}

void JJBreezeAudioProcessor::applyPreset (int index)
{
    const auto& presets = getPresets();
    if (index < 0 || index >= (int) presets.size())
        return;

    const auto& preset = presets[(size_t) index];

    auto setParam = [this] (const juce::String& paramID, float value)
    {
        if (auto* param = apvts.getParameter (paramID))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    setParam (ParamIDs::pitchL,       preset.pitchL);
    setParam (ParamIDs::pitchR,       preset.pitchR);
    setParam (ParamIDs::delayL,       preset.delayL);
    setParam (ParamIDs::delayR,       preset.delayR);
    setParam (ParamIDs::focus,        preset.focus);
    setParam (ParamIDs::mix,          preset.mix);
    setParam (ParamIDs::slapTime,     preset.slapTime);
    setParam (ParamIDs::slapFeedback, preset.slapFeedback);
    setParam (ParamIDs::slapMix,      preset.slapMix);
    setParam (ParamIDs::vibratoRate,  preset.vibratoRate);
    setParam (ParamIDs::vibratoDepth, preset.vibratoDepth);
    setParam (ParamIDs::vibratoMix,   preset.vibratoMix);
    setParam (ParamIDs::dropAmount,   preset.dropAmount);
    setParam (ParamIDs::dropMix,      preset.dropMix);
    setParam (ParamIDs::warmthTone,   preset.warmthTone);
    setParam (ParamIDs::warmthDrive,  preset.warmthDrive);
    setParam (ParamIDs::warmthBody,   preset.warmthBody);
    setParam (ParamIDs::warmthMix,    preset.warmthMix);

    setParam (ParamIDs::shiftOn,   preset.shiftOn   ? 1.0f : 0.0f);
    setParam (ParamIDs::slapOn,    preset.slapOn    ? 1.0f : 0.0f);
    setParam (ParamIDs::vibratoOn, preset.vibratoOn ? 1.0f : 0.0f);
    setParam (ParamIDs::dropOn,    preset.dropOn    ? 1.0f : 0.0f);
    setParam (ParamIDs::warmthOn,  preset.warmthOn  ? 1.0f : 0.0f);
}

void JJBreezeAudioProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;

    currentProgram = index;
    applyPreset (index);
}

void JJBreezeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
    }
}

void JJBreezeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JJBreezeAudioProcessor();
}
