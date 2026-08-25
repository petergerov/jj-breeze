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

echo "==> Building jj-breeze ($CONFIG)"
cmake -B "$BUILD_DIR" -G Xcode -DCMAKE_BUILD_TYPE="$CONFIG"
cmake --build "$BUILD_DIR" --config "$CONFIG"

ARTEFACTS_DIR="$BUILD_DIR/jj_breeze_artefacts/$CONFIG"
AU_PATH="$ARTEFACTS_DIR/AU/jj-breeze.component"
VST3_PATH="$ARTEFACTS_DIR/VST3/jj-breeze.vst3"
STANDALONE_PATH="$ARTEFACTS_DIR/Standalone/jj-breeze.app"

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
rm -rf "$AU_DEST_DIR/jj-breeze.component"
cp -R "$AU_PATH" "$AU_DEST_DIR/"

echo "==> Installing VST3 to $VST3_DEST_DIR"
mkdir -p "$VST3_DEST_DIR"
rm -rf "$VST3_DEST_DIR/jj-breeze.vst3"
cp -R "$VST3_PATH" "$VST3_DEST_DIR/"

if [[ "$INSTALL_STANDALONE" == "1" ]]; then
    if [[ -e "$STANDALONE_PATH" ]]; then
        APPS_DEST_DIR="$HOME/Applications"
        echo "==> Installing Standalone app to $APPS_DEST_DIR"
        mkdir -p "$APPS_DEST_DIR"
        rm -rf "$APPS_DEST_DIR/jj-breeze.app"
        cp -R "$STANDALONE_PATH" "$APPS_DEST_DIR/"
    else
        echo "warning: Standalone artefact not found at $STANDALONE_PATH, skipping" >&2
    fi
fi

# Clear the AudioComponent registration cache so a rescan picks up the
# rebuilt AU right away, and re-register it with auval if available.
touch "$HOME/Library/Preferences/com.apple.audio.InfoHelper.plist" 2>/dev/null || true

echo "==> Done. Restart your DAW (or rescan plugins) to pick up jj-breeze."
if command -v auval >/dev/null 2>&1; then
    echo "==> Validating AU with auval (JJBreezeAU / Grov)"
    auval -v aufx Jjbz Grov || echo "warning: auval reported an issue — check output above" >&2
fi
