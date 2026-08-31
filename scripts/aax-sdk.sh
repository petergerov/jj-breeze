#!/usr/bin/env bash
#
# aax-sdk.sh — find the AAX SDK for this checkout, unpacking it on first use,
# and print its path on stdout.
#
# AAX is Avid's Pro Tools plugin format. Its SDK may not be redistributed, so
# nothing of it is vendored here: you download aax-sdk-<version>.zip from your
# Avid developer account and drop it into AAX/. Everything AAX in this repo is
# conditional on that — a checkout without the SDK builds AU, VST3 and
# Standalone exactly as it did before, with no error and no extra step.
#
# Usage:
#   scripts/aax-sdk.sh              # print the SDK path, or exit 1 if there is none
#   scripts/aax-sdk.sh --quiet      # same, without the "unpacking..." progress note
#
# Env:
#   AAX_SDK_PATH   Use an SDK sitting somewhere else entirely and skip the
#                  search. Being set but wrong is an error, not a silent skip:
#                  it was asked for explicitly.
#
# Exit codes:
#   0   the path is on stdout
#   1   no SDK available, or AAX_SDK_PATH points at something that isn't one
#
# Everything but the path itself goes to stderr, so callers can do:
#
#   if AAX_SDK="$(scripts/aax-sdk.sh)"; then ... else AAX_SDK=""; fi

set -euo pipefail

QUIET=0
for arg in "$@"; do
    case "$arg" in
        --quiet) QUIET=1 ;;
        *)
            echo "error: unknown argument: $arg" >&2
            exit 1
            ;;
    esac
done

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AAX_DIR="$ROOT_DIR/AAX"

note() {
    [[ "$QUIET" == "1" ]] || echo "$@" >&2
}

# What JUCE's juce_set_aax_sdk_path() itself checks for. Testing the same
# thing here means a half-unpacked or wrong-layout directory is rejected by
# this script, with a sentence about it, rather than by CMake with a
# FATAL_ERROR in the middle of a release build.
is_sdk() {
    [[ -d "$1/Interfaces/ACF" ]]
}

if [[ -n "${AAX_SDK_PATH:-}" ]]; then
    if ! is_sdk "$AAX_SDK_PATH"; then
        echo "error: AAX_SDK_PATH is set to '$AAX_SDK_PATH', which has no Interfaces/ACF —" >&2
        echo "       that is not an unpacked AAX SDK" >&2
        exit 1
    fi
    (cd "$AAX_SDK_PATH" && pwd)
    exit 0
fi

# Already unpacked? Newest version wins, so dropping a 2.10 zip beside a 2.9
# one and re-running picks up the new SDK instead of the old directory.
# `sort -V` sorts aax-sdk-2-10-0 after aax-sdk-2-9-0; a plain glob would not.
newest() {
    printf '%s\n' "$@" | sort -V | tail -n 1
}

candidates=()
for dir in "$AAX_DIR"/aax-sdk-*/; do
    dir="${dir%/}"
    is_sdk "$dir" && candidates+=("$dir")
done
if [[ ${#candidates[@]} -gt 0 ]]; then
    newest "${candidates[@]}"
    exit 0
fi

zips=()
for zip in "$AAX_DIR"/aax-sdk-*.zip; do
    [[ -f "$zip" ]] && zips+=("$zip")
done
if [[ ${#zips[@]} -eq 0 ]]; then
    note "note: no AAX SDK found (looked for AAX/aax-sdk-*.zip and AAX/aax-sdk-*/)"
    exit 1
fi

ZIP="$(newest "${zips[@]}")"
DEST="$AAX_DIR/$(basename "${ZIP%.zip}")"

if ! command -v unzip >/dev/null 2>&1; then
    echo "error: found $ZIP but no unzip on PATH to unpack it" >&2
    exit 1
fi

note "==> Unpacking $(basename "$ZIP") (first use; ~100 MB)"

# Unpack beside the destination and move into place only once it is complete,
# so an interrupted run can't leave a half-extracted directory that the check
# above would then happily accept on the next run.
TMP_DIR="$(mktemp -d "$AAX_DIR/.aax-sdk-unpack-XXXXXX")"
trap 'rm -rf "$TMP_DIR"' EXIT

unzip -q "$ZIP" -d "$TMP_DIR"

# Avid's zip wraps everything in a single aax-sdk-<version>/ directory, but
# don't bank on it: accept a zip that holds the SDK at its root too.
SRC=""
if is_sdk "$TMP_DIR"; then
    SRC="$TMP_DIR"
else
    for dir in "$TMP_DIR"/*/; do
        dir="${dir%/}"
        if is_sdk "$dir"; then
            SRC="$dir"
            break
        fi
    done
fi

if [[ -z "$SRC" ]]; then
    echo "error: $ZIP does not look like an AAX SDK (no Interfaces/ACF inside)" >&2
    exit 1
fi

rm -rf "$DEST"
mv "$SRC" "$DEST"

note "==> AAX SDK ready: $DEST"
echo "$DEST"
