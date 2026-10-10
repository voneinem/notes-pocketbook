#!/usr/bin/env bash
# Fails if a test fixture looks like real data (constitution Principle IV: anonymized fixtures only).
#
#   scripts/check-fixtures.sh [fixtures-dir]     check (default: tests/fixtures)
#   scripts/check-fixtures.sh --self-test        prove that the checker rejects bad fixtures
#
# Rules, applied to every file below the directory:
#   - every host in an http(s) URL must be example.com, example.org, localhost, 127.0.0.1,
#     or a subdomain of example.com / example.org / *.invalid;
#   - every e-mail address must be @example.com (or a subdomain);
#   - no "Authorization" header or value may appear.
# See tests/fixtures/README.md for the anonymization procedure.
set -u

check_dir() {
    local dir="$1" bad=0 f
    [ -d "$dir" ] || { echo "no such directory: $dir" >&2; return 2; }
    while IFS= read -r -d '' f; do
        case "$f" in */README.md|*/.gitkeep) continue ;; esac
        while IFS= read -r host; do
            [ -z "$host" ] && continue
            case "$host" in
                example.com|*.example.com|example.org|*.example.org|localhost|127.0.0.1|*.invalid) ;;
                *) echo "$f: non-anonymized host '$host'" >&2; bad=1 ;;
            esac
        done < <(grep -oiE 'https?://[A-Za-z0-9._-]+' "$f" | sed -E 's#^https?://##' | tr 'A-Z' 'a-z' | sort -u)
        while IFS= read -r mail; do
            [ -z "$mail" ] && continue
            case "$mail" in
                *@example.com|*@*.example.com) ;;
                *) echo "$f: non-anonymized e-mail address '$mail'" >&2; bad=1 ;;
            esac
        done < <(grep -oE '[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}' "$f" | tr 'A-Z' 'a-z' | sort -u)
        if grep -qi 'authorization' "$f"; then
            echo "$f: contains an Authorization value" >&2
            bad=1
        fi
    done < <(find "$dir" -type f -print0)
    return $bad
}

if [ "${1:-}" = "--self-test" ]; then
    tmp="$(mktemp -d)"
    trap 'rm -rf "$tmp"' EXIT
    fail=0
    expect() {  # expect <pass|fail> <description>
        local want="$1" desc="$2"
        check_dir "$tmp/f" >/dev/null 2>&1
        local rc=$?
        if { [ "$want" = pass ] && [ $rc -eq 0 ]; } || { [ "$want" = fail ] && [ $rc -ne 0 ]; }; then
            echo "ok   $desc"
        else
            echo "FAIL $desc (rc=$rc)"; fail=1
        fi
    }
    mkdir -p "$tmp/f"
    printf '{"url":"https://cloud.example.com/index.php","mail":"alice@example.com"}' > "$tmp/f/good.json"
    expect pass "anonymized fixture is accepted"
    printf '{"url":"https://nextcloud.my-real-host.net/x"}' > "$tmp/f/bad.json"
    expect fail "real host is rejected"; rm "$tmp/f/bad.json"
    printf '{"owner":"someone@gmail.com"}' > "$tmp/f/bad.json"
    expect fail "real e-mail is rejected"; rm "$tmp/f/bad.json"
    printf 'Authorization: Basic YWxpY2U6cHc=' > "$tmp/f/bad.json"
    expect fail "Authorization value is rejected"; rm "$tmp/f/bad.json"
    exit $fail
fi

check_dir "${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/tests/fixtures}"
