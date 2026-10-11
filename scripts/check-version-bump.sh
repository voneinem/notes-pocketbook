#!/usr/bin/env bash
# Release-version gate: a branch that changes the product must raise VERSION (constitution, Development
# Workflow) and say what changed in CHANGELOG.md.
#
#   scripts/check-version-bump.sh [repo-root]    compare the working tree with the base branch
#   scripts/check-version-bump.sh --self-test    prove the rules on throw-away repositories
#
# Environment: PBNOTES_BASE_REF  branch to compare with (default: main, else origin/main)
#              PBNOTES_NO_BUMP=1 allow a product change without a bump (refactor, no visible change)
#
# A base without a VERSION file (the branch introduces it) needs no bump, only the CHANGELOG entry.
# Files that do not need a bump: specs/, docs/, .specify/, .claude/ and *.md except CHANGELOG.md.
set -euo pipefail

semver_gt() {  # semver_gt <new> <old>: new is strictly greater
    [ "$1" != "$2" ] && [ "$(printf '%s\n%s\n' "$1" "$2" | sort -V | tail -n 1)" = "$1" ]
}

check() {
    local root="$1" base="${PBNOTES_BASE_REF:-}" cur old new changed
    cd "$root"
    git rev-parse --git-dir >/dev/null 2>&1 || { echo "version check: not a git repository, skipped"; return 0; }
    if [ -z "$base" ]; then
        for base in main origin/main; do git rev-parse --verify -q "$base" >/dev/null && break || base=""; done
    fi
    [ -n "$base" ] || { echo "version check: no main branch to compare with, skipped"; return 0; }
    cur="$(git rev-parse --abbrev-ref HEAD)"
    [ "$cur" != "$base" ] || { echo "version check: on $base itself, skipped"; return 0; }
    base="$(git merge-base "$base" HEAD)"

    new="$(head -n 1 VERSION 2>/dev/null | tr -d '[:space:]')"
    [[ "$new" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "version check FAILED: VERSION must contain MAJOR.MINOR.PATCH (got '$new')" >&2; return 1; }
    old="$(git show "$base:VERSION" 2>/dev/null | head -n 1 | tr -d '[:space:]' || true)"

    changed="$( { git diff --name-only "$base"; git ls-files --others --exclude-standard; } | sort -u \
        | grep -Ev '^(specs/|docs/|\.specify/|\.claude/)' | grep -Ev '(^|/)[^/]*\.md$' || true)"
    changed="$(printf '%s\n' "$changed" | grep -Evx 'VERSION|CHANGELOG\.md|' || true)"

    if [ -n "$changed" ] && [ -n "$old" ] && ! semver_gt "$new" "$old" && [ "${PBNOTES_NO_BUMP:-}" != 1 ]; then
        echo "version check FAILED: the branch changes the product but VERSION is still $new (base: $old)." >&2
        echo "Raise VERSION (MINOR for a feature, PATCH for a fix, MAJOR for a breaking change) and add a" >&2
        echo "CHANGELOG.md entry, or set PBNOTES_NO_BUMP=1 for a change nobody can see." >&2
        echo "Changed: $(printf '%s' "$changed" | head -n 5 | tr '\n' ' ')" >&2
        return 1
    fi
    if [ "$new" != "$old" ]; then   # also when the base has no VERSION yet
        if ! grep -Eq "^## \[?${new//./\\.}\]?([[:space:]]|$)" CHANGELOG.md 2>/dev/null; then
            echo "version check FAILED: VERSION is $new but CHANGELOG.md has no '## $new' entry." >&2
            return 1
        fi
    fi
    echo "version check passed (VERSION $new, base ${old:-none})"
}

if [ "${1:-}" = "--self-test" ]; then
    tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
    fail=0
    expect() {  # expect <pass|fail> <description>
        local rc=0
        ( check "$tmp/r" ) >/dev/null 2>&1 || rc=$?
        if { [ "$1" = pass ] && [ $rc -eq 0 ]; } || { [ "$1" = fail ] && [ $rc -ne 0 ]; }; then echo "ok   $2"; else echo "FAIL $2 (rc=$rc)"; fail=1; fi
    }
    mkdir "$tmp/r"; cd "$tmp/r"
    git init -q -b main . && git config user.email t@t && git config user.name t
    printf '1.0.0\n' > VERSION; printf '## 1.0.0\n- first\n' > CHANGELOG.md; mkdir app specs; echo a > app/a.cpp
    git add . && git commit -qm base
    git checkout -qb feature/x
    expect pass "nothing changed"
    echo b > specs/s.md; echo c > README.md; expect pass "docs and specs only need no bump"
    echo b > app/a.cpp; expect fail "code changed, VERSION unchanged"
    PBNOTES_NO_BUMP=1 expect pass "explicit opt-out"
    printf '1.1.0\n' > VERSION; expect fail "bumped but no CHANGELOG entry"
    printf '## 1.1.0\n- feature\n## 1.0.0\n- first\n' > CHANGELOG.md; expect pass "bumped with CHANGELOG entry"
    printf '0.9.0\n' > VERSION; printf '## 0.9.0\n' > CHANGELOG.md; expect fail "version lowered"
    printf 'abc\n' > VERSION; expect fail "malformed VERSION"
    git checkout -q . ; git checkout -q main; expect pass "on main itself"
    # a base without VERSION: the branch introduces it
    rm -rf "$tmp/r"; mkdir "$tmp/r"; cd "$tmp/r"
    git init -q -b main . && git config user.email t@t && git config user.name t
    mkdir app; echo a > app/a.cpp; git add . && git commit -qm base
    git checkout -qb feature/y; printf '1.0.0\n' > VERSION; echo z > app/z.cpp
    expect fail "base has no VERSION: CHANGELOG entry still required"
    printf '## 1.0.0\n' > CHANGELOG.md; expect pass "base has no VERSION: introduced with CHANGELOG"
    exit $fail
fi

check "$(cd "${1:-$(dirname "${BASH_SOURCE[0]}")/..}" && pwd)"
