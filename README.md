# jj-breeze

A stereo micro-pitch widener + slapback echo for Logic Pro (and any AU/VST3
host). It sits between two well-known reference points:

- **Stillwell Audio CMX** — a "stereo microshifter": doubles a signal and
  applies tiny pitch shifts + delay to each side for width.
- **Soundtoys MicroShift** — the same trick (micro pitch shift + time-varying
  delay, panned hard left/right), with extra vintage flavor and more controls.

jj-breeze implements the same core widening technique — it is *not* a
clone of either product's code or presets — but deliberately exposes far
fewer controls than both, aiming for "turn one or two knobs and it sounds
right." It also adds a separate slapback echo section and a Vibrato section,
plus "JJ Cale Vocal" and "Cajun Moon Vocal" factory presets that lean into
intimate/narrow and warm/swirling rather than wide (see Factory presets
below) — hence the name, a nod to JJ Cale's "Call Me the Breeze."

## How it works

Each channel runs through:

1. **A dual-tap crossfaded delay-line pitch shifter** (`Source/DSP/PitchShifter.h`)
   — the classic "microshift" technique: two read pointers trail the write
   pointer by a delay that ramps up or down depending on the desired pitch
   ratio, crossfaded with 50%-overlapping Hann windows so each tap's reset is
   inaudible. Left and right each have their own independent shift amount.
2. **A short modulated delay** (`Source/DSP/ModulatedDelay.h`) with a slow,
   fixed-depth LFO wobble on top of the user's Delay L/R knobs, for the
   "time-varying delay" movement both reference plugins have.
3. **A Focus crossover** — matching how Soundtoys MicroShift's Focus control
   actually works (not a low-cut on the wet signal): a low-pass/high-pass
   pair split the *input* at the Focus frequency, the low band passes through
   completely untouched, and only the high band feeds the pitch
   shifter + delay above. So turning Focus up doesn't just dull the width
   effect — it protects more of the low end from being processed at all.

The (crossover-recombined) wet signal is then blended with the dry signal via Mix.

Separately, **a mono/centered slapback echo** (`Source/DSP/SlapbackDelay.h`)
is summed on top of the output — a single (or, with feedback, a few) damped
repeat(s), the way an old tape echo would sound. It's a genuinely separate
effect from the width processing above (real slapback stays centered/dry;
width effects are the opposite of that), which is why it's off by default
(Slap Mix = 0%) and has its own three controls rather than interacting with
Mix. Useful together for something in the direction of a JJ Cale-style vocal:
keep the widener very subtle (or off) and use a light slapback instead — see
the Controls table below for suggested starting values.

Also separately, **a Vibrato section** (reuses `Source/DSP/ModulatedDelay.h`)
— continuous LFO-driven pitch wobble via a swept short delay (the classic
"delay-based vibrato" technique, the same mechanism a Uni-Vibe's vibrato mode
uses), rather than the Width knobs' *static* micro-detune. This is what gets
you a slow, warm, swirling character (in the direction of JJ Cale's "Cajun
Moon") that a fixed detune can't produce. Also off by default (Vibrato
Mix = 0%). Width and Vibrato are independent "lenses" on the same dry signal
— each is crossfaded against that dry signal by its own Mix knob and the two
are then summed, so turning one up doesn't eat into the other; with Vibrato
Mix at 0 the output is identical to before this section existed.

## Controls

Pitch and delay are controlled independently per channel — there's no
Wide/Mono-Safe mode switch; you just dial in whatever left/right pitch and
delay values you want directly.

| Control | Range | What it does |
|---|---|---|
| **Pitch L** | −50 to +50 cents | Pitch shift applied to the left channel. Default +12 (up). |
| **Pitch R** | −50 to +50 cents | Pitch shift applied to the right channel. Default −12 (down) — opposite Pitch L gives the classic wide microshift; matching signs instead gives a more mono-compatible width via delay offset alone. |
| **Delay L** | 0–40 ms | Base delay time on the left channel, with subtle built-in modulation. |
| **Delay R** | 0–40 ms | Base delay time on the right channel. |
| **Focus** | 20 Hz–10 kHz | Crossover point (both channels): everything below stays fully dry/untouched; only the band above gets pitch-shifted + delayed. Raise it to keep the width effect off the low end entirely. |
| **Mix** | 0–100% | Dry/wet blend (both channels). |
| **Slap Time** | 30–300 ms | Delay time of the slapback echo. Classic slapback territory is roughly 80–140 ms. |
| **Slap Feedback** | 0–70% | How much the (damped) repeat feeds back for further repeats. 0% = a single slap; higher = a decaying series of repeats. |
| **Slap Mix** | 0–100% | Level of the slapback echo. 0% by default (off) — this is a separate, opt-in effect. |
| **Vibrato Rate** | 0.1–8 Hz | Speed of the pitch wobble. JJ Cale's "Cajun Moon" territory is slow, around 1–1.5 Hz. |
| **Vibrato Depth** | 0–8 ms | How far the swept delay moves — bigger swing = more obvious pitch wobble. |
| **Vibrato Mix** | 0–100% | Blend of the wobbled signal with dry. At 100% it's a true vibrato (fully replaces the static pitch); lower values give a wobbly chorus-like blend instead. 0% by default (off). |

## Factory presets

The plugin exposes three factory presets (`Source/PluginProcessor.cpp`,
`getPresets()`) through the host's own preset menu (in Logic: the preset
field at the top of the plugin window) — no in-plugin preset UI needed:

- **Default** — the settings above.
- **JJ Cale Vocal** — width turned way down (Pitch L/R ±4 ct, Mix 18%) rather
  than off, for a touch of doubling glue without an obvious wide effect,
  plus a single low-feedback slapback (Time 100 ms, Feedback 12%, Mix 20%)
  instead of the width being the main event. Aimed at an intimate,
  laid-back, close vocal rather than a wide/shimmery one.
- **Cajun Moon Vocal** — width off entirely (0 ct, 0% Mix); the character
  instead comes from a slow Vibrato (1.1 Hz, 3.5 ms depth, 55% mix) for that
  warm swirling wobble, plus a light slapback (Time 100 ms, Feedback 10%,
  Mix 15%) for presence.

To add more presets, extend the `std::array<Preset, N>` returned by
`getPresets()` in `PluginProcessor.cpp` (and bump `N`).

The LFO rate/depth on the width path's modulated delay, the pitch shifter's
grain length, the slapback's feedback-path damping (fixed low-pass ~3.5 kHz,
for a warm/tape-like repeat), and Vibrato's fixed 9ms center delay are all
fixed internally rather than exposed as parameters — that's the main way
this plugin stays simpler than its references.

## Building

Requires Xcode (command line tools), CMake ≥ 3.22. JUCE 9.0.1 is fetched
automatically via CMake `FetchContent` — no manual JUCE install needed.

```sh
cmake -B build -G Xcode -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

This builds three targets:

- **AU** and **VST3** — automatically copied to
  `~/Library/Audio/Plug-Ins/Components` and
  `~/Library/Audio/Plug-Ins/VST3` (via `COPY_PLUGIN_AFTER_BUILD`). Restart
  Logic (or rescan plugins) to see **jj-breeze** under Audio Units >
  Gerov.
- **Standalone** — a runnable app for quick testing without opening a DAW,
  at `build/jj_breeze_artefacts/Release/Standalone/jj-breeze.app`.

## Project layout

```
CMakeLists.txt              JUCE plugin target (AU, VST3, Standalone)
Source/
  PluginProcessor.h/.cpp    Parameters (APVTS) + the per-block audio path
  PluginEditor.h/.cpp       Minimal GUI: widener knobs + separate Slapback and Vibrato sections
  DSP/
    PitchShifter.h          Dual-tap crossfaded delay-line pitch shifter
    ModulatedDelay.h        Short delay + LFO wobble (used by both Width and Vibrato)
    SlapbackDelay.h          Mono feedback delay with damped repeats (echo)
```
