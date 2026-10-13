#!/usr/bin/env bash
# Mechanical check of the user interface style (spec 010 FR-006, SC-003): no bordered text button remains in the QML.
# Run by CTest and scripts/pre-merge.sh.
#
#   scripts/check-ui-style.sh [repo-root]    scan (default: this repository)
#   scripts/check-ui-style.sh --self-test    prove that every rule below detects a violation
#
# Rules (a finding makes the script exit non-zero), over every file under qml/ (QML, the qmldir and the resource list):
#   U1 The bordered text button `InkButton` is not used and not declared (it was deleted: every action is an icon control,
#      a tool of a bottom panel, a footer button of a dialog or a tappable row).
#   U2 No `bordered:` property (the switch that drew a border round a text button).
#   U3 No Qt Quick Controls `Button { ... }` (its own bevel and border read as a grey bar on the panel).
set -u

SELF="$(basename "${BASH_SOURCE[0]}")"

scan() {
    local root="$1" bad=0 f hits
    [ -d "$root/qml" ] || { echo "$SELF: no qml/ directory under $root" >&2; return 2; }
    while IFS= read -r -d '' f; do
        hits="$(grep -nE 'InkButton' "$f" || true)"
        if [ -n "$hits" ]; then echo "U1 $f: InkButton"; echo "$hits" | sed 's/^/    /'; bad=1; fi
        case "$f" in *.qml)
            hits="$(grep -nE '(^|[^A-Za-z_.])bordered[[:space:]]*:' "$f" || true)"
            if [ -n "$hits" ]; then echo "U2 $f: bordered property"; echo "$hits" | sed 's/^/    /'; bad=1; fi
            hits="$(grep -nE '(^|[^A-Za-z_.])Button[[:space:]]*\{' "$f" || true)"
            if [ -n "$hits" ]; then echo "U3 $f: Qt Quick Controls Button"; echo "$hits" | sed 's/^/    /'; bad=1; fi
            ;;
        esac
    done < <(find "$root/qml" -type f \( -name '*.qml' -o -name qmldir -o -name qml.qrc \) -print0)
    return $bad
}

self_test() {
    local tmp rc=0
    tmp="$(mktemp -d)"
    trap 'rm -rf "$tmp"' RETURN
    mk() { mkdir -p "$tmp/$1/qml"; }
    # a clean tree passes
    mk good
    printf 'import QtQuick\nItem { IconButton { text: "x" } ToolButton { text: "y" } }\n' > "$tmp/good/qml/A.qml"
    printf 'IconButton 1.0 IconButton.qml\n' > "$tmp/good/qml/qmldir"
    scan "$tmp/good" >/dev/null || { echo "self-test: a clean tree was reported" >&2; rc=1; }
    # each bad pattern is found
    for case_ in "U1:Item { InkButton { text: \"x\" } }" "U2:Item { property bool bordered: true }" "U3:import QtQuick.Controls
Button { text: \"x\" }" ; do
        mk bad
        printf 'import QtQuick\n%s\n' "${case_#*:}" > "$tmp/bad/qml/B.qml"
        if scan "$tmp/bad" >/dev/null; then echo "self-test: ${case_%%:*} was not detected" >&2; rc=1; fi
        rm -rf "$tmp/bad"
    done
    # the qmldir and the resource list are scanned too
    mk bad2
    printf 'InkButton 1.0 InkButton.qml\n' > "$tmp/bad2/qml/qmldir"
    if scan "$tmp/bad2" >/dev/null; then echo "self-test: an InkButton line in qmldir was not detected" >&2; rc=1; fi
    return $rc
}

case "${1:-}" in
    --self-test) self_test; rc=$?; [ $rc -eq 0 ] && echo "check-ui-style self-test passed"; exit $rc ;;
    *) ROOT="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
       scan "$ROOT"; rc=$?
       if [ $rc -ne 0 ]; then echo "check-ui-style: the findings above must be fixed" >&2; fi
       exit $rc ;;
esac
