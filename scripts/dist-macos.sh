#!/usr/bin/env bash
#
# dist-macos.sh — build jj-breeze (AU, VST3, Standalone) in Release and
# package the artefacts into a zip under dist/.
#
# Usage:
#   scripts/dist-macos.sh [--clean]
#
#   --clean   Remove the build/ directory before building (forces a full
#             rebuild instead of reusing an existing CMake cache).
#
# Optional env vars:
#   CODESIGN_IDENTITY   Sign each artefact with this identity before
#                        packaging (e.g. "Developer ID Application: Name (TEAMID)",
#                        or just the TEAMID). Defaults to "73DGAYU6A5"; pass
#                        CODESIGN_IDENTITY= (empty) to skip signing.

set -euo pipefail

: "${CODESIGN_IDENTITY:=73DGAYU6A5}"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="build"
DIST_DIR="dist"

if [[ "${1:-}" == "--clean" ]]; then
    echo "==> Removing existing $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

VERSION="$(grep -Eo 'project\(jj-breeze VERSION [0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt | grep -Eo '[0-9]+\.[0-9]+\.[0-9]+')"
if [[ -z "$VERSION" ]]; then
    echo "error: could not determine version from CMakeLists.txt" >&2
    exit 1
fi

echo "==> Building jj-breeze $VERSION (Release)"
cmake -B "$BUILD_DIR" -G Xcode -DCMAKE_BUILD_TYPE=Release
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

if [[ -n "${CODESIGN_IDENTITY:-}" ]]; then
    echo "==> Signing artefacts with identity: $CODESIGN_IDENTITY"
    for path in "$AU_PATH" "$VST3_PATH" "$STANDALONE_PATH"; do
        codesign --force --deep --options runtime --timestamp \
            --sign "$CODESIGN_IDENTITY" "$path"
    done
fi

PKG_NAME="jj-breeze-${VERSION}-macos"
PKG_DIR="$DIST_DIR/$PKG_NAME"

echo "==> Assembling $PKG_DIR"
rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR"
cp -R "$AU_PATH" "$PKG_DIR/"
cp -R "$VST3_PATH" "$PKG_DIR/"
cp -R "$STANDALONE_PATH" "$PKG_DIR/"

ZIP_PATH="$DIST_DIR/$PKG_NAME.zip"
echo "==> Zipping $ZIP_PATH"
rm -f "$ZIP_PATH"
(cd "$DIST_DIR" && ditto -c -k --sequesterRsrc --keepParent "$PKG_NAME" "$PKG_NAME.zip")

echo "==> Done: $ZIP_PATH"
