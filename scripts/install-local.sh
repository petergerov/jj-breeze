#!/usr/bin/env bash
#
# install-local.sh — build jj-breeze and install it into the standard
# per-user plugin/app locations on this Mac, for local testing in a DAW.
#
# Usage:
#   scripts/install-local.sh [--debug] [--clean] [--no-standalone]
#
#   --debug           Build the Debug configuration instead of Release.
#   --clean           Remove the build/ directory before building.
#   --no-standalone   Skip installing the Standalone app into ~/Applications.
#
# What it does:
#   - Builds AU, VST3 and Standalone via CMake/Xcode.
#   - Copies the AU to ~/Library/Audio/Plug-Ins/Components
#   - Copies the VST3 to ~/Library/Audio/Plug-Ins/VST3
#   - Copies the Standalone .app to ~/Applications
#   (CMake's COPY_PLUGIN_AFTER_BUILD already does the AU/VST3 copy as part
#   of the build, but this script does it explicitly too so it works even
#   if that step was skipped, and to also place the Standalone app.)

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="build"
CONFIG="Release"
INSTALL_STANDALONE=1

for arg in "$@"; do
    case "$arg" in
        --debug) CONFIG="Debug" ;;
        --clean) rm -rf "$BUILD_DIR" ;;
        --no-standalone) INSTALL_STANDALONE=0 ;;
        *)
            echo "error: unknown argument: $arg" >&2
            exit 1
            ;;
    esac
done

# The artefact filenames follow PRODUCT_NAME in CMakeLists.txt, not the repo
# or CMake target name — read it rather than hardcoding "jj-breeze", so a
# rename there (like "jj-breeze" -> "j.j.breeze") can't silently leave this
# script installing a stale build under the old name while a new one sits
# unused in build/.
PRODUCT_NAME="$(grep -Eo 'PRODUCT_NAME "[^"]*"' CMakeLists.txt | sed -E 's/PRODUCT_NAME "(.*)"/\1/')"
if [[ -z "$PRODUCT_NAME" ]]; then
    echo "error: could not determine PRODUCT_NAME from CMakeLists.txt" >&2
    exit 1
fi

echo "==> Building $PRODUCT_NAME ($CONFIG)"
cmake -B "$BUILD_DIR" -G Xcode -DCMAKE_BUILD_TYPE="$CONFIG"
cmake --build "$BUILD_DIR" --config "$CONFIG"

ARTEFACTS_DIR="$BUILD_DIR/jj_breeze_artefacts/$CONFIG"
AU_PATH="$ARTEFACTS_DIR/AU/$PRODUCT_NAME.component"
VST3_PATH="$ARTEFACTS_DIR/VST3/$PRODUCT_NAME.vst3"
STANDALONE_PATH="$ARTEFACTS_DIR/Standalone/$PRODUCT_NAME.app"

for path in "$AU_PATH" "$VST3_PATH"; do
    if [[ ! -e "$path" ]]; then
        echo "error: expected build artefact not found: $path" >&2
        exit 1
    fi
done

AU_DEST_DIR="$HOME/Library/Audio/Plug-Ins/Components"
VST3_DEST_DIR="$HOME/Library/Audio/Plug-Ins/VST3"

echo "==> Installing AU to $AU_DEST_DIR"
mkdir -p "$AU_DEST_DIR"
rm -rf "$AU_DEST_DIR/$PRODUCT_NAME.component"
cp -R "$AU_PATH" "$AU_DEST_DIR/"

echo "==> Installing VST3 to $VST3_DEST_DIR"
mkdir -p "$VST3_DEST_DIR"
rm -rf "$VST3_DEST_DIR/$PRODUCT_NAME.vst3"
cp -R "$VST3_PATH" "$VST3_DEST_DIR/"

if [[ "$INSTALL_STANDALONE" == "1" ]]; then
    if [[ -e "$STANDALONE_PATH" ]]; then
        APPS_DEST_DIR="$HOME/Applications"
        echo "==> Installing Standalone app to $APPS_DEST_DIR"
        mkdir -p "$APPS_DEST_DIR"
        rm -rf "$APPS_DEST_DIR/$PRODUCT_NAME.app"
        cp -R "$STANDALONE_PATH" "$APPS_DEST_DIR/"
    else
        echo "warning: Standalone artefact not found at $STANDALONE_PATH, skipping" >&2
    fi
fi

# Clear the AudioComponent registration cache so a rescan (and auval below)
# picks up the rebuilt AU right away. Touching com.apple.audio.InfoHelper.plist
# alone doesn't actually do this — macOS keeps the real component registry
# (name, version, etc.) in this cache file instead, keyed by the component's
# type/subtype/manufacturer tuple, so a rebuild under a changed PRODUCT_NAME
# can otherwise still report the *old* name here (and in every host) until
# this is cleared.
rm -f "$HOME/Library/Caches/AudioUnitCache/com.apple.audiounits.cache" 2>/dev/null || true
touch "$HOME/Library/Preferences/com.apple.audio.InfoHelper.plist" 2>/dev/null || true

echo "==> Done. Restart your DAW (or rescan plugins) to pick up $PRODUCT_NAME."
if command -v auval >/dev/null 2>&1; then
    echo "==> Validating AU with auval (JJBreezeAU / Grov)"
    auval -v aufx Jjbz Grov || echo "warning: auval reported an issue — check output above" >&2
fi
