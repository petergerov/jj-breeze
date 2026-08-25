# Sourced by the dist/notarize scripts — not meant to be run directly.
#
# Loads scripts/.env into the environment, but never over a variable that is
# already set. That ordering is deliberate:
#
#   - it keeps `APP_SIGN_IDENTITY= scripts/dist-macos-pkg.sh` working as the
#     documented way to force an unsigned build, and
#   - CI passes everything as real environment variables and ships no .env,
#     so nothing here can quietly override a workflow.
#
# Set JJ_BREEZE_ENV_FILE to point somewhere else.

_jj_load_env_file() {
    local file="$1"
    [[ -f "$file" ]] || return 0

    local line key value
    while IFS= read -r line || [[ -n "$line" ]]; do
        line="${line%$'\r'}"                          # tolerate CRLF
        [[ "$line" =~ ^[[:space:]]*(#|$) ]] && continue
        [[ "$line" == *=* ]] || continue

        key="${line%%=*}"
        key="${key#"${key%%[![:space:]]*}"}"          # trim leading space
        key="${key#export }"
        key="$(printf '%s' "$key" | tr -d '[:space:]')"
        # Ignore anything that isn't a plain shell identifier rather than
        # risking an `export` of arbitrary text.
        [[ "$key" =~ ^[A-Za-z_][A-Za-z0-9_]*$ ]] || continue

        value="${line#*=}"
        value="${value#"${value%%[![:space:]]*}"}"    # trim leading space

        # Strip one surrounding pair of quotes, so values containing spaces and
        # parentheses (every Developer ID name) survive.
        if [[ ${#value} -ge 2 ]]; then
            if [[ "${value:0:1}" == '"' && "${value: -1}" == '"' ]] \
                || [[ "${value:0:1}" == "'" && "${value: -1}" == "'" ]]; then
                value="${value:1:${#value}-2}"
            fi
        fi

        # ${!key+x} is "is it set at all", so an explicitly empty environment
        # variable still counts as set and is left alone.
        if [[ -z "${!key+x}" ]]; then
            export "$key=$value"
        fi
    done < "$file"
}

_jj_load_env_file "${JJ_BREEZE_ENV_FILE:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/.env}"
