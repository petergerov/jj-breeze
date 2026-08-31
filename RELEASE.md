# Releasing jj-breeze

How to cut a distributable build of jj-breeze for macOS and Windows, locally
or on GitHub Actions, and how to confirm it installs on someone else's machine
without a security warning.

For day-to-day development builds see the **Building** section of
[README.md](README.md) — this document only covers shipping.

## What ships

| Platform | Formats | Installer | Installs to |
| --- | --- | --- | --- |
| macOS (Apple Silicon) | AU, VST3, Standalone (+ AAX) | `dist/jj-breeze-<version>-macos.pkg` | `/Library/Audio/Plug-Ins/Components`, `/Library/Audio/Plug-Ins/VST3`, `/Applications` (+ `/Library/Application Support/Avid/Audio/Plug-Ins`) |
| Windows (x64) | VST3, Standalone (+ AAX) | `dist/jj-breeze-<version>-windows.exe` | `C:\Program Files\Common Files\VST3`, `C:\Program Files\Gerov\j.j.breeze` (+ `C:\Program Files\Common Files\Avid\Audio\Plug-Ins`) |

AU is Apple-only; JUCE's CMake API drops it automatically on Windows, so the
`FORMATS` list in `CMakeLists.txt` stays the same for both platforms.

AAX is in brackets because it is conditional: it is built and packaged only on
a machine that has Avid's AAX SDK, and it needs one further step (PACE signing)
before Pro Tools will load it. See [AAX (Pro Tools)](#aax-pro-tools) below.

There is also `scripts/dist-macos.sh`, which zips the three macOS artefacts
as-is for someone who would rather drag them into place themselves. It is not
part of the flow below.

## Version numbers

`CMakeLists.txt` is the single source of truth:

```cmake
project(jj-breeze VERSION 1.0.0)
```

Every packaging script parses that line (and `PRODUCT_NAME`) rather than
hardcoding it, so bumping the version there is the entire version bump — the
installer filenames, the Windows version resource and the macOS package
identifiers all follow. Nothing else needs editing.

`PRODUCT_NAME` is `j.j.breeze`, which is **not** the repo name or the CMake
target name (`jj_breeze`). All three appear in artefact paths, so prefer the
scripts over hand-rolled `cp` commands.

---

## Credentials: `scripts/.env`

Every signing identity, Apple ID and password the local scripts need lives in
one gitignored file, `scripts/.env`. No credential is hardcoded in a committed
script — a fresh clone signs nothing until you create it.

Create it by copying the block below and filling in your own values:

```sh
# --- macOS code signing ---
# Exact identity names, as printed by:  security find-identity -v
APP_SIGN_IDENTITY="Developer ID Application: <your name> (<team id>)"
PKG_SIGN_IDENTITY="Developer ID Installer: <your name> (<team id>)"
CODESIGN_IDENTITY="Developer ID Application: <your name> (<team id>)"

# --- macOS notarization ---
APPLE_ID="<your apple id>"
APPLE_TEAM_ID="<your 10-character team id>"
APPLE_APP_PASSWORD="xxxx-xxxx-xxxx-xxxx"
NOTARY_PROFILE="notary-profile"

# --- Windows code signing ---
WINDOWS_CERT_PFX=""
WINDOWS_CERT_PASSWORD=""
```

| Variable | Used by | Empty or unset means |
| --- | --- | --- |
| `APP_SIGN_IDENTITY` | `dist-macos-pkg.sh` | binaries left unsigned |
| `PKG_SIGN_IDENTITY` | `dist-macos-pkg.sh`, `notarize-macos-pkg.sh` | `.pkg` left unsigned |
| `CODESIGN_IDENTITY` | `dist-macos.sh` | artefacts in the zip left unsigned |
| `APPLE_ID`, `APPLE_TEAM_ID`, `APPLE_APP_PASSWORD` | `notarize-macos-pkg.sh` | notarization refuses to start |
| `NOTARY_PROFILE` | `dist-macos-pkg.sh --notarize` | `--notarize` refuses to start |
| `WINDOWS_CERT_PFX`, `WINDOWS_CERT_PASSWORD` | `dist-windows.ps1` | installer left unsigned |

Notes:

- **A variable already set in the environment always wins over the file.** That
  keeps the documented override working —
  `APP_SIGN_IDENTITY= scripts/dist-macos-pkg.sh` still forces an unsigned
  build — and means CI, which sets real environment variables and ships no
  `.env`, is never shadowed by it.
- `scripts/load-env.sh` is the shared loader for the bash scripts;
  `dist-windows.ps1` reads the same file itself.
- The file holds an app-specific password in plain text. If you would rather
  not, leave `APPLE_APP_PASSWORD` empty and use `NOTARY_PROFILE` instead — that
  keeps the password in the login keychain (see *Notarize* below).
- GitHub Actions does **not** read this file. CI credentials are repository
  secrets; see *GitHub Actions* below.

---

## macOS

### Prerequisites

- Xcode command line tools, CMake ≥ 3.22
- Two *different* Developer ID certificates in the login keychain:
  - **Developer ID Application** — signs the AU, VST3 and app binaries
  - **Developer ID Installer** — signs the `.pkg` wrapper

  Check with `security find-identity -v`, and put the exact identity names
  in `scripts/.env`. Certificates expire — `security find-certificate -c
  "Developer ID Installer" -p | openssl x509 -noout -enddate` tells you when.
- An **app-specific password** for notarization, generated at
  appleid.apple.com under *Sign-In and Security → App-Specific Passwords*.
  This is not your Apple ID password.

An "Apple Development" identity is not a substitute for either — it is only
good for local testing and Gatekeeper rejects it on other machines.

### 1. Build and sign

```sh
scripts/dist-macos-pkg.sh
```

Builds Release, signs each artefact with the Developer ID Application identity
(hardened runtime + secure timestamp), then assembles a signed
`dist/jj-breeze-<version>-macos.pkg` with a three-checkbox installer UI.

```sh
scripts/dist-macos-pkg.sh --clean                                  # wipe build/ first
APP_SIGN_IDENTITY= PKG_SIGN_IDENTITY= scripts/dist-macos-pkg.sh    # unsigned, for local testing only
```

### 2. Notarize

**A signature alone is not enough.** Since macOS 10.15, Gatekeeper rejects a
correctly signed but un-notarized installer. This is what a properly signed,
un-notarized package looks like:

```
$ pkgutil --check-signature dist/jj-breeze-1.0.0-macos.pkg
   Status: signed by a developer certificate issued by Apple for distribution
   1. Developer ID Installer: <your name> (<team id>)

$ spctl --assess -vv --type install dist/jj-breeze-1.0.0-macos.pkg
   rejected
   source=Unnotarized Developer ID
```

Notarization submits the package to Apple, which scans it and issues a ticket.
**Stapling** attaches that ticket to the file, so Gatekeeper can verify it
without contacting Apple — without stapling, a Mac with no network connection
shows the warning again.

```sh
scripts/notarize-macos-pkg.sh
```

`APPLE_ID`, `APPLE_TEAM_ID` and `APPLE_APP_PASSWORD` come from `scripts/.env`;
setting them inline for a one-off run works too and takes precedence.

With no arguments it picks the newest `.pkg` in `dist/`. It signs the package
first if it isn't signed yet, runs a local preflight, submits, staples and
verifies.

The **preflight** checks every bundle in the payload for a Developer ID
signature, the hardened runtime and a secure timestamp. Apple rejects
submissions missing any of those, but only says so in a separate log fetched
minutes later — the preflight turns that into an immediate, specific error.
Pass `--skip-preflight` to bypass it.

If you would rather not put credentials in the environment each time, store
them once and use the profile instead:

```sh
xcrun notarytool store-credentials "notary-profile" \
    --apple-id <your-apple-id> --team-id <your-team-id> --password <app-specific-password>

# then, in scripts/.env, leave APPLE_APP_PASSWORD empty and set:
#   NOTARY_PROFILE="notary-profile"
```

`scripts/dist-macos-pkg.sh --notarize` does build-sign-notarize in one shot,
but only supports the stored-profile form.

### 3. Verify

`notarize-macos-pkg.sh` ends with this check, but to confirm by hand:

```sh
xcrun stapler validate dist/jj-breeze-1.0.0-macos.pkg
spctl --assess -vv --type install dist/jj-breeze-1.0.0-macos.pkg
```

You want `source=Notarized Developer ID`. Anything else means the installer
will warn on another Mac.

---

## Windows

### Prerequisites

- Visual Studio 2022 with the C++ workload, CMake ≥ 3.22
- **Inno Setup 6.3 or newer** — <https://jrsoftware.org/isdl.php>
- Optionally an Authenticode code-signing `.pfx`. There is no notarization
  equivalent on Windows; the analogue is simply an Authenticode signature,
  ideally from an EV certificate, which is what builds SmartScreen reputation.

Visual Studio **2026 will not work** with these scripts as written — see
Troubleshooting.

### Build

```powershell
scripts\dist-windows.ps1
```

Builds Release and compiles `installer/windows/jj-breeze.iss` into
`dist\jj-breeze-<version>-windows.exe`, offering VST3 and Standalone as
separate components with a Start-menu entry and an uninstaller.

```powershell
scripts\dist-windows.ps1 -Clean
scripts\dist-windows.ps1 -CertPath C:\certs\gerov.pfx -CertPassword <password>
```

With a certificate it signs the two binaries *and* the installer separately —
an unsigned installer wrapping signed payloads still trips SmartScreen at
download time.

Without one it produces a working but unsigned installer; SmartScreen will
warn on other machines.

### A note on the VST3 layout

On Windows a VST3 is a bundle *directory*
(`j.j.breeze.vst3\Contents\x86_64-win\j.j.breeze.vst3`), not a single DLL. The
installer recurses the whole tree, and `signtool` targets the DLL *inside* the
bundle. Anything that treats `j.j.breeze.vst3` as one file will be wrong.

### AppId

`installer/windows/jj-breeze.iss` carries a fixed `AppId` GUID. **Never change
it.** Windows keys upgrades and uninstalls off that value; a new one would
leave the previous version installed alongside the new one instead of
replacing it.

---

## AAX (Pro Tools)

AAX is optional throughout. Nothing about it is required to build or ship the
formats above, and a checkout without the SDK behaves exactly as it did before
AAX existed here — no error, no extra flag.

### Getting the SDK in place

The AAX SDK is Avid's, under an NDA-ish licence, and may not be redistributed,
so it is not committed. Download `aax-sdk-<version>.zip` from your Avid
developer account (<https://developer.avid.com>) and drop it into `AAX/`:

```
AAX/aax-sdk-2-9-0.zip
```

That is the whole setup. `scripts/aax-sdk.sh` (`scripts/aax-sdk.ps1` on
Windows) unpacks it next to itself on first use and prints the path; every
build script calls it and passes the result to CMake as
`-DJJ_BREEZE_AAX_SDK_PATH=`. Both the zip and the unpacked directory are
gitignored.

```sh
scripts/aax-sdk.sh          # prints the SDK path, unpacking on first run
                            # exits 1 (and builds stay AU/VST3/Standalone) if there is none
AAX_SDK_PATH=/elsewhere/aax-sdk-2-9-0 scripts/dist-macos-pkg.sh   # use an SDK from elsewhere
```

With the SDK present these all pick AAX up on their own, and each takes
`--no-aax` (`-NoAax` on Windows) to leave it out again:

| Script | What it does with AAX |
| --- | --- |
| `scripts/install-local.sh` | Builds it; prints the elevated `cp` to install it (see below) |
| `scripts/dist-macos.sh` | Adds `j.j.breeze.aaxplugin` to the zip |
| `scripts/dist-macos-pkg.sh` | Adds an "AAX (Pro Tools)" component to the `.pkg` |
| `scripts/dist-windows.ps1` | Adds an "AAX plug-in (Pro Tools)" component to the installer |

### PACE signing — the part that is missing

**Pro Tools will not load the `.aaxplugin` these scripts produce.** Every AAX
plugin, including a locally built one, has to be signed with Avid's PACE
`wraptool` against a developer certificate before any shipping Pro Tools will
even list it. That needs an Avid developer account and an Eden/iLok signing
certificate, neither of which this repo has, so no build script attempts it.

Once you have those, the step belongs immediately after the build and before
packaging, roughly:

```sh
wraptool sign --verbose \
    --account   <avid-developer-account> \
    --wcguid    <your-wrap-config-guid> \
    --signid    "Developer ID Application: Petar Gerov (C9LBGZNZ6P)" \
    --in  build/jj_breeze_artefacts/Release/AAX/j.j.breeze.aaxplugin \
    --out build/jj_breeze_artefacts/Release/AAX/j.j.breeze.aaxplugin
```

Until then the AAX artefact is good for a development build of Pro Tools only,
and every script that produces one says so on the way out.

### Install locations

There is no per-user AAX folder — Pro Tools scans exactly one machine-wide
path, which is why `install-local.sh` can place the AU and VST3 but only
*prints* the AAX copy for you to run under `sudo`:

| Platform | Path |
| --- | --- |
| macOS | `/Library/Application Support/Avid/Audio/Plug-Ins` |
| Windows | `C:\Program Files\Common Files\Avid\Audio\Plug-Ins` |

For the same reason `CMakeLists.txt` clears JUCE's `JUCE_AAX_COPY_DIR`: the
automatic post-build copy that works for AU and VST3 would need elevation for
AAX and would fail every build. The installers place it instead.

---

## GitHub Actions

All four workflows are `workflow_dispatch` only — run them from the Actions
tab. Nothing triggers on push or tag.

| Workflow | Runner | Produces |
| --- | --- | --- |
| Build macOS (Apple Silicon) | `macos-15` | `jj-breeze-au-macos-arm64`, `jj-breeze-vst3-macos-arm64`, `jj-breeze-standalone-macos-arm64` |
| Build Windows | `windows-2022` | `jj-breeze-vst3-windows`, `jj-breeze-standalone-windows` |
| Package macOS installer (.pkg) | `macos-15` | `jj-breeze-macos-installer` |
| Package Windows installer (.exe) | `windows-2022` | `jj-breeze-windows-installer` |

The two *Build* workflows are CI checks — the macOS one installs the AU and
runs `auval` on it. The two *Package* workflows produce the shippable
installers.

Both runners are pinned deliberately rather than using `-latest`; see
Troubleshooting for why.

### Secrets

Both package workflows degrade gracefully: with no secrets configured they
still produce a working installer, just an unsigned one. Configure these under
*Settings → Secrets and variables → Actions* for a shippable build.

**macOS** (`package-macos.yml`):

| Secret | Contents |
| --- | --- |
| `MACOS_APP_CERT_P12` | base64 of the Developer ID **Application** `.p12` |
| `MACOS_INSTALLER_CERT_P12` | base64 of the Developer ID **Installer** `.p12` |
| `MACOS_CERT_PASSWORD` | the password both `.p12` files were exported with |
| `APPLE_ID` | Apple ID owning the certificates |
| `APPLE_TEAM_ID` | your 10-character Apple Developer team ID |
| `APPLE_APP_PASSWORD` | app-specific password |

Export both certificates from Keychain Access, then
`base64 -i cert.p12 | pbcopy`. The `APPLE_*` three are only used when the
workflow's **notarize** input is ticked.

**Windows** (`package-windows.yml`):

| Secret | Contents |
| --- | --- |
| `WINDOWS_CERT_PFX_B64` | base64 of an Authenticode code-signing `.pfx` |
| `WINDOWS_CERT_PASSWORD` | the password that `.pfx` was exported with |

The `_B64` suffix is deliberate: `dist-windows.ps1` reads `WINDOWS_CERT_PFX`
as a filesystem *path*, so reusing that name for base64 content would make the
script fail with a confusing "certificate not found".

Certificates are imported into a throwaway keychain (macOS) or written to the
runner's temp directory and deleted afterwards (Windows) — never into the
workspace, where they could be swept into an uploaded artifact.

---

## Verification checklist

Before handing an installer to anyone:

- [ ] Version in `CMakeLists.txt` bumped, and the installer filename reflects it
- [ ] macOS: `spctl --assess -vv --type install` reports `source=Notarized Developer ID`
- [ ] macOS: `xcrun stapler validate` passes
- [ ] macOS: `auval -v aufx Jjbz Grov` passes after installing (the AU must be
      installed first — `auval` only sees registered components)
- [ ] Windows: installer runs, VST3 appears in `C:\Program Files\Common Files\VST3`
- [ ] Both: plugin loads and scans clean in an actual DAW
- [ ] Installed over a previous version, not just onto a clean machine

---

## Troubleshooting

**`auval` says "didn't find the component"**
`auval` only inspects *registered* Audio Units; it cannot be pointed at a
bundle in the build tree. Install it first — `scripts/install-local.sh` does
this, and clears `~/Library/Caches/AudioUnitCache/com.apple.audiounits.cache`,
which otherwise keeps reporting a stale name after a `PRODUCT_NAME` change.

**Notarization comes back "Invalid" with no reason**
The submit output never explains itself; the reason is in a separate log:

```sh
xcrun notarytool log <submission-id> --apple-id ... --team-id ... --password ...
```

`notarize-macos-pkg.sh` fetches this automatically on failure. The usual cause
is a binary missing the hardened runtime or a secure timestamp, which the
preflight catches before the round trip.

**Windows: "could not find any instance of Visual Studio"**
Between 2026-06-08 and 2026-06-15, GitHub migrated the `windows-latest` and
`windows-2025` labels to an image carrying Visual Studio **2026** (v18)
instead of 2022 (v17), so the `Visual Studio 17 2022` generator stopped
resolving. Both Windows workflows are therefore pinned to `windows-2022`.

Moving to `windows-latest` later means changing the generator to
`Visual Studio 18 2026` in *both* `build-windows.yml` and
`scripts/dist-windows.ps1`, and checking that the CMake on PATH is ≥ 4.1 —
older CMake does not know that generator name at all.
See [actions/runner-images#14017](https://github.com/actions/runner-images/issues/14017).

**macOS runner images**
`macos-14` is deprecated; both macOS workflows are pinned to `macos-15`.
`macos-26` is also GA and is what `macos-latest` now points at. The pin is
deliberate — it keeps the build reproducible and guaranteed Apple Silicon.

**`M_PI` undeclared on MSVC**
`M_PI` is a POSIX extension, not standard C++. Apple's headers define it
unconditionally; MSVC only does so if `_USE_MATH_DEFINES` is set before
`<cmath>`. Use `std::numbers::pi` from `<numbers>` — the project is C++20, and
it keeps the DSP headers free of both platform defines and JUCE.

**Signing "succeeds" when you asked for no signing**
`${VAR:=default}` in bash substitutes the default for an *empty* value too,
not just an unset one, which silently re-enables signing. The dist scripts use
`${VAR=default}` for exactly this reason. Do not add the colon back.

---

## Known gaps

Recorded honestly so nobody assumes more coverage than exists:

- The **Windows installer has never been built end-to-end.** The CMake build
  on `windows-2022` is proven, but `dist-windows.ps1` past the build step, the
  Inno Setup compile, and the resulting installer are untested.
- The **notarization round trip has never run.** Signing, the preflight (both
  its pass and fail paths) and the credential handling in
  `notarize-macos-pkg.sh` are verified; submit, staple and the final Gatekeeper
  assessment need real Apple credentials and have not been exercised.
- The **AAX build is compile-and-package only.** The macOS `.aaxplugin` builds,
  signs and lands in both the zip and the `.pkg`; it has never been loaded in
  Pro Tools, because that needs the PACE `wraptool` signing described above.
  The Windows AAX path (Inno Setup component, `signtool` on the bundle's inner
  binary) is untested for the same reason the rest of the Windows installer is.
- No release has been tagged. `git tag` is empty, and no workflow triggers on
  tags — versioning is whatever is in `CMakeLists.txt` at build time.
