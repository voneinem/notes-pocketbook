#!/usr/bin/env bash
# Local merge gate (CI is deferred): everything that must be green before a change is merged
# (constitution, Development Workflow: tests pass and no new secrets).
#
#   scripts/pre-merge.sh              build the host target, run all tests, scan for secrets
#   scripts/pre-merge.sh --secrets    only the secret scan
#   scripts/pre-merge.sh --self-test  prove that the built-in secret scan detects what it should
#
# It also runs scripts/check-version-bump.sh: a branch that changes the product must raise VERSION and
# add a CHANGELOG.md entry.
# The tests include scripts/check-constitution.sh (TLS bypasses, SDK paths, Qt in the core,
# logging of personal data, absolute local paths) and scripts/check-fixtures.sh (anonymized fixtures).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# ---- secret scan ---------------------------------------------------------------------------------
# Uses gitleaks when installed, otherwise built-in patterns. tests/, third_party/ and the checker
# scripts (scripts/check-*.sh, whose self-tests embed fake bad samples) are excluded from the
# built-in scan: they contain obviously fake credentials by design.
secret_scan() {
    local root="$1"
    if command -v gitleaks >/dev/null 2>&1 && [ -d "$root/.git" ]; then
        echo "secret scan: gitleaks"
        gitleaks detect --no-git --source "$root" --no-banner --redact
        return $?
    fi
    echo "secret scan: built-in patterns (gitleaks not installed)"
    local bad=0 f p
    while IFS= read -r -d '' f; do
        case "$f" in
            third_party/*|tests/*|.git/*|build*|.claude/*|*.bmp|*.zip) continue ;;
        esac
        p="$root/$f"
        [ -f "$p" ] && grep -Iq . "$p" 2>/dev/null || continue
        # this script's patterns and the checkers' self-tests contain deliberately fake samples
        case "$f" in scripts/pre-merge.sh|scripts/check-*.sh) continue ;; esac
        if grep -nE -e '-----BEGIN ([A-Z]+ )?PRIVATE KEY-----' \
                    -e 'Authorization:[[:space:]]*Basic[[:space:]]+[A-Za-z0-9+/=]{8,}' \
                    -e '(password|passwd|secret|api[_-]?key|token)[[:space:]]*[:=][[:space:]]*"[^"$<{][^"]{5,}"' \
                    "$p" | grep -vE '(example|placeholder|REDACTED|<|\$\{)' | sed "s#^#SECRET $f:#" | grep .; then
            bad=1
        fi
    done < <(if [ -d "$root/.git" ]; then git -C "$root" ls-files -z --cached --others --exclude-standard; else (cd "$root" && find . -type f -not -path './.git/*' -print0 | sed -z 's#^\./##'); fi)
    return $bad
}

if [ "${1:-}" = "--self-test" ]; then
    tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
    fail=0
    check() {  # check <pass|fail> <description>
        local rc=0
        secret_scan "$tmp" >/dev/null 2>&1 || rc=$?
        if { [ "$1" = pass ] && [ $rc -eq 0 ]; } || { [ "$1" = fail ] && [ $rc -ne 0 ]; }; then echo "ok   $2"; else echo "FAIL $2 (rc=$rc)"; fail=1; fi
    }
    # gitleaks would take over; the self-test always exercises the built-in patterns
    printf 'int main() { return 0; }\n' > "$tmp/a.cpp"
    PATH_BACKUP="$PATH"; PATH="/usr/bin:/bin"; command -v gitleaks >/dev/null 2>&1 && { echo "note: gitleaks present, built-in self-test skipped"; exit 0; }
    check pass "clean tree"
    printf -- '-----BEGIN RSA PRIVATE KEY-----\nMIIE\n' > "$tmp/k.pem.txt"; check fail "private key"; rm "$tmp/k.pem.txt"
    printf 'Authorization: Basic YWxpY2U6cHdzZWNyZXQ=\n' > "$tmp/h.txt"; check fail "basic auth header"; rm "$tmp/h.txt"
    printf 'const char* password = "hunter2hunter2";\n' > "$tmp/p.cpp"; check fail "hard-coded password"; rm "$tmp/p.cpp"
    printf 'password = "<your app password>"\n' > "$tmp/ok.txt"; check pass "placeholder password is fine"; rm "$tmp/ok.txt"
    exit $fail
fi

if [ "${1:-}" = "--secrets" ]; then
    secret_scan "$ROOT"
    exit $?
fi

echo "== version bump"
"$ROOT/scripts/check-version-bump.sh" "$ROOT"

echo "== host build and tests"
cmake -S "$ROOT" -B "$ROOT/build-premerge" -G Ninja -DCMAKE_BUILD_TYPE=Debug >/dev/null
cmake --build "$ROOT/build-premerge"
ctest --test-dir "$ROOT/build-premerge" --output-on-failure

echo "== secret scan"
secret_scan "$ROOT"
echo "pre-merge checks passed"
