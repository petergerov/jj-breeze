# jj-breeze

**Homepage and downloads: https://petergerov.github.io/jj-breeze/** — the page
in [docs/](docs/), served by GitHub Pages. It offers the macOS and Windows
installers from the newest GitHub release (free, no account), and describes
what the plug-in does for people who won't read this file.

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
| **Pitch L** | −1200 to +1200 cents | Pitch shift applied to the left channel. Default +300 (up) — see JJ Moon under Factory presets below. Skewed so fine micro-detune territory near 0 still gets most of the knob's travel, but the full ±1 octave is reachable. |
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

Between Pitch L and R, and between Delay L and R, sits a **link switch**.
While it's lit, turning either knob of the pair moves the other by the same
amount (clamped to its range) — so a ±8 ct spread or a 12/18 ms offset can
be moved as a unit. The links are saved with the host project but not in
presets, and they only follow your hand: automation or a preset load moves
each knob on its own.

The editor's header carries a few more workflow controls, top to bottom:

- A **preset** picker (see Factory presets below) covering the eighteen
  factory presets, grouped by category with a one-line summary each, and
  any **user presets** you save. User presets live on disk
  (`~/Library/Gerov/jj-breeze/Presets`, independent of
  the fixed factory list, so the host-visible preset count never changes) —
  `presets/` in this repo holds preset XML in that same format, copyable
  straight into that folder to use without rebuilding.
  - **SAVE** with a user preset loaded offers **Save "name"** (writes the
    knobs back into it, no retyping) and **Save As…** (a new name); with a
    factory preset loaded it goes straight to Save As…. The trash icon
    deletes the loaded user preset (disabled for factory presets).
  - The **red dot** at the left of the preset window lights once you've
    moved anything away from the loaded preset. Click it while lit to go
    back to the preset as it was loaded or last saved (it asks first, and
    the revert is a single undo step); while dark it just opens the picker.
    POWER off doesn't count as an edit.
  - Which preset is loaded is saved with the host project, so a reopened
    session still shows its name — and the dot, if it was left edited.
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

Eighteen factory presets (`Source/PluginProcessor.cpp`, `getPresets()`) —
the same lineup as the AUv3 sibling. The editor's picker lists them by
category, each with a one-line summary; the host's own preset menu (in
Logic: the preset field at the top of the plugin window) lists the same
programs as one flat list, and both stay in sync.

| Category | Presets |
|---|---|
| Start | Init |
| Vocal | JJ Moon, JJ Breeze, JJ Dark Vocal, JJ Lies, Subtle Doubler |
| Width | Stereo Width, Wide Guitar, Bass-Safe Width, Haas Width, Octave Width |
| Character | Tape Wobble, Old Radio, Seasick |
| Echo | Slapback Twang, 50s Slap Wide |
| FX | Fifth Up, Deep Baritone |

A new instance starts on **JJ Moon**. The old **Default** preset was an
exact copy of JJ Moon and is gone; JJ Moon now sits at program 0, so a
host recalling program 0 gets the same sound it always did. The other
programs' host indices changed in this lineup — sessions don't depend on
them (the full patch is saved with the project), but anything that selects
presets by number, such as a program-change automation lane, will need
re-pointing.

The presets that started on the desktop keep the values they were built
with here (the AUv3 retuned a few of its own — JJ Dark Vocal and Deep
Baritone differ between the two); the rest are carried over from the AUv3
as-is. Notes on the ones built from reference recordings:

- **JJ Moon** — retuned against an actual reference recording
  (`example/cajunmoon_vocal.mp3`) rather than guessed. Analysing it
  (spectrogram, autocorrelation-based pitch tracking, stereo correlation)
  found no discrete slapback echo, no strong deliberate vibrato (the
  measured pitch movement was just natural vocal phrasing), and one clear,
  dominant trait: a heavily rolled-off, dark/warm tone. So Vibrato is a
  light touch (1.2 Hz, 3 ms, 15%) and Warmth carries the character. Pitch
  L/R at +300 cents with matching signs — a mono-compatible pitch-up rather
  than a wide microshift — at a low 13% Mix.
- **JJ Dark Vocal** — built from comparing two takes of the same
  performance (`example/cajunmoon_vocal_vocal_1.mp3` and `_vocal_2.mp3`).
  Pitches *down* (−300 cents) as a deliberately deeper take on "dark", with
  Focus at 25 Hz so the shift covers the full band, and Warmth's **Body**
  at 70% for the low-mid "chest" fullness the analysis found, plus the same
  light vibrato as JJ Moon.
- **JJ Breeze** — built from analysing
  `example/call_me_the_breaze_vocal.wav`. Unlike Lies there's no dry
  counterpart to diff against, so every value below comes from measuring the
  reference's own traits and then calibrating each control against them by
  running a dry vocal (`example/lies_1.mp3`, mono-summed) through a port of
  this plugin's exact DSP chain:
  - **Width without detune.** Stereo correlation +0.81 and a side/mid ratio of
    0.35 say "wide", but per-frame cross-correlation puts the L/R time offset
    at 0.00 ms and pitch tracking puts the L/R pitch difference at 0.0 cents —
    and the width is uniform across every octave (side/mid 0.33 to 0.39 from
    80 Hz to 20 kHz) rather than highs-only. Pitch L at **−8 cents** and Pitch
    R at **+8 cents**, with **Mix 35%**, reproduce that measurement
    (calibrated result: +0.78 correlation, 0.354 side/mid, and 0.0 cents of
    *measured* L/R difference — at 35% the unshifted dry still sets each
    channel's pitch), and **Focus 25 Hz** keeps the width band-uniform the way
    the reference is instead of the default 150 Hz crossover's highs-only
    widening.
  - **Dark, but only moderately.** 85% of the energy sits below 1.1 kHz and
    the spectrum falls at −7.7 dB/oct across 1–8 kHz, yet it's still twice
    as bright above 2 kHz (5.3% of total energy) as `cajunmoon_vocal.mp3`'s
    2.5%. So Warmth is on but gentle — **Tone 4 kHz, Drive 25%, Mix 30%**
    reproduces that −7.7 dB/oct tilt (calibrated result: −7.6) rather than
    Cajun Moon's much heavier rolloff.
  - **Body stays at 0%.** Warmth's shelf lifts 150 Hz and below, and the
    reference has almost nothing there (0.1% under 100 Hz, 0.4% under
    150 Hz); its 200 Hz–1 kHz slope is also *shallower* than the dry
    reference vocal's, so there's no low-end lift to match — the warmth here
    is top-end rolloff only.
  - **No vibrato, no echo.** Sustained notes hold within ±10 cents once they
    settle (the apparent ~1.5 Hz movement is phrasing into and out of each
    note — the same false positive the Cajun Moon analysis turned up), and
    the envelope autocorrelation decays smoothly with no repeat peak
    anywhere between 15 and 400 ms. Vibrato stays off and Delay L/R stay in
    width territory (12/18 ms) rather than slapback territory.

  One measured trait deliberately not reproduced: the reference's left
  channel is ~2.3 dB louder than its right. Shift has no per-channel gain,
  and a static level tilt isn't the character anyway.
- **JJ Lies** — built from analysing `example/lies_1.mp3` (dry) against
  `example/lies_2.mp3` (processed): pitch tracking found the right channel
  shifted up by a measured **+470 cents** (154 Hz to 202 Hz median), while
  the left channel collapsed to near-silence. That silenced channel isn't
  reproducible here (Shift has no per-channel gain), so the preset keeps
  the *asymmetry* instead: Pitch L at 0, Pitch R at +470 ct, Focus low so
  the shift covers the full band. The brightness increase measured between
  the files needed no Warmth — an upward shift this size produces it.
- **Octave Width** — the left channel drops a full octave (Pitch L −1200 ct),
  the right stays at pitch, blended low so the dry fundamental stays audible
  under the sub-octave layer. More like an old octave pedal panned across the
  stereo field than a chorus-y doubler.
- **Slapback Twang** — the classic rockabilly move, built from Shift alone:
  Pitch L/R at 0 (repeat only), Delay L/R at 110/115 ms, Focus at 25 Hz so
  the repeat covers the full band, Mix 35%. Shift's delay has no feedback
  path, so this is one clean repeat rather than a series of echoes.
- **Deep Baritone** — JJ Dark Vocal pushed into effect territory: Pitch L/R
  at −700 cents with Warmth's Body and Drive turned further up, for a
  growly low end. No vibrato — a wobble reads as comic at this depth.

To add more presets, extend the `std::array<Preset, N>` returned by
`getPresets()` in `PluginProcessor.cpp` (and bump `N`), giving each a
category from `getPresetCategories()` and a one-line summary. Add new
presets at the end, so existing host program numbers don't move. Keep `N`
equal to the number of entries actually listed: a mismatch leaves
zero-filled phantom presets in the host's program list that silently zero
every parameter when selected.

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

This builds three targets:

- **AU** and **VST3** — automatically copied to
  `~/Library/Audio/Plug-Ins/Components` and
  `~/Library/Audio/Plug-Ins/VST3` (via `COPY_PLUGIN_AFTER_BUILD`). Restart
  Logic (or rescan plugins) to see **jj-breeze** under Audio Units >
  Gerov.
- **Standalone** — a runnable app for quick testing without opening a DAW,
  at `build/jj_breeze_artefacts/Release/Standalone/jj-breeze.app`.

### Scripts

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
  ```

- **`scripts/dist-macos.sh`** — builds a Release and packages AU, VST3 and
  Standalone into `dist/jj-breeze-<version>-macos.zip`, for someone who'll
  drag each artefact into place themselves. Signs the artefacts with
  `CODESIGN_IDENTITY` (defaults to the "Developer ID Application" identity —
  pass `CODESIGN_IDENTITY=` empty to skip signing).

  ```sh
  scripts/dist-macos.sh                                    # sign with the default identity
  scripts/dist-macos.sh --clean                              # wipe build/ first
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
  APP_SIGN_IDENTITY= PKG_SIGN_IDENTITY= scripts/dist-macos-pkg.sh   # unsigned .pkg
  ```

## Project layout

```
CMakeLists.txt              JUCE plugin target (AU, VST3, Standalone)
scripts/
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
