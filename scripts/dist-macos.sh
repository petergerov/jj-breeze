#!/usr/bin/env bash
#
# dist-macos.sh — build jj-breeze (AU, VST3, Standalone) in Release and
# package the artefacts into a zip under dist/.
#
# The AAX (Pro Tools) plugin is packaged too whenever the AAX SDK is available
# — see scripts/aax-sdk.sh. Without it the zip holds the same three artefacts
# it always did.
#
# Usage:
#   scripts/dist-macos.sh [--clean] [--no-aax]
#
#   --clean   Remove the build/ directory before building (forces a full
#             rebuild instead of reusing an existing CMake cache).
#   --no-aax  Skip the AAX build even if the SDK is there.
#
# Optional env vars — normally set once in scripts/.env (gitignored; see
# RELEASE.md). Anything already in the environment wins over that file.
#   CODESIGN_IDENTITY   Sign each artefact with this identity before
#                        packaging. Use a "Developer ID Application" identity:
#                        that is the one Gatekeeper accepts on someone else's
#                        Mac (an "Apple Development" identity, which is what a
#                        bare team ID tends to resolve to, is only good for
#                        local/Xcode testing). Empty (or unset) skips signing.

set -euo pipefail

# shellcheck source=scripts/load-env.sh
source "$(dirname "${BASH_SOURCE[0]}")/load-env.sh"

# Define-if-unset, without the colon: ${VAR:=} would also overwrite an
# intentionally-empty value and silently re-enable signing.
: "${CODESIGN_IDENTITY=}"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="build"
DIST_DIR="dist"

WITH_AAX=1
for arg in "$@"; do
    case "$arg" in
        --clean)
            echo "==> Removing existing $BUILD_DIR"
            rm -rf "$BUILD_DIR"
            ;;
        --no-aax) WITH_AAX=0 ;;
        *)
            echo "error: unknown argument: $arg" >&2
            exit 1
            ;;
    esac
done

VERSION="$(grep -Eo 'project\(jj-breeze VERSION [0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt | grep -Eo '[0-9]+\.[0-9]+\.[0-9]+')"
if [[ -z "$VERSION" ]]; then
    echo "error: could not determine version from CMakeLists.txt" >&2
    exit 1
fi

# The artefact filenames follow PRODUCT_NAME in CMakeLists.txt, not the repo
# or CMake target name — read it rather than hardcoding "jj-breeze", so a
# rename there can't silently leave this script looking for (and packaging)
# a stale build under the old name.
PRODUCT_NAME="$(grep -Eo 'PRODUCT_NAME "[^"]*"' CMakeLists.txt | sed -E 's/PRODUCT_NAME "(.*)"/\1/')"
if [[ -z "$PRODUCT_NAME" ]]; then
    echo "error: could not determine PRODUCT_NAME from CMakeLists.txt" >&2
    exit 1
fi

# An absent SDK is the normal case, not a failure: aax-sdk.sh exits 1 and the
# build simply carries on without the AAX format. Only an SDK that is present
# but broken stops us, and that comes out of the script as an explicit error.
AAX_SDK=""
if [[ "$WITH_AAX" == "1" ]]; then
    AAX_SDK="$("$ROOT_DIR/scripts/aax-sdk.sh")" || AAX_SDK=""
fi

if [[ -n "$AAX_SDK" ]]; then
    echo "==> Building $PRODUCT_NAME $VERSION (Release, with AAX)"
else
    echo "==> Building $PRODUCT_NAME $VERSION (Release)"
fi
cmake -B "$BUILD_DIR" -G Xcode -DCMAKE_BUILD_TYPE=Release \
    -DJJ_BREEZE_AAX_SDK_PATH="$AAX_SDK"
cmake --build "$BUILD_DIR" --config Release

ARTEFACTS_DIR="$BUILD_DIR/jj_breeze_artefacts/Release"
AU_PATH="$ARTEFACTS_DIR/AU/$PRODUCT_NAME.component"
VST3_PATH="$ARTEFACTS_DIR/VST3/$PRODUCT_NAME.vst3"
STANDALONE_PATH="$ARTEFACTS_DIR/Standalone/$PRODUCT_NAME.app"
AAX_PATH="$ARTEFACTS_DIR/AAX/$PRODUCT_NAME.aaxplugin"

# One list from here on, so signing and packaging can't drift apart over which
# artefacts exist in this build.
ARTEFACTS=("$AU_PATH" "$VST3_PATH" "$STANDALONE_PATH")
if [[ -n "$AAX_SDK" ]]; then
    ARTEFACTS+=("$AAX_PATH")
fi

for path in "${ARTEFACTS[@]}"; do
    if [[ ! -e "$path" ]]; then
        echo "error: expected build artefact not found: $path" >&2
        exit 1
    fi
done

if [[ -n "${CODESIGN_IDENTITY:-}" ]]; then
    echo "==> Signing artefacts with identity: $CODESIGN_IDENTITY"
    for path in "${ARTEFACTS[@]}"; do
        codesign --force --deep --options runtime --timestamp \
            --sign "$CODESIGN_IDENTITY" "$path"
    done
fi

PKG_NAME="jj-breeze-${VERSION}-macos"
PKG_DIR="$DIST_DIR/$PKG_NAME"

echo "==> Assembling $PKG_DIR"
rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR"
for path in "${ARTEFACTS[@]}"; do
    cp -R "$path" "$PKG_DIR/"
done

ZIP_PATH="$DIST_DIR/$PKG_NAME.zip"
echo "==> Zipping $ZIP_PATH"
rm -f "$ZIP_PATH"
(cd "$DIST_DIR" && ditto -c -k --sequesterRsrc --keepParent "$PKG_NAME" "$PKG_NAME.zip")

echo "==> Done: $ZIP_PATH"
if [[ -n "$AAX_SDK" ]]; then
    echo "    note: the .aaxplugin in there is not PACE-signed, so Pro Tools"
    echo "          will refuse to load it — see the AAX section of RELEASE.md"
fi
