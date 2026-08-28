#!/usr/bin/env bash
#
# notarize-macos-pkg.sh — take a built .pkg and make it install on other Macs
# without a Gatekeeper warning: sign it if needed, send it to Apple's notary
# service, staple the ticket, and verify the result.
#
# Why this is a separate step from signing: since macOS 10.15, a Developer ID
# signature alone is NOT enough. An installer that is signed but not notarized
# still gets "Apple could not verify ... is free of malware". Only a stapled
# notarization ticket removes the warning entirely, including offline, which
# is what stapling is for — without it Gatekeeper has to reach Apple, and a
# machine with no network shows the warning again.
#
# Usage:
#   scripts/notarize-macos-pkg.sh [path/to.pkg] [--skip-preflight]
#
#   path/to.pkg      Defaults to the newest .pkg in dist/.
#   --skip-preflight Skip the local checks and submit straight to Apple.
#
# Credentials — either set all three of:
#   APPLE_ID             Apple ID that owns the Developer ID certificates
#   APPLE_TEAM_ID        e.g. C9LBGZNZ6P
#   APPLE_APP_PASSWORD   app-specific password from appleid.apple.com
#                        (NOT the normal Apple ID password)
# or, to reuse a stored profile instead:
#   NOTARY_PROFILE       name from `xcrun notarytool store-credentials`
#
# Optional:
#   PKG_SIGN_IDENTITY    "Developer ID Installer" identity, used only if the
#                        .pkg is not already signed. Defaults to this repo's.
#
# Typical use, after scripts/dist-macos-pkg.sh has built and signed a package:
#   APPLE_ID=you@example.com APPLE_TEAM_ID=C9LBGZNZ6P APPLE_APP_PASSWORD=xxxx-xxxx-xxxx-xxxx \
#       scripts/notarize-macos-pkg.sh

set -euo pipefail

# ${VAR=default}, not ${VAR:=default}: the ":" form would also override an
# explicitly-empty value, which is how a caller says "don't sign".
: "${PKG_SIGN_IDENTITY=Developer ID Installer: Petar Gerov (C9LBGZNZ6P)}"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

PKG_PATH=""
SKIP_PREFLIGHT=0
for arg in "$@"; do
    case "$arg" in
        --skip-preflight) SKIP_PREFLIGHT=1 ;;
        -*)
            echo "error: unknown argument: $arg" >&2
            exit 1
            ;;
        *)
            if [[ -n "$PKG_PATH" ]]; then
                echo "error: more than one .pkg given" >&2
                exit 1
            fi
            PKG_PATH="$arg"
            ;;
    esac
done

if [[ -z "$PKG_PATH" ]]; then
    # Newest first, so this picks up the package a build just produced.
    PKG_PATH="$(ls -t dist/*.pkg 2>/dev/null | head -1 || true)"
    if [[ -z "$PKG_PATH" ]]; then
        echo "error: no .pkg given and none found in dist/ — run scripts/dist-macos-pkg.sh first" >&2
        exit 1
    fi
    echo "==> Using newest package in dist/: $PKG_PATH"
fi

if [[ ! -f "$PKG_PATH" ]]; then
    echo "error: not a file: $PKG_PATH" >&2
    exit 1
fi

# --- Credentials ------------------------------------------------------------
# Resolved before any slow work, so a missing password fails in a second
# rather than after a full preflight.
NOTARY_ARGS=()
if [[ -n "${APPLE_ID:-}" && -n "${APPLE_TEAM_ID:-}" && -n "${APPLE_APP_PASSWORD:-}" ]]; then
    NOTARY_ARGS=(--apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" --password "$APPLE_APP_PASSWORD")
elif [[ -n "${NOTARY_PROFILE:-}" ]]; then
    NOTARY_ARGS=(--keychain-profile "$NOTARY_PROFILE")
else
    cat >&2 <<'MSG'
error: no notarization credentials.

Set all three of:
    APPLE_ID, APPLE_TEAM_ID, APPLE_APP_PASSWORD

The password must be an app-specific password generated at appleid.apple.com
(Sign-In and Security -> App-Specific Passwords), not your Apple ID password.

Alternatively store them once:
    xcrun notarytool store-credentials "notary-profile" \
        --apple-id you@example.com --team-id C9LBGZNZ6P --password <app-specific-password>
and then re-run with NOTARY_PROFILE=notary-profile.
MSG
    exit 1
fi

# --- Sign the package itself, if it isn't already ---------------------------
# Note this can only sign the .pkg wrapper. The binaries *inside* have to have
# been signed before packaging (dist-macos-pkg.sh does that) — the preflight
# below is what catches it if they weren't.
if pkgutil --check-signature "$PKG_PATH" 2>/dev/null | grep -q "Status: signed"; then
    echo "==> Package is already signed"
else
    if [[ -z "$PKG_SIGN_IDENTITY" ]]; then
        echo "error: $PKG_PATH is unsigned and PKG_SIGN_IDENTITY is empty — notarization requires a signed package" >&2
        exit 1
    fi
    echo "==> Package is unsigned, signing with: $PKG_SIGN_IDENTITY"
    SIGNED_TMP="${PKG_PATH%.pkg}.signed.pkg"
    productsign --sign "$PKG_SIGN_IDENTITY" "$PKG_PATH" "$SIGNED_TMP"
    mv -f "$SIGNED_TMP" "$PKG_PATH"
fi

# --- Preflight --------------------------------------------------------------
# Apple rejects a submission if any nested binary lacks a Developer ID
# signature, the hardened runtime, or a secure timestamp — and reports it only
# through a separate log fetch, minutes later. Checking locally first turns a
# slow, opaque round trip into an immediate, specific error.
if [[ "$SKIP_PREFLIGHT" == "0" ]]; then
    echo "==> Preflight: checking payload signatures"
    EXPAND_DIR="$(mktemp -d)"
    trap 'rm -rf "$EXPAND_DIR"' EXIT
    rm -rf "$EXPAND_DIR/pkg"
    pkgutil --expand-full "$PKG_PATH" "$EXPAND_DIR/pkg" >/dev/null

    problems=0
    while IFS= read -r bundle; do
        name="$(basename "$bundle")"

        if ! codesign --verify --strict "$bundle" 2>/dev/null; then
            echo "    $name: NOT validly signed" >&2
            problems=$((problems + 1))
            continue
        fi

        details="$(codesign -dvv "$bundle" 2>&1)"

        if ! grep -q "Authority=Developer ID Application" <<<"$details"; then
            echo "    $name: not signed with a 'Developer ID Application' certificate" >&2
            problems=$((problems + 1))
            continue
        fi
        # Hardened runtime shows up as the "runtime" flag on the code directory.
        if ! grep -qE "flags=.*runtime" <<<"$details"; then
            echo "    $name: hardened runtime not enabled (codesign --options runtime)" >&2
            problems=$((problems + 1))
            continue
        fi
        if ! grep -q "Timestamp=" <<<"$details"; then
            echo "    $name: no secure timestamp (codesign --timestamp)" >&2
            problems=$((problems + 1))
            continue
        fi

        echo "    $name: OK"
    done < <(find "$EXPAND_DIR/pkg" \( -name "*.component" -o -name "*.vst3" -o -name "*.app" \) -prune -print | sort)

    if [[ "$problems" -gt 0 ]]; then
        cat >&2 <<MSG

error: $problems payload item(s) would be rejected by Apple.

The binaries must be signed *before* being packaged. Rebuild with:
    scripts/dist-macos-pkg.sh
which signs each artefact with --options runtime --timestamp, then re-run this.
MSG
        exit 1
    fi
fi

# --- Notarize ---------------------------------------------------------------
echo "==> Submitting to Apple's notary service (this usually takes a few minutes)"
SUBMIT_OUTPUT="$(xcrun notarytool submit "$PKG_PATH" "${NOTARY_ARGS[@]}" --wait 2>&1)" || true
echo "$SUBMIT_OUTPUT"

SUBMISSION_ID="$(grep -Eo '[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}' <<<"$SUBMIT_OUTPUT" | head -1 || true)"

if ! grep -q "status: Accepted" <<<"$SUBMIT_OUTPUT"; then
    echo >&2
    echo "error: notarization did not succeed." >&2
    # The submit output only ever says "Invalid" — the actual reason lives in a
    # separate log that has to be fetched by submission id.
    if [[ -n "$SUBMISSION_ID" ]]; then
        echo "==> Fetching the rejection log for submission $SUBMISSION_ID" >&2
        xcrun notarytool log "$SUBMISSION_ID" "${NOTARY_ARGS[@]}" >&2 || true
    fi
    exit 1
fi

# --- Staple -----------------------------------------------------------------
# Attaches the ticket to the .pkg itself, so Gatekeeper can verify it without
# contacting Apple. Without this, an offline Mac still warns.
echo "==> Stapling the notarization ticket"
xcrun stapler staple "$PKG_PATH"

# --- Verify -----------------------------------------------------------------
echo "==> Verifying"
xcrun stapler validate "$PKG_PATH"

# This is the definitive check: it is the same assessment Gatekeeper performs
# when the user double-clicks the installer.
if spctl --assess -vv --type install "$PKG_PATH" 2>&1 | tee /dev/stderr | grep -q "source=Notarized Developer ID"; then
    echo
    echo "==> Done: $PKG_PATH is signed, notarized and stapled — it will install without a Gatekeeper warning."
else
    echo >&2
    echo "error: Gatekeeper assessment did not report a notarized Developer ID source." >&2
    exit 1
fi
