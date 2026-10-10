#!/usr/bin/env bash
#
# Assemble the InkPad One install bundle from a freshly built ARM binary.
# Adapted from InkFeed's packaging script (MIT, see NOTICE).
#
#   scripts/package-device.sh <built-arm-elf> [version] [out-dir]
#
# Produces <out-dir>/pbnotes-inkpad-one-<version>.zip, laid out so the user can unzip it
# straight onto the reader's ext1 partition:
#
#   applications/pbnotes.bin           the app (ARM ELF)
#   applications/pbnotes.app           the launcher (shell script)
#   applications/pbnotes-cacert.pem    bundled CA certificates (used when the device has none)
#   applications/icons/*.bmp           launcher icons
#   INSTALL.md                         install / registration steps
#
# The version defaults to `git describe`; pass it explicitly to override.
set -euo pipefail

BIN="${1:?usage: package-device.sh <built-arm-elf> [version] [out-dir]}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="${2:-$(git -C "$ROOT" describe --tags --always 2>/dev/null | sed 's/^v//')}"
VERSION="${VERSION:-0.0.0}"
OUT_DIR="${3:-dist}"

for f in "$BIN" \
         "$ROOT/platform/pocketbook/pbnotes.app" \
         "$ROOT/third_party/cacert/cacert.pem" \
         "$ROOT/res/icons/pbnotes.bmp" "$ROOT/res/icons/pbnotes_f.bmp" \
         "$ROOT/INSTALL.md"; do
    [ -f "$f" ] || { echo "missing input: $f" >&2; exit 1; }
done

OUT_DIR="$(mkdir -p "$OUT_DIR" && cd "$OUT_DIR" && pwd)"
ZIP="$OUT_DIR/pbnotes-inkpad-one-$VERSION.zip"

stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
apps="$stage/applications"
mkdir -p "$apps/icons"

install -m 777 "$BIN"                                   "$apps/pbnotes.bin"
install -m 777 "$ROOT/platform/pocketbook/pbnotes.app"  "$apps/pbnotes.app"
install -m 644 "$ROOT/third_party/cacert/cacert.pem"    "$apps/pbnotes-cacert.pem"
install -m 666 "$ROOT/res/icons/pbnotes.bmp"            "$apps/icons/pbnotes.bmp"
install -m 666 "$ROOT/res/icons/pbnotes_f.bmp"          "$apps/icons/pbnotes_f.bmp"
install -m 644 "$ROOT/INSTALL.md"                       "$stage/INSTALL.md"

rm -f "$ZIP"
( cd "$stage" && zip -r -q "$ZIP" . )
echo "$ZIP"
