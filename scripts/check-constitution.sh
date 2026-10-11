#!/usr/bin/env bash
# Mechanical checks of the project constitution (run by CTest and scripts/pre-merge.sh).
#
#   scripts/check-constitution.sh [repo-root]    scan (default: this repository)
#   scripts/check-constitution.sh --self-test    prove that every rule below detects a violation
#
# Rules (a finding makes the script exit non-zero):
#   R1 TLS verification is never disabled, in code AND shell scripts:
#        curl -k, curl --insecure, wget --no-check-certificate,
#        CURLOPT_SSL_VERIFYPEER / CURLOPT_SSL_VERIFYHOST set to 0 (VERIFYHOST must be 2),
#        CURLOPT_SSL_VERIFYSTATUS, CURLOPT_PROXY_SSL_VERIFYPEER, CURLOPT_PROXY_SSL_VERIFYHOST,
#        CURLOPT_DOH_SSL_VERIFYPEER, CURLOPT_DOH_SSL_VERIFYHOST, CURLOPT_DOH_SSL_VERIFYSTATUS
#        set to 0 / false.
#   R2 No hard-coded SDK locations ("pocketbook-sdk", "/opt/pocketbook") in build files and scripts.
#   R3 core/ is Qt-free and InkView-free.
#   R4 No logging call is given a password, user name, note content or title.
#   R5 No absolute local paths (user home directories, drive letters) in any tracked text file.
#   R6 No log::text / log::count key that is, starts with or ends with the word url, path, file, name
#        or address (e.g. "url", "file_name", "link_url") in core/, bridge/ and app/ (spec 002 FR-033: no link addresses, attachment names or file paths in logs).
set -u

SELF="$(basename "${BASH_SOURCE[0]}")"

# Lists the text files to scan, relative to $1. Uses git when available, else find.
list_files() {
    local root="$1"
    if [ -d "$root/.git" ] && command -v git >/dev/null 2>&1; then
        git -C "$root" ls-files -z --cached --others --exclude-standard
    else
        (cd "$root" && find . -type f -not -path './.git/*' -not -path './build*' -print0 | sed -z 's#^\./##')
    fi
}

scan() {
    local root="$1" bad=0 f
    while IFS= read -r -d '' f; do
        [ -f "$root/$f" ] || continue
        case "$f" in
            third_party/*|"scripts/$SELF"|.claude/*|*.bmp|*.zip|*.pem) continue ;;
        esac
        grep -Iq . "$root/$f" 2>/dev/null || continue   # skip binary/empty files
        local p="$root/$f"

        # R5: absolute local paths, everywhere
        if grep -nE '/home/[A-Za-z0-9._-]+/|/Users/[A-Za-z0-9._-]+/|(^|[^A-Za-z0-9])[A-Za-z]:\\[A-Za-z0-9_.-]' "$p" >/dev/null 2>&1; then
            grep -nE '/home/[A-Za-z0-9._-]+/|/Users/[A-Za-z0-9._-]+/|(^|[^A-Za-z0-9])[A-Za-z]:\\[A-Za-z0-9_.-]' "$p" | sed "s#^#R5 $f:#"
            bad=1
        fi

        case "$f" in
            core/*|bridge/*|app/*|tests/*|scripts/*|platform/*|cmake/*|CMakeLists.txt|*/CMakeLists.txt)
                # R1: TLS bypasses
                if grep -nE '(curl|wget)[^#|;&]*[[:space:]](-k|--insecure|--no-check-certificate)([[:space:]]|$)' "$p" >/dev/null 2>&1 ||
                   grep -nE 'CURLOPT_(PROXY_|DOH_)?SSL_VERIFY(PEER|HOST|STATUS)[[:space:]]*,[[:space:]]*(0|false|FALSE)' "$p" >/dev/null 2>&1 ||
                   grep -nE 'CURLOPT_SSL_VERIFYHOST[[:space:]]*,[[:space:]]*1[^0-9]' "$p" >/dev/null 2>&1 ||
                   grep -nE 'CURLOPT_SSL_VERIFYSTATUS' "$p" >/dev/null 2>&1; then
                    grep -nE '(curl|wget)[^#|;&]*[[:space:]](-k|--insecure|--no-check-certificate)([[:space:]]|$)|CURLOPT_(PROXY_|DOH_)?SSL_VERIFY(PEER|HOST|STATUS)[[:space:]]*,[[:space:]]*(0|false|FALSE|1[^0-9])|CURLOPT_SSL_VERIFYSTATUS' "$p" | sed "s#^#R1 $f:#"
                    bad=1
                fi
                ;;
        esac
        case "$f" in
            cmake/*|CMakeLists.txt|*/CMakeLists.txt|scripts/*|platform/*)
                # R2: hard-coded SDK locations
                if grep -nE 'pocketbook-sdk|/opt/pocketbook' "$p" >/dev/null 2>&1; then
                    grep -nE 'pocketbook-sdk|/opt/pocketbook' "$p" | sed "s#^#R2 $f:#"
                    bad=1
                fi
                ;;
        esac
        case "$f" in
            core/*)
                # R3: no Qt / InkView in the core
                if grep -nE '#[[:space:]]*include[[:space:]]*[<"](Q[A-Za-z]|inkview)|\bQObject\b|\bQString\b' "$p" >/dev/null 2>&1; then
                    grep -nE '#[[:space:]]*include[[:space:]]*[<"](Q[A-Za-z]|inkview)|\bQObject\b|\bQString\b' "$p" | sed "s#^#R3 $f:#"
                    bad=1
                fi
                ;;
        esac
        case "$f" in
            core/*|bridge/*|app/*)
                # R4: personal data into the log
                if grep -nE '(log::|qDebug|qWarning|qInfo|std::cerr|fprintf\(stderr)[^;]*(password|passwd|username|\.content|->content|\.title|->title)' "$p" >/dev/null 2>&1; then
                    grep -nE '(log::|qDebug|qWarning|qInfo|std::cerr|fprintf\(stderr)[^;]*(password|passwd|username|\.content|->content|\.title|->title)' "$p" | sed "s#^#R4 $f:#"
                    bad=1
                fi
                # R6: log keys that would carry addresses, names or paths
                if grep -nE 'log::(text|count)[[:space:]]*\([[:space:]]*"([A-Za-z0-9.]+_)?(url|path|file|name|address)(_[A-Za-z0-9_.]+)?"' "$p" >/dev/null 2>&1; then
                    grep -nE 'log::(text|count)[[:space:]]*\([[:space:]]*"([A-Za-z0-9.]+_)?(url|path|file|name|address)(_[A-Za-z0-9_.]+)?"' "$p" | sed "s#^#R6 $f:#"
                    bad=1
                fi
                ;;
        esac
    done < <(list_files "$root")
    return $bad
}

if [ "${1:-}" = "--self-test" ]; then
    tmp="$(mktemp -d)"
    trap 'rm -rf "$tmp"' EXIT
    fail=0
    mkdir -p "$tmp/core/src" "$tmp/scripts" "$tmp/cmake" "$tmp/docs"
    expect() {  # expect <pass|fail> <description>
        scan "$tmp" >/dev/null 2>&1
        local rc=$?
        if { [ "$1" = pass ] && [ $rc -eq 0 ]; } || { [ "$1" = fail ] && [ $rc -ne 0 ]; }; then
            echo "ok   $2"
        else
            echo "FAIL $2 (rc=$rc)"; fail=1
        fi
    }
    violate() {  # violate <file> <content> <description>
        printf '%s\n' "$2" > "$tmp/$1"
        expect fail "$3"
        rm -f "$tmp/$1"
    }
    printf 'int main() { return 0; }\n' > "$tmp/core/src/ok.cpp"
    printf '#!/bin/sh\necho hello\n' > "$tmp/scripts/ok.sh"
    expect pass "a clean tree passes"
    printf 'curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);\ncurl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);\n' > "$tmp/core/src/tls_ok.cpp"
    expect pass "VERIFYPEER 1 / VERIFYHOST 2 are allowed"; rm "$tmp/core/src/tls_ok.cpp"

    violate scripts/a.sh 'curl -k https://example.com' "R1: curl -k"
    violate scripts/a.sh 'curl -sS --insecure https://example.com' "R1: curl --insecure"
    violate scripts/a.sh 'wget --no-check-certificate https://example.com' "R1: wget --no-check-certificate"
    violate core/src/a.cpp 'curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);' "R1: VERIFYPEER 0"
    violate core/src/a.cpp 'curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 0L);' "R1: VERIFYHOST 0"
    violate core/src/a.cpp 'curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 1L);' "R1: VERIFYHOST 1"
    violate core/src/a.cpp 'curl_easy_setopt(c, CURLOPT_SSL_VERIFYSTATUS, 0L);' "R1: VERIFYSTATUS"
    violate core/src/a.cpp 'curl_easy_setopt(c, CURLOPT_PROXY_SSL_VERIFYPEER, 0L);' "R1: PROXY_SSL_VERIFYPEER 0"
    violate core/src/a.cpp 'curl_easy_setopt(c, CURLOPT_DOH_SSL_VERIFYHOST, false);' "R1: DOH_SSL_VERIFYHOST false"
    violate cmake/a.cmake 'set(SDK_PATH /opt/pocketbook-sdk/x)' "R2: /opt/pocketbook path"
    violate scripts/a.sh 'SDK=../pocketbook-sdk-6.11/sdk' "R2: pocketbook-sdk path"
    violate core/src/a.cpp '#include <QString>' "R3: Qt include in core"
    violate core/src/a.cpp '#include <inkview.h>' "R3: inkview include in core"
    violate core/src/a.cpp 'log::text("x", account.password);' "R4: password into the log"
    violate core/src/a.cpp 'log::text("x", note.content);' "R4: note content into the log"
    violate core/src/a.cpp 'log::text("url", link);' "R6: url key in the log"
    violate core/src/a.cpp 'log::text("file_name", n);' "R6: file_name key in the log"
    violate core/src/a.cpp 'log::count("path", 1);' "R6: path key in the log"
    violate core/src/a.cpp 'log::text("address", a);' "R6: address key in the log"
    printf 'log::text("result", r); log::count("pulled", 3); log::path(p);\n' > "$tmp/core/src/log_ok.cpp"
    expect pass "result/count keys and log::path are allowed"; rm "$tmp/core/src/log_ok.cpp"
    violate docs/a.md 'see /home/someone/project/file' "R5: Linux home path"
    violate docs/a.md 'see /Users/someone/project/file' "R5: macOS home path"
    violate docs/a.md 'see C:\Users\x\file' "R5: drive-letter path"
    exit $fail
fi

ROOT="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
if scan "$ROOT"; then
    echo "constitution checks passed"
else
    echo "constitution checks FAILED (see findings above)" >&2
    exit 1
fi
