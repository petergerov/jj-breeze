#include <cmath>
#include <limits>

#include "PluginProcessor.h"
#include "PluginEditor.h"

JJBreezeAudioProcessor::JJBreezeAudioProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "PARAMETERS", createParameterLayout())
{
    // A new instance starts on JJ Moon — the sound the retired "Default"
    // preset gave — rather than on the raw parameter defaults, so the
    // preset picker's name and the sound agree from the first launch.
    applyPreset (initialProgram);
    setLoadedPreset ({}, captureValues());
    undoManager.clearUndoHistory();

    // Slot A starts out as whatever the default patch is, so the A/B
    // compare toggle in the editor has something meaningful to flip back to
    // from the very first launch.
    storeCompareSnapshot (0);
}

juce::AudioProcessorValueTreeState::ParameterLayout JJBreezeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Range widened from the original +-50 cents (still the default-preset
    // territory, and still a skewed range so that fine micro-detune territory
    // isn't crammed into a sliver of the knob) to +-1200 (a full octave) so
    // Shift's independent per-channel Pitch *and* Delay controls can also
    // dial in a big, semitone-scale shift directly — both a subtle widener
    // and (with Focus turned down so the wet path covers the full band —
    // see point 3 in README.md) a deep/dark or bright-processed voice
    // effect are the same mechanism now. Default +300ct on both channels
    // matches the measured pitch gap between the two reference takes in
    // example/cajunmoon_vocal_vocal_1.mp3 vs _vocal_2.mp3 (see README.md).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::pitchL, 1 }, "Pitch L",
        juce::NormalisableRange<float> (-1200.0f, 1200.0f, 0.01f, 0.4f, true), 300.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("cents")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " ct"; })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::pitchR, 1 }, "Pitch R",
        juce::NormalisableRange<float> (-1200.0f, 1200.0f, 0.01f, 0.4f, true), 300.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("cents")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " ct"; })));

    // Range widened from the original 0-40ms (still the default-preset
    // territory, and still skewed toward the low end so subtle-width
    // territory isn't crammed into a sliver of the knob) to 0-250ms so
    // Shift's per-channel Delay can also reach classic slapback-echo
    // timing directly -- the same mechanism now covers both the subtle
    // width wobble and (with Mix and a low Focus, see the dedicated
    // Slapback-echo presets below) a discrete delayed repeat, once
    // handled by a separate Slapback section removed in favor of this.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::delayL, 1 }, "Delay L",
        juce::NormalisableRange<float> (0.0f, 250.0f, 0.01f, 0.3f), 15.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " ms"; })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::delayR, 1 }, "Delay R",
        juce::NormalisableRange<float> (0.0f, 250.0f, 0.01f, 0.3f), 15.0f,
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

    // Warmth: a final tone stage (low-shelf body boost + low-pass + soft
    // saturation) on the fully summed output — for a darker, rounder
    // character none of the other sections provide. Off by default
    // (Warmth Mix = 0%), like Vibrato.
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
        juce::ParameterID { ParamIDs::vibratoOn, 1 }, "Vibrato On", false));

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
    const float vibRateHz   = apvts.getRawParameterValue (ParamIDs::vibratoRate)->load();
    const float vibDepthMs  = apvts.getRawParameterValue (ParamIDs::vibratoDepth)->load();
    const float vibMixAmt   = apvts.getRawParameterValue (ParamIDs::vibratoMix)->load() * 0.01f;
    const float warmthToneHz  = apvts.getRawParameterValue (ParamIDs::warmthTone)->load();
    const float warmthDriveAmt = apvts.getRawParameterValue (ParamIDs::warmthDrive)->load() * 0.01f;
    const float warmthBodyAmt  = apvts.getRawParameterValue (ParamIDs::warmthBody)->load() * 0.01f;
    const float warmthMixAmt  = apvts.getRawParameterValue (ParamIDs::warmthMix)->load() * 0.01f;

    // Section on/off — each section's DSP still runs below (for click-free
    // re-enabling), only its contribution to the output is gated here.
    const bool shiftIsOn   = apvts.getRawParameterValue (ParamIDs::shiftOn)->load() > 0.5f;
    const bool vibratoIsOn = apvts.getRawParameterValue (ParamIDs::vibratoOn)->load() > 0.5f;
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

        // Focus is a crossover, not a filter on the wet signal: everything
        // below the Focus point passes through untouched, and only the band
        // above it goes through the pitch shifter + delay (matches how
        // Soundtoys MicroShift's Focus control works).
        const float lowL  = leftVoice.lowBandFilter.processSample (dryL);
        float highL = leftVoice.highBandFilter.processSample (dryL);
        highL = leftVoice.pitchShifter.processSample (highL);
        highL = leftVoice.delay.processSample (highL);
        const float wetL = lowL + highL;

        const float lowR  = rightVoice.lowBandFilter.processSample (dryR);
        float highR = rightVoice.highBandFilter.processSample (dryR);
        highR = rightVoice.pitchShifter.processSample (highR);
        highR = rightVoice.delay.processSample (highR);
        const float wetR = lowR + highR;

        // Vibrato: continuous LFO-swept delay (pitch wobble), independent of
        // the static Width detune — the "Cajun Moon"-style swirl.
        const float vibL = vibratoL.processSample (dryL);
        const float vibR = vibratoR.processSample (dryR);

        // Width and Vibrato are two independent "lenses" on the same dry
        // signal, each crossfaded against that same dry baseline and then
        // summed (rather than sequentially crossfaded), so turning one up
        // doesn't eat into the other. With vibratoMix at 0 this reduces
        // exactly to the previous dry*(1-mix) + wet*mix formula.
        const float shiftMixL = shiftIsOn ? mix * (wetL - dryL) : 0.0f;
        const float shiftMixR = shiftIsOn ? mix * (wetR - dryR) : 0.0f;
        const float vibMixL   = vibratoIsOn ? vibMixAmt * (vibL - dryL) : 0.0f;
        const float vibMixR   = vibratoIsOn ? vibMixAmt * (vibR - dryR) : 0.0f;

        const float outL = dryL + shiftMixL + vibMixL;
        const float outR = dryR + shiftMixR + vibMixR;

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

const juce::StringArray& JJBreezeAudioProcessor::getPresetCategories()
{
    // Menu order. Every Preset::category below is one of these.
    static const juce::StringArray categories { "Start", "Vocal", "Width", "Character", "Echo", "FX" };
    return categories;
}

const std::array<JJBreezeAudioProcessor::Preset, 18>& JJBreezeAudioProcessor::getPresets()
{
    // The host's program list, in this order. JJ Moon comes first so that
    // program 0 — what projects saved with the retired "Default" preset (an
    // exact copy of JJ Moon) recall, and what some hosts select on load —
    // still gives the same sound. The editor's preset menu groups the
    // presets by category instead (see getPresetCategories()).
    //
    // The same eighteen presets as the AUv3 sibling (../jj-breeze-auv3,
    // JJBreezeExtension/Parameters/Presets.swift). The ones that started on
    // the desktop keep the values (and the analysis) they were built from
    // here; the rest are carried over from the AUv3 as-is.
    //
    // name, category, summary,
    // pitchL, pitchR, delayL, delayR, focus, mix, vibratoRate, vibratoDepth, vibratoMix,
    // warmthTone, warmthDrive, warmthBody, warmthMix, shiftOn, vibratoOn, warmthOn
    static const std::array<Preset, 18> presets { {
        // "Cajun Moon"-style warmth: retuned against an actual reference
        // recording (see example/cajunmoon_vocal.mp3) rather than guessed.
        // That analysis found (1) no discrete slapback echo at all, (2) no
        // strong deliberate vibrato — the measured pitch movement was just
        // natural vocal phrasing, not a steady ~1-2Hz wobble, and (3) the
        // one clear, dominant trait: a heavily rolled-off, dark/warm tone
        // (only ~1% of spectral energy above 2kHz). So width stays off,
        // vibrato is a light touch rather than the main event, and Warmth
        // — not vibrato — is what actually carries the "Cajun Moon"
        // character here. Pitch L/R +300ct (matching sign, not opposite — a
        // mono-compatible pitch-up rather than a wide microshift) matches
        // Pitch L/R's own parameter default; see createParameterLayout().
        { "JJ Moon", "Vocal", "Warm doubled voice, light vibrato",
          300.0f, 300.0f, 23.0f, 37.0f, 25.0f, 13.0f, 1.2f, 3.0f, 15.0f, 3500.0f, 20.0f, 0.0f, 20.0f, true, true, true },

        // "Init": a clean slate — Shift switched in but at zero mix, every
        // other section out, so nothing is heard until a knob is turned.
        { "Init", "Start", "Dry signal, everything at zero",
          0.0f, 0.0f, 0.0f, 0.0f, 20.0f, 0.0f, 1.2f, 3.0f, 0.0f, 3500.0f, 20.0f, 0.0f, 0.0f, true, false, false },

        // "JJ Call Me The Breeze": built from analyzing
        // example/call_me_the_breaze_vocal.wav (7.25s, 44.1kHz stereo) the
        // same way JJ Cajun Moon and JJ Lies were — no dry counterpart
        // exists for this one, so the numbers below come from measuring the
        // reference's own traits and then calibrating each control against
        // them by running a dry vocal (example/lies_1.mp3, mono-summed)
        // through this exact chain in a port of the DSP:
        //
        //  * Width, not detune. Stereo correlation sits at +0.81 with a
        //    side/mid ratio of 0.35 — clearly wide — but per-frame
        //    cross-correlation and pitch-tracking both put the L/R time
        //    offset at 0.00ms and the L/R pitch difference at 0.0 cents,
        //    and the width is uniform across every octave (side/mid 0.33 to
        //    0.39 from 80Hz to 20kHz). Pitch L at -8 and Pitch R at +8
        //    cents, with Mix 35%, land on that measurement (calibrated:
        //    +0.78 correlation,
        //    0.354 side/mid, and 0.0 cents of *measured* L/R difference,
        //    since at 35% the unshifted dry still sets each channel's
        //    pitch), and Focus at 25Hz keeps the width band-uniform the way
        //    the reference is, rather than the default 150Hz crossover's
        //    highs-only widening.
        //  * Dark, but only moderately. 85% of the energy sits below
        //    1.1kHz and the spectrum falls at -7.7 dB/oct across 1-8kHz —
        //    darker than a typical vocal, yet twice as bright above 2kHz
        //    (5.3% of total energy) as example/cajunmoon_vocal.mp3's 2.5%.
        //    So Warmth is on but gentle: Tone 4kHz / Drive 25% / Mix 30%
        //    reproduces that -7.7 dB/oct tilt (measured -7.6) instead of
        //    JJ Cajun Moon's much heavier rolloff.
        //  * Body stays at 0%. Warmth's shelf lifts 150Hz and below, and
        //    the reference has almost nothing there (0.1% under 100Hz,
        //    0.4% under 150Hz); its 200Hz-1kHz slope is also *shallower*
        //    than the dry reference vocal's, so there is no low-end lift to
        //    match — its warmth is top-end rolloff only.
        //  * No vibrato and no echo. Sustained notes hold within +-10 cents
        //    once they settle (the apparent ~1.5Hz movement is phrasing
        //    into and out of each note, the same false positive the Cajun
        //    Moon analysis found), and the envelope autocorrelation decays
        //    smoothly with no repeat peak anywhere from 15 to 400ms. So
        //    Vibrato is off, and Delay L/R stay in width territory
        //    (12/18ms) rather than slapback territory.
        //
        // One measured trait deliberately not reproduced: the reference's
        // left channel is ~2.3dB louder than its right. Shift has no
        // per-channel gain, and a static level tilt isn't the character
        // anyway.
        { "JJ Breeze", "Vocal", "Wide, gently darkened vocal",
          -8.0f, 8.0f, 12.0f, 18.0f, 25.0f, 35.0f, 1.2f, 3.0f, 0.0f, 4000.0f, 25.0f, 0.0f, 30.0f, true, false, true },

        // "JJ Dark Vocal": originally built by comparing two takes of the
        // same performance (example/cajunmoon_vocal_vocal_1.mp3 vs
        // _vocal_2.mp3). Pitches down rather than up — a deliberately
        // deeper take on "dark" rather than a literal vocal_2 match, since a
        // lower/warmer voice is still a reasonable, independently useful
        // direction. Uses Shift (not a separate Drop section — removed once
        // Shift's range covered the same ground) with Focus turned all the
        // way down so the pitch-down covers the full band, not just what's
        // above the default 150Hz crossover point. Warmth's Body control
        // adds the low-mid "chest" fullness the original analysis found.
        { "JJ Dark Vocal", "Vocal", "Lower, darker voice with chest",
          -300.0f, -300.0f, 15.0f, 15.0f, 25.0f, 15.0f, 1.1f, 3.5f, 15.0f, 2800.0f, 25.0f, 70.0f, 15.0f, true, true, true },

        // "Lies": built from analyzing example/lies_1.mp3 (dry) against
        // example/lies_2.mp3 (processed) the same way JJ Dark Vocal was —
        // autocorrelation pitch-tracking found the right channel shifted up
        // by a measured +470 cents (154Hz to 202Hz median), while the left
        // channel's level collapsed to near-total digital silence in
        // lies_2. That silenced channel isn't reproducible here (Shift has
        // no per-channel pan/gain, only pitch and delay), so this preset
        // approximates the same *asymmetry* instead, Octave-Width-style:
        // Pitch L stays at 0 (matching how close lies_1's L and R already
        // were — only one side got the extreme treatment) while Pitch R
        // carries the measured +470ct alone, one channel left recognizable
        // against a wildly pitched-up other one. Focus dropped low so the
        // shift covers the full band rather than just the highs, and Mix
        // pushed higher than Octave Width's 15% since here the effect is
        // meant to dominate, not just widen. The large brightness increase
        // measured between the two files (spectral centroid 682Hz to
        // 1288Hz) needed no separate Warmth stage — an upward pitch shift
        // this size produces that on its own. No vibrato or delay signature
        // was found, so both stay off/default.
        { "JJ Lies", "Vocal", "One side lifted far up",
          0.0f, 470.0f, 15.0f, 15.0f, 25.0f, 25.0f, 1.2f, 3.0f, 0.0f, 3500.0f, 20.0f, 0.0f, 0.0f, true, false, false },

        // Focus at 150 Hz keeps the chest of the voice centred and untouched.
        { "Subtle Doubler", "Vocal", "Natural double-tracked feel",
          -7.0f, 7.0f, 9.0f, 14.0f, 150.0f, 25.0f, 1.2f, 3.0f, 0.0f, 3500.0f, 20.0f, 0.0f, 0.0f, true, false, false },

        // Opposite-sign shifts: the classic wide microshift, pushed big.
        { "Stereo Width", "Width", "Opposite pitch shifts, big spread",
          300.0f, -300.0f, 23.0f, 37.0f, 20.0f, 18.0f, 1.2f, 2.5f, 10.0f, 3500.0f, 18.0f, 0.0f, 15.0f, true, true, true },

        { "Wide Guitar", "Width", "Detuned width for guitars and keys",
          -12.0f, 12.0f, 18.0f, 31.0f, 200.0f, 30.0f, 0.8f, 1.5f, 10.0f, 3500.0f, 20.0f, 0.0f, 0.0f, true, true, false },

        // Everything below 300 Hz stays dry, so the low end stays mono.
        { "Bass-Safe Width", "Width", "Widens the highs, bass stays mono",
          -10.0f, 10.0f, 12.0f, 20.0f, 300.0f, 30.0f, 1.2f, 3.0f, 0.0f, 3500.0f, 20.0f, 0.0f, 0.0f, true, false, false },

        // Width from the precedence (Haas) effect alone: no pitch shift,
        // only the right side's tap delayed.
        { "Haas Width", "Width", "Width from delay alone, no detune",
          0.0f, 0.0f, 0.0f, 15.0f, 120.0f, 40.0f, 1.2f, 3.0f, 0.0f, 3500.0f, 20.0f, 0.0f, 0.0f, true, false, false },

        // "Octave Width": a stereo width effect using Shift's independent
        // L/R at the full-octave end of its range rather than the
        // cents-level microshift territory JJ Moon/JJ Breeze live in — the
        // left channel drops a full octave (Pitch L −1200 cents), the right
        // stays at pitch (Pitch R 0), blended low so the dry fundamental
        // stays audible under the sub-octave layer. Focus down low so the
        // drop covers the full band, not just the highs. More like an old
        // octave pedal panned across the stereo field than a chorus-y
        // doubler.
        { "Octave Width", "Width", "Octave-down layer on the left",
          -1200.0f, 0.0f, 15.0f, 15.0f, 25.0f, 15.0f, 1.2f, 3.0f, 15.0f, 3500.0f, 20.0f, 0.0f, 15.0f, true, false, true },

        // Character presets leave Shift out entirely: the sound is Vibrato
        // and/or Warmth on their own.
        { "Tape Wobble", "Character", "Slow tape wow, soft saturation",
          0.0f, 0.0f, 15.0f, 15.0f, 25.0f, 0.0f, 0.5f, 2.5f, 40.0f, 5000.0f, 30.0f, 0.0f, 50.0f, false, true, true },

        { "Old Radio", "Character", "Dull, driven, lo-fi tone",
          0.0f, 0.0f, 15.0f, 15.0f, 25.0f, 0.0f, 1.2f, 3.0f, 0.0f, 1800.0f, 50.0f, 0.0f, 60.0f, false, false, true },

        { "Seasick", "Character", "Fast, deep pitch warble",
          0.0f, 0.0f, 15.0f, 15.0f, 25.0f, 0.0f, 6.0f, 1.5f, 50.0f, 3500.0f, 20.0f, 0.0f, 0.0f, false, true, false },

        // "Slapback Twang": the classic rockabilly move — a bright, dry
        // signal with nothing but a single delayed repeat. Previously a
        // dedicated Slapback section (Time 110ms, Feedback 25%, Mix 35%,
        // Shift itself off since it contributed nothing at Mix 0%); now
        // built directly from Shift instead (the section that section's
        // range was folded into) — Pitch L/R at 0 (no shift, repeat only),
        // Delay L/R at 110/115ms (matching the old Slap Time, offset
        // slightly L/R), Focus dropped to 25Hz so the repeat covers the
        // full band like a real slapback rather than just the highs, Mix
        // at 35% matching the old Slap Mix. One real difference: Shift's
        // Delay is a single tap with no feedback path, so this is now a
        // single clean repeat rather than the old preset's couple of
        // decaying echoes. Vibrato and Warmth stay off on purpose, so the
        // top end stays open/twangy rather than rolling dark.
        { "Slapback Twang", "Echo", "Classic single slap repeat",
          0.0f, 0.0f, 110.0f, 115.0f, 25.0f, 35.0f, 1.2f, 3.0f, 0.0f, 3500.0f, 20.0f, 0.0f, 0.0f, true, false, false },

        // Slapback Twang split across the stereo field: two different slap
        // times, a hair of detune between them, the top end gently rounded.
        { "50s Slap Wide", "Echo", "Stereo slap, slightly detuned",
          -5.0f, 5.0f, 95.0f, 130.0f, 25.0f, 30.0f, 1.2f, 3.0f, 0.0f, 4500.0f, 20.0f, 0.0f, 25.0f, true, false, true },

        // Focus at 800 Hz keeps the harmony out of the low mids.
        { "Fifth Up", "FX", "Faint harmony a fifth above",
          700.0f, 700.0f, 15.0f, 25.0f, 800.0f, 15.0f, 1.2f, 3.0f, 0.0f, 3500.0f, 20.0f, 0.0f, 0.0f, true, false, false },

        // "Deep Baritone": JJ Dark Vocal pushed further into effect
        // territory rather than a natural-sounding voice — Pitch L/R at
        // -700 cents (vs JJ Dark Vocal's -300) with Warmth's Body and
        // Drive both turned up further, for a growly, monster-movie-
        // trailer low end. No Vibrato here (unlike the other dark/warm
        // presets) — a wobble reads as comic rather than menacing at this
        // depth.
        { "Deep Baritone", "FX", "Voice dropped a fifth",
          -700.0f, -700.0f, 15.0f, 15.0f, 25.0f, 20.0f, 1.1f, 3.5f, 0.0f, 2500.0f, 35.0f, 85.0f, 70.0f, true, false, true },
    } };
    return presets;
}

JJBreezeAudioProcessor::PatchValues JJBreezeAudioProcessor::valuesOf (const Preset& p)
{
    // ParamIDs::all order.
    return { p.pitchL, p.pitchR, p.delayL, p.delayR, p.focus, p.mix,
             p.vibratoRate, p.vibratoDepth, p.vibratoMix,
             p.warmthTone, p.warmthDrive, p.warmthBody, p.warmthMix,
             p.shiftOn ? 1.0f : 0.0f, p.vibratoOn ? 1.0f : 0.0f, p.warmthOn ? 1.0f : 0.0f };
}

JJBreezeAudioProcessor::PatchValues JJBreezeAudioProcessor::captureValues() const
{
    PatchValues values {};
    for (size_t i = 0; i < ParamIDs::all.size(); ++i)
        if (auto* param = apvts.getParameter (ParamIDs::all[i]))
            values[i] = param->convertFrom0to1 (param->getValue());
    return values;
}

void JJBreezeAudioProcessor::applyValues (const PatchValues& values)
{
    for (size_t i = 0; i < ParamIDs::all.size(); ++i)
        if (auto* param = apvts.getParameter (ParamIDs::all[i]))
            param->setValueNotifyingHost (param->convertTo0to1 (values[i]));
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

juce::String JJBreezeAudioProcessor::getProgramCategory (int index) const
{
    const auto& presets = getPresets();
    if (index >= 0 && index < (int) presets.size())
        return presets[(size_t) index].category;
    return {};
}

juce::String JJBreezeAudioProcessor::getProgramSummary (int index) const
{
    const auto& presets = getPresets();
    if (index >= 0 && index < (int) presets.size())
        return presets[(size_t) index].summary;
    return {};
}

void JJBreezeAudioProcessor::applyPreset (int index)
{
    const auto& presets = getPresets();
    if (index < 0 || index >= (int) presets.size())
        return;

    applyValues (valuesOf (presets[(size_t) index]));
}

void JJBreezeAudioProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;

    undoManager.beginNewTransaction ("Load preset: " + getProgramName (index));

    currentProgram = index;
    applyPreset (index);
    setLoadedPreset ({}, captureValues());
    updateHostDisplay();
}

void JJBreezeAudioProcessor::setLoadedPreset (const juce::String& userPresetName, const PatchValues& values)
{
    {
        const juce::ScopedLock sl (presetLock);
        currentUserPresetName = userPresetName;
        loadedValues = values;
    }
    ++presetChangeCount;
}

juce::String JJBreezeAudioProcessor::getCurrentUserPresetName() const
{
    const juce::ScopedLock sl (presetLock);
    return currentUserPresetName;
}

juce::String JJBreezeAudioProcessor::getCurrentPresetName() const
{
    const auto userName = getCurrentUserPresetName();
    return userName.isNotEmpty() ? userName : getPresets()[(size_t) juce::jlimit (0, (int) getPresets().size() - 1, currentProgram)].name;
}

bool JJBreezeAudioProcessor::isPresetModified() const
{
    PatchValues reference;
    {
        const juce::ScopedLock sl (presetLock);
        reference = loadedValues;
    }

    auto live = captureValues();

    // POWER off forces every section's switch off without that being an
    // edit — compare against the switches it will put back instead.
    if (bypassed)
    {
        live[13] = bypassSavedShiftOn   ? 1.0f : 0.0f;
        live[14] = bypassSavedVibratoOn ? 1.0f : 0.0f;
        live[15] = bypassSavedWarmthOn  ? 1.0f : 0.0f;
    }

    constexpr float tolerance = 0.002f; // of each parameter's full range

    for (size_t i = 0; i < ParamIDs::all.size(); ++i)
    {
        const auto range = apvts.getParameterRange (ParamIDs::all[i]);
        const float span = juce::jmax (range.end - range.start, std::numeric_limits<float>::epsilon());
        if (std::abs (live[i] - reference[i]) / span > tolerance)
            return true;
    }

    return false;
}

void JJBreezeAudioProcessor::revertToLoadedPreset()
{
    PatchValues reference;
    {
        const juce::ScopedLock sl (presetLock);
        reference = loadedValues;
    }

    undoManager.beginNewTransaction ("Revert to " + getCurrentPresetName());
    bypassed = false; // the preset's own section switches are what come back
    applyValues (reference);
}

bool JJBreezeAudioProcessor::overwriteCurrentUserPreset()
{
    const auto name = getCurrentUserPresetName();
    if (name.isEmpty())
        return false;

    saveUserPreset (name);
    return true;
}

void JJBreezeAudioProcessor::storeCompareSnapshot (int slot)
{
    if (slot < 0 || slot > 1)
        return;

    for (size_t i = 0; i < ParamIDs::all.size(); ++i)
        if (auto* param = apvts.getParameter (ParamIDs::all[i]))
            compareSnapshots[slot][i] = param->getValue();

    hasCompareSnapshot[slot] = true;
}

void JJBreezeAudioProcessor::recallCompareSnapshot (int slot)
{
    if (slot < 0 || slot > 1 || ! hasCompareSnapshot[slot])
        return;

    undoManager.beginNewTransaction ("Recall compare slot " + juce::String (slot == 0 ? "A" : "B"));

    for (size_t i = 0; i < ParamIDs::all.size(); ++i)
        if (auto* param = apvts.getParameter (ParamIDs::all[i]))
            param->setValueNotifyingHost (compareSnapshots[slot][i]);
}

void JJBreezeAudioProcessor::setBypassed (bool shouldBeBypassed)
{
    if (shouldBeBypassed == bypassed)
        return;

    undoManager.beginNewTransaction (shouldBeBypassed ? "Bypass" : "Un-bypass");

    auto setSectionOn = [this] (const juce::String& paramID, bool value)
    {
        if (auto* param = apvts.getParameter (paramID))
            param->setValueNotifyingHost (value ? 1.0f : 0.0f);
    };

    if (shouldBeBypassed)
    {
        // Remember each section's current on/off state, then force
        // everything off so the output is the untouched dry signal — a
        // quick "hear it raw" without disturbing any knob position or
        // which sections were individually on.
        bypassSavedShiftOn   = apvts.getRawParameterValue (ParamIDs::shiftOn)->load()   > 0.5f;
        bypassSavedVibratoOn = apvts.getRawParameterValue (ParamIDs::vibratoOn)->load() > 0.5f;
        bypassSavedWarmthOn  = apvts.getRawParameterValue (ParamIDs::warmthOn)->load()  > 0.5f;
        setSectionOn (ParamIDs::shiftOn,   false);
        setSectionOn (ParamIDs::vibratoOn, false);
        setSectionOn (ParamIDs::warmthOn,  false);
    }
    else
    {
        setSectionOn (ParamIDs::shiftOn,   bypassSavedShiftOn);
        setSectionOn (ParamIDs::vibratoOn, bypassSavedVibratoOn);
        setSectionOn (ParamIDs::warmthOn,  bypassSavedWarmthOn);
    }

    bypassed = shouldBeBypassed;
}

juce::File JJBreezeAudioProcessor::getUserPresetsDirectory() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Gerov").getChildFile ("jj-breeze").getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

juce::File JJBreezeAudioProcessor::userPresetFile (const juce::String& name) const
{
    return getUserPresetsDirectory().getChildFile (juce::File::createLegalFileName (name) + ".xml");
}

juce::StringArray JJBreezeAudioProcessor::getUserPresetNames() const
{
    juce::StringArray names;
    for (const auto& file : getUserPresetsDirectory().findChildFiles (juce::File::findFiles, false, "*.xml"))
        names.add (file.getFileNameWithoutExtension());
    names.sort (true);
    return names;
}

void JJBreezeAudioProcessor::saveUserPreset (const juce::String& name)
{
    // Tracked under the name it is stored as (illegal filename characters
    // stripped), which is how getUserPresetNames() will list it.
    const auto file = userPresetFile (name);
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            if (xml->writeTo (file))
                setLoadedPreset (file.getFileNameWithoutExtension(), captureValues());
}

bool JJBreezeAudioProcessor::loadUserPreset (const juce::String& name)
{
    const auto file = userPresetFile (name);
    if (! file.existsAsFile())
        return false;

    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;

    // Same replaceState() mechanism setStateInformation() already uses for
    // full session recall — proven to correctly update every parameter
    // (and, through it, every attached UI control) in one shot.
    undoManager.beginNewTransaction ("Load user preset: " + name);
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    setLoadedPreset (name, captureValues());
    return true;
}

void JJBreezeAudioProcessor::deleteUserPreset (const juce::String& name)
{
    userPresetFile (name).deleteFile();

    // The sound stays as it is; the last factory program becomes the
    // loaded preset again, so the edited dot lights if the two differ.
    if (getCurrentUserPresetName() == name)
        setLoadedPreset ({}, valuesOf (getPresets()[(size_t) currentProgram]));
}

JJBreezeAudioProcessor::PatchValues JJBreezeAudioProcessor::readUserPresetValues (const juce::String& name,
                                                                                const PatchValues& fallback) const
{
    auto values = fallback;

    if (auto xml = juce::XmlDocument::parse (userPresetFile (name)))
    {
        const auto tree = juce::ValueTree::fromXml (*xml);
        for (size_t i = 0; i < ParamIDs::all.size(); ++i)
        {
            const auto child = tree.getChildWithProperty ("id", ParamIDs::all[i]);
            if (child.isValid() && child.hasProperty ("value"))
                values[i] = (float) child.getProperty ("value");
        }
    }

    return values;
}

// The session state is the parameter tree (the same XML a user preset
// file holds), plus a few attributes on its root element for things that
// belong to the project but not to a preset: which preset was loaded, and
// the Pitch/Delay link switches. Added to the XML here and stripped again
// in setStateInformation(), so they never end up in apvts.state — and so
// never in a user preset saved from it.
namespace StateKeys
{
    static const juce::Identifier program    { "jjProgram" };
    static const juce::Identifier userPreset { "jjUserPreset" };
    static const juce::Identifier linkPitch  { "jjLinkPitch" };
    static const juce::Identifier linkDelay  { "jjLinkDelay" };
}

void JJBreezeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        if (auto xml = state.createXml())
        {
            xml->setAttribute (StateKeys::program, currentProgram);
            xml->setAttribute (StateKeys::userPreset, getCurrentUserPresetName());
            xml->setAttribute (StateKeys::linkPitch, linkPitch.load());
            xml->setAttribute (StateKeys::linkDelay, linkDelay.load());
            copyXmlToBinary (*xml, destData);
        }
    }
}

void JJBreezeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    // Sessions saved before these attributes existed recall as JJ Moon
    // (what their "Default" was) with both links off.
    const int program = juce::jlimit (0, getNumPrograms() - 1, xml->getIntAttribute (StateKeys::program, initialProgram));
    const auto userPreset = xml->getStringAttribute (StateKeys::userPreset);
    linkPitch = xml->getBoolAttribute (StateKeys::linkPitch, false);
    linkDelay = xml->getBoolAttribute (StateKeys::linkDelay, false);

    for (const auto& key : { StateKeys::program, StateKeys::userPreset, StateKeys::linkPitch, StateKeys::linkDelay })
        xml->removeAttribute (key);

    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    // The edited dot compares against what the preset itself holds, not
    // against the restored knobs — a project saved with edits reopens
    // with the dot lit, as it was left. A user preset that has since been
    // deleted from this machine falls back to its factory program.
    currentProgram = program;
    const auto factoryValues = valuesOf (getPresets()[(size_t) program]);
    if (userPreset.isNotEmpty() && userPresetFile (userPreset).existsAsFile())
        setLoadedPreset (userPreset, readUserPresetValues (userPreset, factoryValues));
    else
        setLoadedPreset ({}, factoryValues);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JJBreezeAudioProcessor();
}
