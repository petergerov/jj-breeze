#!/usr/bin/env bash
#
# dist-macos-pkg.sh — build jj-breeze (AU, VST3, Standalone) in Release,
# code-sign each artefact, and assemble a signed system-wide .pkg installer
# under dist/:
#   Audio Unit (AU) -> /Library/Audio/Plug-Ins/Components
#   VST3            -> /Library/Audio/Plug-Ins/VST3
#   Standalone app  -> /Applications
#
# (dist-macos.sh instead zips the three artefacts as-is, for someone who'll
# just drag them into place themselves. This produces a real double-click
# installer with an admin-password prompt, matching how most commercial
# plugins ship.)
#
# Usage:
#   scripts/dist-macos-pkg.sh [--clean] [--notarize]
#
#   --clean      Remove build/ first (forces a full rebuild).
#   --notarize   After building the .pkg, submit it to Apple's notary
#                service and staple the ticket so Gatekeeper doesn't warn
#                on other Macs. Requires a one-time setup on this machine:
#                    xcrun notarytool store-credentials "notary-profile" \
#                        --apple-id you@example.com \
#                        --team-id C9LBGZNZ6P \
#                        --password <app-specific-password>
#                (generate the app-specific password at appleid.apple.com —
#                not your regular Apple ID password). This step talks to
#                Apple's servers and can take several minutes; run it
#                separately when you're ready to actually ship, not as part
#                of routine local builds.
#
# Env vars:
#   APP_SIGN_IDENTITY   Identity for signing the AU/VST3/app bundles.
#                        Defaults to "Developer ID Application: Petar Gerov (C9LBGZNZ6P)".
#                        Pass APP_SIGN_IDENTITY= (empty) to skip binary signing
#                        (the .pkg will still build, but won't pass Gatekeeper
#                        on another Mac).
#   PKG_SIGN_IDENTITY   Identity for signing the .pkg itself (a *different*
#                        identity type than the one above — "Developer ID
#                        Installer", not "Developer ID Application").
#                        Defaults to "Developer ID Installer: Petar Gerov (C9LBGZNZ6P)".
#                        Pass PKG_SIGN_IDENTITY= (empty) to skip pkg signing.
#   NOTARY_PROFILE       Keychain profile name for notarytool. Defaults to
#                        "notary-profile" (see --notarize above).

set -euo pipefail

: "${APP_SIGN_IDENTITY:=Developer ID Application: Petar Gerov (C9LBGZNZ6P)}"
: "${PKG_SIGN_IDENTITY:=Developer ID Installer: Petar Gerov (C9LBGZNZ6P)}"
: "${NOTARY_PROFILE:=notary-profile}"

DO_CLEAN=0
DO_NOTARIZE=0
for arg in "$@"; do
    case "$arg" in
        --clean) DO_CLEAN=1 ;;
        --notarize) DO_NOTARIZE=1 ;;
        *)
            echo "error: unknown argument: $arg" >&2
            exit 1
            ;;
    esac
done

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="build"
DIST_DIR="dist"

if [[ "$DO_CLEAN" == "1" ]]; then
    echo "==> Removing existing $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

VERSION="$(grep -Eo 'project\(jj-breeze VERSION [0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt | grep -Eo '[0-9]+\.[0-9]+\.[0-9]+')"
if [[ -z "$VERSION" ]]; then
    echo "error: could not determine version from CMakeLists.txt" >&2
    exit 1
fi

echo "==> Building jj-breeze $VERSION (Release)"
# COPY_PLUGIN_AFTER_BUILD off: this script places the signed artefacts into
# a package instead, so the ad-hoc copy JUCE would otherwise do to this
# machine's own plugin folders is just noise here.
cmake -B "$BUILD_DIR" -G Xcode -DCMAKE_BUILD_TYPE=Release -DJJ_BREEZE_COPY_PLUGIN_AFTER_BUILD=OFF
cmake --build "$BUILD_DIR" --config Release

ARTEFACTS_DIR="$BUILD_DIR/jj_breeze_artefacts/Release"
AU_PATH="$ARTEFACTS_DIR/AU/jj-breeze.component"
VST3_PATH="$ARTEFACTS_DIR/VST3/jj-breeze.vst3"
STANDALONE_PATH="$ARTEFACTS_DIR/Standalone/jj-breeze.app"

for path in "$AU_PATH" "$VST3_PATH" "$STANDALONE_PATH"; do
    if [[ ! -e "$path" ]]; then
        echo "error: expected build artefact not found: $path" >&2
        exit 1
    fi
done

if [[ -n "${APP_SIGN_IDENTITY:-}" ]]; then
    echo "==> Signing artefacts with: $APP_SIGN_IDENTITY"
    for path in "$AU_PATH" "$VST3_PATH" "$STANDALONE_PATH"; do
        codesign --force --deep --options runtime --timestamp \
            --sign "$APP_SIGN_IDENTITY" "$path"
    done
else
    echo "==> APP_SIGN_IDENTITY is empty, skipping binary signing (Gatekeeper will reject this on another Mac)"
fi

# --- Stage one root per component, mirroring the paths each should land at ---
STAGE_DIR="$BUILD_DIR/pkg-staging"
rm -rf "$STAGE_DIR"

AU_ROOT="$STAGE_DIR/au-root/Library/Audio/Plug-Ins/Components"
VST3_ROOT="$STAGE_DIR/vst3-root/Library/Audio/Plug-Ins/VST3"
APP_ROOT="$STAGE_DIR/app-root/Applications"
mkdir -p "$AU_ROOT" "$VST3_ROOT" "$APP_ROOT"
cp -R "$AU_PATH" "$AU_ROOT/"
cp -R "$VST3_PATH" "$VST3_ROOT/"
cp -R "$STANDALONE_PATH" "$APP_ROOT/"

mkdir -p "$DIST_DIR"
PKG_COMPONENTS_DIR="$BUILD_DIR/pkg-components"
rm -rf "$PKG_COMPONENTS_DIR"
mkdir -p "$PKG_COMPONENTS_DIR"

echo "==> Building component packages"
pkgbuild --root "$STAGE_DIR/au-root" --identifier com.gerov.jjbreeze.au \
    --version "$VERSION" --install-location / "$PKG_COMPONENTS_DIR/AU.pkg" >/dev/null
pkgbuild --root "$STAGE_DIR/vst3-root" --identifier com.gerov.jjbreeze.vst3 \
    --version "$VERSION" --install-location / "$PKG_COMPONENTS_DIR/VST3.pkg" >/dev/null
pkgbuild --root "$STAGE_DIR/app-root" --identifier com.gerov.jjbreeze.standalone \
    --version "$VERSION" --install-location / "$PKG_COMPONENTS_DIR/Standalone.pkg" >/dev/null

# --- Distribution XML (lets the installer show three checkboxes, one per
# component, instead of an opaque single blob) + productbuild -> final .pkg ---
DISTRIBUTION_XML="$BUILD_DIR/distribution.xml"
cat > "$DISTRIBUTION_XML" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>jj-breeze $VERSION</title>
    <options customize="allow" require-scripts="false" rootVolumeOnly="true"/>
    <welcome file="welcome.txt"/>
    <choices-outline>
        <line choice="au"/>
        <line choice="vst3"/>
        <line choice="standalone"/>
    </choices-outline>
    <choice id="au" title="Audio Unit (AU)">
        <pkg-ref id="com.gerov.jjbreeze.au"/>
    </choice>
    <choice id="vst3" title="VST3">
        <pkg-ref id="com.gerov.jjbreeze.vst3"/>
    </choice>
    <choice id="standalone" title="Standalone app">
        <pkg-ref id="com.gerov.jjbreeze.standalone"/>
    </choice>
    <pkg-ref id="com.gerov.jjbreeze.au" version="$VERSION" onConclusion="none">AU.pkg</pkg-ref>
    <pkg-ref id="com.gerov.jjbreeze.vst3" version="$VERSION" onConclusion="none">VST3.pkg</pkg-ref>
    <pkg-ref id="com.gerov.jjbreeze.standalone" version="$VERSION" onConclusion="none">Standalone.pkg</pkg-ref>
</installer-gui-script>
XML

cat > "$BUILD_DIR/welcome.txt" <<TXT
This installs jj-breeze $VERSION:

 - Audio Unit  -> /Library/Audio/Plug-Ins/Components
 - VST3        -> /Library/Audio/Plug-Ins/VST3
 - Standalone app -> /Applications

Restart your DAW (or rescan plugins) afterwards.
TXT

PKG_NAME="jj-breeze-${VERSION}-macos"
FINAL_PKG="$DIST_DIR/$PKG_NAME.pkg"

echo "==> Building final installer package"
PRODUCTBUILD_ARGS=(
    --distribution "$DISTRIBUTION_XML"
    --package-path "$PKG_COMPONENTS_DIR"
    --resources "$BUILD_DIR"
)
if [[ -n "${PKG_SIGN_IDENTITY:-}" ]]; then
    echo "    signing installer with: $PKG_SIGN_IDENTITY"
    PRODUCTBUILD_ARGS+=(--sign "$PKG_SIGN_IDENTITY")
else
    echo "    PKG_SIGN_IDENTITY is empty, building an unsigned .pkg"
fi
rm -f "$FINAL_PKG"
productbuild "${PRODUCTBUILD_ARGS[@]}" "$FINAL_PKG"

if [[ "$DO_NOTARIZE" == "1" ]]; then
    echo "==> Submitting for notarization (keychain profile: $NOTARY_PROFILE)"
    xcrun notarytool submit "$FINAL_PKG" --keychain-profile "$NOTARY_PROFILE" --wait
    echo "==> Stapling notarization ticket"
    xcrun stapler staple "$FINAL_PKG"
fi

echo "==> Done: $FINAL_PKG"
