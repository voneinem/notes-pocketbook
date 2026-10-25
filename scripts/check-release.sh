#!/usr/bin/env bash
# Release gate: the tag, the VERSION file, the name of the package and the version inside its manifest.json must be
# the same version (spec 011, FR-013). The store finds the package of a version through the tag, so a mismatch makes the
# package unfindable or wrong.
#
#   scripts/check-release.sh [zip] [tag]    default zip: dist/pbnotes-inkpad-one-<VERSION>.zip, default tag: v<VERSION>
#   scripts/check-release.sh --self-test    prove the rules on throw-away packages
#
# Exit 0: all agree. Exit 1: a mismatch (one line each) or no readable manifest. Exit 2: the package does not exist.
# Needs unzip; the self-test builds its packages with python3.
set -euo pipefail

check() {  # check <repo-root> <zip> <tag>   (empty zip / tag: the defaults)
    local root="$1" zip="$2" tag="$3" ver mver bad=0
    ver="$(head -n 1 "$root/VERSION" 2>/dev/null | tr -d '[:space:]')"
    [ -n "$ver" ] || { echo "release check FAILED: no VERSION file in $root" >&2; return 1; }
    [ -n "$zip" ] || zip="$root/dist/pbnotes-inkpad-one-$ver.zip"
    [ -n "$tag" ] || tag="v$ver"
    [ -f "$zip" ] || { echo "release check: package not found: $zip" >&2; return 2; }

    mver="$(unzip -p "$zip" manifest.json 2>/dev/null \
        | sed -n 's/.*"version"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' | head -n 1 || true)"
    [ -n "$mver" ] || { echo "release check FAILED: $zip has no readable manifest.json with a version" >&2; return 1; }

    if [ "$tag" != "v$mver" ]; then
        echo "release check FAILED: tag $tag does not match manifest version $mver (expected v$mver)" >&2; bad=1
    fi
    if [ "$ver" != "$mver" ]; then
        echo "release check FAILED: VERSION says $ver but the manifest version is $mver" >&2; bad=1
    fi
    if [ "$(basename "$zip")" != "pbnotes-inkpad-one-$mver.zip" ]; then
        echo "release check FAILED: package name $(basename "$zip") does not match manifest version $mver" >&2; bad=1
    fi
    [ "$bad" = 0 ] || return 1
    echo "release check passed ($mver)"
}

if [ "${1:-}" = "--self-test" ]; then
    tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
    fail=0
    mkdir -p "$tmp/r/dist" "$tmp/m"
    expect() {  # expect <exit-code> <description> <zip> <tag>
        local want="$1" rc=0
        ( check "$tmp/r" "$3" "$4" ) >/dev/null 2>&1 || rc=$?
        if [ "$rc" = "$want" ]; then echo "ok   $2"; else echo "FAIL $2 (exit $rc, wanted $want)"; fail=1; fi
    }
    mkzip() {  # mkzip <file name> <manifest version, or - for a package without manifest>
        rm -f "$tmp/r/dist/$1"
        if [ "$2" = "-" ]; then printf 'x\n' > "$tmp/m/readme.txt"; ( cd "$tmp/m" && python3 -m zipfile -c "$tmp/r/dist/$1" readme.txt )
        else printf '{\n  "version": "%s"\n}\n' "$2" > "$tmp/m/manifest.json"; ( cd "$tmp/m" && python3 -m zipfile -c "$tmp/r/dist/$1" manifest.json ); fi
    }
    printf '1.2.3\n' > "$tmp/r/VERSION"
    good="$tmp/r/dist/pbnotes-inkpad-one-1.2.3.zip"
    mkzip pbnotes-inkpad-one-1.2.3.zip 1.2.3
    expect 0 "everything agrees" "$good" v1.2.3
    expect 1 "tag differs from the manifest version" "$good" v1.2.4
    expect 1 "tag without the v prefix" "$good" 1.2.3
    printf '1.2.4\n' > "$tmp/r/VERSION"; expect 1 "VERSION differs from the manifest version" "$good" v1.2.3
    printf '1.2.3\n' > "$tmp/r/VERSION"
    mkzip pbnotes-inkpad-one-1.2.4.zip 1.2.3; expect 1 "package name differs from the manifest version" "$tmp/r/dist/pbnotes-inkpad-one-1.2.4.zip" v1.2.3
    mkzip pbnotes-inkpad-one-1.2.3.zip -; expect 1 "package without manifest.json" "$good" v1.2.3
    expect 2 "package does not exist" "$tmp/r/dist/nonexistent.zip" v1.2.3
    exit $fail
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
check "$ROOT" "${1:-}" "${2:-}"
