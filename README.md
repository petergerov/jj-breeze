# jj-breeze

A stereo micro-pitch widener for Logic Pro (and any AU/VST3 host). It sits
between two well-known reference points:

- **Stillwell Audio CMX** — a "stereo microshifter": doubles a signal and
  applies tiny pitch shifts + delay to each side for width.
- **Soundtoys MicroShift** — the same trick (micro pitch shift + time-varying
  delay, panned hard left/right), with extra vintage flavor and more controls.

jj-breeze implements the same core widening technique — it is *not* a
clone of either product's code or presets — but deliberately exposes far
fewer controls than both, aiming for "turn one or two knobs and it sounds
right." It also adds a Vibrato section and a Warmth tone stage, plus "JJ
Cale Vocal", "Cajun Moon Vocal" and a family of "JJ Dark Vocal" factory
presets that lean into intimate/narrow, warm/dark, and deep/dark rather
than wide (see Factory presets below) — hence the name, a nod to JJ
Cale's "Call Me the Breeze."

## How it works

Each channel runs through:

1. **A dual-tap crossfaded delay-line pitch shifter** (`Source/DSP/PitchShifter.h`)
   — the classic "microshift" technique: two read pointers trail the write
   pointer by a delay that ramps up or down depending on the desired pitch
   ratio, crossfaded with 50%-overlapping Hann windows so each tap's reset is
   inaudible. Left and right each have their own independent shift amount.
   Pitch L/R covers ±1200 cents (a full octave), skewed so the fine
   micro-detune territory near 0 still gets most of the knob's travel — so
   the same mechanism covers both the classic microshift widening *and*, at
   larger values (with Focus turned down — see point 3), a big, semitone-
   scale dark/deep or bright/processed voice effect. One nuance worth
   knowing either way: the wet path here only covers whatever's above Focus
   — the band below always stays dry/unshifted — so getting a full-band
   shift out of this section means turning Focus most of the way down, not
   just Mix up.
2. **A short modulated delay** (`Source/DSP/ModulatedDelay.h`) with a slow,
   fixed-depth LFO wobble on top of the user's Delay L/R knobs, for the
   "time-varying delay" movement both reference plugins have. Delay L/R
   covers 0–250ms — wide enough to also reach classic slapback-echo timing
   directly (see the "Slapback Twang" preset below), not just the
   subtle-width territory the original 0–40ms range covered.
3. **A Focus crossover** — matching how Soundtoys MicroShift's Focus control
   actually works (not a low-cut on the wet signal): a low-pass/high-pass
   pair split the *input* at the Focus frequency, the low band passes through
   completely untouched, and only the high band feeds the pitch
   shifter + delay above. So turning Focus up doesn't just dull the width
   effect — it protects more of the low end from being processed at all.

The (crossover-recombined) wet signal is then blended with the dry signal via Mix.

Separately, **a Vibrato section** (reuses `Source/DSP/ModulatedDelay.h`)
— continuous LFO-driven pitch wobble via a swept short delay (the classic
"delay-based vibrato" technique, the same mechanism a Uni-Vibe's vibrato mode
uses), rather than the Width knobs' *static* micro-detune. This is what gets
you a slow, warm, swirling character that a fixed detune can't produce. Also
off by default (Vibrato Mix = 0%). Width and Vibrato are independent "lenses"
on the same dry signal — each is crossfaded against that dry signal by its
own Mix knob and the two are then summed, so turning one up doesn't eat into
the other; with Vibrato Mix at 0 the output is identical to before this
section existed.

Finally, **a Warmth stage** (`Source/DSP/Warmth.h`) — a low-shelf body boost,
then a low-pass filter, then gentle tanh soft-saturation, applied to the
*fully-summed* output rather than being another independent lens on dry.
Unlike Width/Vibrato, this isn't meant to be a special effect that
stacks with the rest — it's a final tone-shaping pass for a darker, rounder,
more "through an old tube amp" character. Off by default (Warmth Mix = 0%).
The low-pass + saturation were added after analyzing `example/cajunmoon_vocal.mp3`
as a reference target: that recording's defining trait turned out to be a
heavily rolled-off top end (only ~1% of its spectral energy above 2kHz), not
width, echo, or a strong deliberate vibrato — something none of the other
sections could produce, hence this stage. The low-shelf **Body** control was
added later, from a second, more targeted analysis (see "JJ Dark Vocal"
under Factory presets below) that isolated *pitch* and *low-mid fullness*,
not top-end rolloff, as what actually separates a "dark" take from a
"normal" one.

All three sections — Shift, Vibrato, Warmth — have their own lit on/off
switch in the UI. Turning one off both bypasses its contribution to the
output (its DSP keeps running internally, so re-enabling it is click-free)
and collapses its knobs out of the way, so a section that isn't doing
anything doesn't stay on screen distracting you.

(An earlier version of this plugin had a fourth section, "Drop" — a
separate, single-amount pitch shifter applied ahead of everything else. It
was removed once Pitch L/R's range was widened to ±1200 cents, since at
that point Shift covered the same ground and more, with independent
per-channel Pitch *and* Delay rather than one shared amount. A fifth
section, a dedicated mono/centered "Slapback" echo, was removed the same
way once Delay L/R was widened to 250ms: a single delayed repeat from
Shift's own Delay, with Focus turned down so it covers the full band, now
does the same job — see the "Slapback Twang" preset below and Delay L/R
in Controls. The one thing the dedicated section had that Shift's plain
delay tap doesn't: a feedback path for a *series* of decaying repeats
rather than just one.)

## Controls

Pitch and delay are controlled independently per channel — there's no
Wide/Mono-Safe mode switch; you just dial in whatever left/right pitch and
delay values you want directly.

| Control | Range | What it does |
|---|---|---|
| **Pitch L** | −1200 to +1200 cents | Pitch shift applied to the left channel. Default +300 (up) — see Factory presets/Default below for why. Skewed so fine micro-detune territory near 0 still gets most of the knob's travel, but the full ±1 octave is reachable. |
| **Pitch R** | −1200 to +1200 cents | Pitch shift applied to the right channel. Default +300 (up), matching Pitch L — opposite signs instead gives the classic wide microshift; matching signs (as the default does) gives a mono-compatible pitch shift via delay offset alone, which is also how the dark/deep and bright/processed presets below work. |
| **Delay L** | 0–250 ms | Base delay time on the left channel, with subtle built-in modulation. Subtle-width territory lives near the low end; classic slapback-echo timing (roughly 80–140 ms) and beyond is reachable further up, especially with Focus turned down (see "Slapback Twang" below). |
| **Delay R** | 0–250 ms | Base delay time on the right channel. |
| **Focus** | 20 Hz–10 kHz | Crossover point (both channels): everything below stays fully dry/untouched; only the band above gets pitch-shifted + delayed. Raise it to keep the width effect off the low end entirely; lower it (toward 20 Hz) to get a full-band pitch shift — or full-band delayed repeat — out of large Pitch L/R or Delay L/R values instead of just the highs. |
| **Mix** | 0–100% | Dry/wet blend (both channels). |
| **Vibrato Rate** | 0.1–8 Hz | Speed of the pitch wobble. JJ Cale's "Cajun Moon" territory is slow, around 1–1.5 Hz. |
| **Vibrato Depth** | 0–8 ms | How far the swept delay moves — bigger swing = more obvious pitch wobble. |
| **Vibrato Mix** | 0–100% | Blend of the wobbled signal with dry. At 100% it's a true vibrato (fully replaces the static pitch); lower values give a wobbly chorus-like blend instead. 0% by default (off). |
| **Warmth Tone** | 500 Hz–12 kHz | Low-pass cutoff applied to the final output. Lower = darker/warmer. Default 3.5 kHz. |
| **Warmth Drive** | 0–100% | Soft (tanh) saturation amount, applied after the low-pass. 0% = filter only, no added harmonics. |
| **Warmth Body** | 0–100% | Low-shelf boost at a fixed 150Hz corner, up to +6dB, applied before the low-pass. 0% = no boost. Restores the chest fullness a big pitch-down doesn't add on its own. |
| **Warmth Mix** | 0–100% | Blend of the warmed (shelved + filtered + saturated) signal with the rest of the chain's output. 0% by default (off). |

Each of the three sections (Shift, Vibrato, Warmth) also has its own on/off
switch, independent of its Mix knob — see "How it works" above.

The editor's header carries a few more workflow controls, top to bottom:

- A **preset** picker (see Factory presets below) covering both the nine
  built-in factory presets and any **user presets** you save. **SAVE**
  prompts for a name and writes the current knob settings to disk
  (`~/Library/Application Support/Gerov/jj-breeze/Presets`, independent of
  the fixed factory list, so the host-visible preset count never changes);
  **DEL** removes the selected user preset (disabled for factory presets,
  which can't be deleted). The picker's text dims once you've tweaked
  something away from the selected preset, so it never silently claims to
  still show what's actually loaded.
- **Undo/redo** (the curved-arrow icon buttons, or Cmd+Z/Cmd+Shift+Z once
  the editor has focus) — covers every knob, toggle, preset load and A/B
  recall, grouped so one knob drag or one preset load undoes as a single
  step. Mainly useful in the Standalone build, which — unlike being hosted
  in a DAW — has no host-level undo of its own.
- **BYPASS** forces all three sections off — the untouched dry signal —
  without touching any knob value or which sections were individually on,
  and restores them on un-bypass.
- An **A/B** compare toggle for quickly flipping between two variations of
  a patch while dialing it in (switching away from a slot stores whatever
  you last tweaked there, and a slot you haven't touched yet starts as a
  copy of the other one, so the first switch is silent).

The window itself is resizable (drag the bottom-right corner) with a fixed
aspect ratio, rather than a fixed size. Every knob resets to its default on
double-click, and hovering any knob, toggle, or header control shows a
tooltip explaining what it does — most usefully on Focus, whose crossover
behavior (everything below it stays untouched — see "How it works" above)
isn't obvious from the knob alone.

## Factory presets

The plugin exposes nine factory presets (`Source/PluginProcessor.cpp`,
`getPresets()`), selectable either from the in-plugin preset picker in the
editor's header or from the host's own preset menu (in Logic: the preset
field at the top of the plugin window) — both drive the same underlying
program list, so they always stay in sync with each other:

- **Default** — Pitch L/R +300 cents (matching sign — a mono-compatible
  pitch-up, not a wide microshift), Focus at its own 150Hz default, Mix 50%.
  See "JJ Dark Vocal (Up)" below for where +300 cents comes from.
- **JJ Cale Vocal** — width turned way down (Pitch L/R ±4 ct) rather than
  off, for a touch of doubling glue without an obvious wide effect, plus a
  single delayed repeat from Shift's own widened Delay (95/105 ms, offset
  L/R) instead of a separate slapback layer, with Focus dropped to 40 Hz so
  the repeat covers essentially the whole vocal rather than just the highs,
  and Mix at 30% (up from the old 18%, since Mix now carries both the width
  and the echo). Aimed at an intimate, laid-back, close vocal rather than a
  wide/shimmery one.
- **Cajun Moon Vocal** — retuned against an actual reference recording
  (`example/cajunmoon_vocal.mp3`) rather than guessed. Analysing it
  (spectrogram, autocorrelation-based pitch tracking, stereo correlation)
  found no discrete slapback echo, no strong deliberate vibrato (the
  measured pitch movement was just natural vocal phrasing), and one clear,
  dominant trait: a heavily rolled-off, dark/warm tone. So the preset is now
  width off, a light touch of Vibrato (1.1 Hz, 3.5 ms depth, 15% mix —
  present but no longer the main event), and **Warmth on** (Tone 2.8 kHz,
  Drive 25%, Mix 70%) carrying the actual character.
- **JJ Dark Vocal** — originally built from a second, more targeted
  comparison: two takes of the same performance,
  `example/cajunmoon_vocal_vocal_1.mp3` and `example/cajunmoon_vocal_vocal_2.mp3`.
  **Note:** the file/character mapping used at the time (vocal_1 = "dark",
  vocal_2 = "normal") was later confirmed backwards — vocal_1 is the normal
  take, vocal_2 the processed/"dark" one (see "JJ Dark Vocal (Up)" below,
  which targets the corrected, literal direction). This preset predates
  that correction and pitches *down* rather than up; kept as-is as a
  deliberately different, deeper take on "dark" rather than removed, since
  it's still a reasonable, independently useful direction. What the
  original analysis got right regardless of file labels: overall top-end
  rolloff was nearly identical between the two takes (spectral tilt
  ~−6.2 dB/oct either way) — top-end darkness wasn't what distinguished
  them — while low-mid "chest" body and fundamental pitch did differ
  substantially. This preset uses Pitch L/R at **−300 cents** with Focus
  turned down to 25 Hz and Mix at 100% (so the shift covers the full band,
  not just what's above the usual 150Hz Focus default), plus **Warmth on**
  with **Body** engaged (Tone 2.8 kHz, Drive 25%, Body 70%, Mix 70%) for a
  lower, chestier voice, with the same light Cajun-Moon-style vibrato as a
  finishing touch.
- **JJ Dark Vocal (Up)** — the corrected, literal vocal_1 (normal) →
  vocal_2 (processed) match. Cross-correlating resampled vocal_1 windows
  against time-aligned vocal_2 windows (which sidesteps the octave-error
  risk of per-file absolute pitch tracking) put vocal_2 consistently
  *above* vocal_1 in pitch, by roughly 2–3 semitones (noisy per-window
  estimates, median 2.36 st); Pitch L/R at **+300 cents** (a clean,
  easy-to-dial-in round number in that range — also Pitch L/R's own
  parameter default, see Controls above) is used here instead of JJ Dark
  Vocal's −300. Same Focus-down/Mix-100% full-band setup as JJ Dark Vocal.
  Warmth's **Body** is left at 0% rather than boosted, since vocal_2's own
  80–160Hz band actually sat *below* vocal_1's, not above it — unlike JJ
  Dark Vocal, this preset doesn't add chest fullness. Tone/Drive/Mix are
  carried over unchanged for the same rolled-off top end both presets
  share.
- **Octave Width** — a stereo width effect using Shift's independent L/R at
  the full-octave end of its range rather than the cents-level microshift
  territory Default/JJ Cale Vocal live in: the left channel drops a full
  octave (Pitch L −1200 ct), the right stays at pitch (Pitch R 0 ct),
  blended at 55% so the dry fundamental stays audible under the sub-octave
  layer, with Focus down low so the drop covers the full band. Reads more
  like an old octave pedal panned across the stereo field than a chorus-y
  doubler. Vibrato and Warmth both stay off so the technique reads clearly
  on its own.
- **Slapback Twang** — the classic rockabilly move: a bright, dry signal
  with nothing but a single delayed repeat, now built from Shift alone
  (Pitch L/R at 0 — no shift, repeat only — Delay L/R at 110/115 ms,
  matching the old dedicated Slapback section's Time, Focus dropped to
  25 Hz so the repeat covers the full band, Mix at 35%). The one thing
  the old section had that this doesn't: a feedback path, so this reads
  as one clean repeat rather than a couple of decaying echoes. Vibrato and
  Warmth stay off on purpose, so the top end stays open/twangy rather than
  rolling dark.
- **Deep Baritone** — JJ Dark Vocal pushed further into effect territory
  rather than a natural-sounding voice: Pitch L/R at **−700 cents** (vs JJ
  Dark Vocal's −300) with Warmth's Body and Drive both turned up further,
  for a growly, monster-movie-trailer low end. No Vibrato here (unlike the
  other dark/warm presets) — a wobble reads as comic rather than menacing
  at this depth.
- **Lies** — built from analyzing `example/lies_1.mp3` (dry) against
  `example/lies_2.mp3` (processed) the same way JJ Dark Vocal was:
  autocorrelation pitch-tracking found the right channel shifted up by a
  measured **+470 cents** (154Hz to 202Hz median), while the left channel's
  level collapsed to near-total digital silence in the processed file. That
  silenced channel isn't reproducible with this plugin (Shift has no
  per-channel pan or gain, only pitch and delay), so the preset approximates
  the same *asymmetry* instead, Octave-Width-style: Pitch L stays at 0
  (lies_1's L and R were already nearly identical — only one side got the
  extreme treatment) while Pitch R alone carries the measured +470ct, one
  channel left recognizable against a wildly pitched-up other one. Focus
  dropped low so the shift covers the full band, and Mix pushed to 90%
  (higher than Octave Width's 55%) since here the effect is meant to
  dominate rather than just widen. The large brightness increase measured
  between the two files (spectral centroid 682Hz to 1288Hz) needed no
  separate Warmth stage — an upward shift this size produces that on its
  own. No vibrato or delay signature was found in the reference files, so
  both stay off/default.

To add more presets, extend the `std::array<Preset, N>` returned by
`getPresets()` in `PluginProcessor.cpp` (and bump `N`).

The LFO rate/depth on the width path's modulated delay, the pitch shifter's
grain length, and Vibrato's fixed 9ms center delay are all fixed internally
rather than exposed as parameters — that's the main way this plugin stays
simpler than its references.

## Building

Requires Xcode (command line tools), CMake ≥ 3.22. JUCE 9.0.1 is fetched
automatically via CMake `FetchContent` — no manual JUCE install needed.

```sh
cmake -B build -G Xcode -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

This builds three targets (four with the AAX SDK in place — see
[RELEASE.md](RELEASE.md#aax-pro-tools)):

- **AU** and **VST3** — automatically copied to
  `~/Library/Audio/Plug-Ins/Components` and
  `~/Library/Audio/Plug-Ins/VST3` (via `COPY_PLUGIN_AFTER_BUILD`). Restart
  Logic (or rescan plugins) to see **jj-breeze** under Audio Units >
  Gerov.
- **Standalone** — a runnable app for quick testing without opening a DAW,
  at `build/jj_breeze_artefacts/Release/Standalone/jj-breeze.app`.

### Scripts

- **`scripts/aax-sdk.sh`** — prints the path to the AAX (Pro Tools) SDK,
  unpacking `AAX/aax-sdk-<version>.zip` on first use. Every script below calls
  it and builds the AAX format too when it succeeds; when there is no SDK it
  exits quietly and the build is AU/VST3/Standalone as always. Each of those
  scripts also takes `--no-aax` to skip AAX even when the SDK is there. The
  SDK is Avid's and is not committed — drop your download into `AAX/`. Note
  that Pro Tools only loads AAX plugins that have been PACE-signed, which none
  of this does; see the [AAX section of RELEASE.md](RELEASE.md#aax-pro-tools).

- **`scripts/install-local.sh`** — builds AU, VST3 and Standalone and
  installs them into the standard per-user locations
  (`~/Library/Audio/Plug-Ins/Components`, `~/Library/Audio/Plug-Ins/VST3`,
  `~/Applications`) for testing in a DAW on this machine. Runs `auval` on
  the AU afterwards if available.

  ```sh
  scripts/install-local.sh                    # Release build
  scripts/install-local.sh --debug             # Debug build
  scripts/install-local.sh --clean              # wipe build/ first
  scripts/install-local.sh --no-standalone       # skip the Standalone app
  scripts/install-local.sh --no-aax               # skip the AAX build
  ```

- **`scripts/dist-macos.sh`** — builds a Release and packages AU, VST3 and
  Standalone into `dist/jj-breeze-<version>-macos.zip`, for someone who'll
  drag each artefact into place themselves. Signs the artefacts with
  `CODESIGN_IDENTITY` (defaults to the "Developer ID Application" identity —
  pass `CODESIGN_IDENTITY=` empty to skip signing).

  ```sh
  scripts/dist-macos.sh                                    # sign with the default identity
  scripts/dist-macos.sh --clean                              # wipe build/ first
  scripts/dist-macos.sh --no-aax                              # skip the AAX build
  CODESIGN_IDENTITY= scripts/dist-macos.sh                    # skip signing
  CODESIGN_IDENTITY="Developer ID Application: Name (TEAMID)" scripts/dist-macos.sh
  ```

- **`scripts/dist-macos-pkg.sh`** — the same build, but assembled into a
  proper double-click `dist/jj-breeze-<version>-macos.pkg` installer instead
  of a zip: signs each artefact, then `pkgbuild`/`productbuild` a system-wide
  installer (AU → `/Library/Audio/Plug-Ins/Components`, VST3 →
  `/Library/Audio/Plug-Ins/VST3`, Standalone → `/Applications`), signed with
  a separate "Developer ID Installer" identity (a different identity *type*
  than the one used for the binaries themselves — Gatekeeper checks both).
  `--notarize` submits the finished `.pkg` to Apple's notary service and
  staples the ticket, so it installs with no Gatekeeper warning at all on
  another Mac; that needs a one-time `xcrun notarytool store-credentials`
  setup first (see the script's header comment) and talks to Apple's
  servers, so it's opt-in rather than part of a routine build.

  ```sh
  scripts/dist-macos-pkg.sh                    # build + sign the .pkg
  scripts/dist-macos-pkg.sh --clean              # wipe build/ first
  scripts/dist-macos-pkg.sh --notarize            # also notarize + staple
  scripts/dist-macos-pkg.sh --no-aax               # skip the AAX component
  APP_SIGN_IDENTITY= PKG_SIGN_IDENTITY= scripts/dist-macos-pkg.sh   # unsigned .pkg
  ```

## Project layout

```
CMakeLists.txt              JUCE plugin target (AU, VST3, Standalone, + AAX with the SDK)
AAX/                        Drop Avid's aax-sdk-<version>.zip here to build AAX (gitignored)
scripts/
  aax-sdk.sh                 Locate/unpack the AAX SDK; prints its path, exits 1 without one
  install-local.sh           Build + install AU/VST3/Standalone locally for DAW testing
  dist-macos.sh               Build + package AU/VST3/Standalone into a distributable zip
  dist-macos-pkg.sh           Build + package AU/VST3/Standalone into a signed .pkg installer
Source/
  PluginProcessor.h/.cpp    Parameters (APVTS) + the per-block audio path
  PluginEditor.h/.cpp       GUI: Shift knobs, plus separate Vibrato and Warmth sections
  DSP/
    PitchShifter.h           Dual-tap crossfaded delay-line pitch shifter, ±1200 cents
    ModulatedDelay.h         Short delay + LFO wobble (used by Width, Vibrato, and Shift's Delay L/R)
    Warmth.h                 Low-shelf body boost + low-pass + soft saturation tone stage on the final output
```
