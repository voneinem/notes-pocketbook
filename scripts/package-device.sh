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
#   applications/pbnotes-emoji.ttf     monochrome emoji font (Noto Emoji, SIL OFL 1.1)
#   applications/pbnotes-emoji-OFL.txt the font's license text
#   applications/pbnotes-licenses/     license texts for the About screens: licenses/manifest.txt and every
#                                      file the manifest names, at the same relative path as in the repository
#   applications/icons/*.bmp           launcher icons
#   INSTALL.md                         install / registration steps
#
# The version defaults to the contents of the VERSION file, the same source CMakeLists.txt uses for the
# version compiled into the app. The script fails when the binary does not contain that version string,
# so the zip name and the About screen cannot disagree.
set -euo pipefail

BIN="${1:?usage: package-device.sh <built-arm-elf> [version] [out-dir]}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="${2:-$(head -n 1 "$ROOT/VERSION" 2>/dev/null | tr -d '[:space:]')}"
[ -n "$VERSION" ] || { echo "no version: pass one or create $ROOT/VERSION" >&2; exit 1; }
OUT_DIR="${3:-dist}"

for f in "$BIN" \
         "$ROOT/platform/pocketbook/pbnotes.app" \
         "$ROOT/third_party/cacert/cacert.pem" \
         "$ROOT/third_party/noto-emoji/NotoEmoji.ttf" "$ROOT/third_party/noto-emoji/OFL.txt" \
         "$ROOT/res/icons/pbnotes.bmp" "$ROOT/res/icons/pbnotes_f.bmp" \
         "$ROOT/INSTALL.md"; do
    [ -f "$f" ] || { echo "missing input: $f" >&2; exit 1; }
done

MANIFEST="$ROOT/licenses/manifest.txt"
[ -f "$MANIFEST" ] || { echo "missing input: $MANIFEST" >&2; exit 1; }

# The About screen shows the version compiled into the binary; it must be the one the zip is named after.
if ! grep -aqF -- "$VERSION" "$BIN"; then
    echo "version mismatch: the binary does not contain the version string '$VERSION'." >&2
    echo "Rebuild (cmake reads the version from the VERSION file) or pass the version the binary was built with." >&2
    exit 1
fi

# Every license text the manifest names (comma-separated `text:` values), relative to the repository root.
license_files="$(sed -n 's/^text:[[:space:]]*//p' "$MANIFEST" | tr ',' '\n' | sed 's/^[[:space:]]*//; s/[[:space:]]*$//' | grep -v '^$' | sort -u)"
for t in $license_files; do
    [ -f "$ROOT/$t" ] || { echo "license text named by licenses/manifest.txt is missing: $t" >&2; exit 1; }
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
install -m 644 "$ROOT/third_party/noto-emoji/NotoEmoji.ttf" "$apps/pbnotes-emoji.ttf"
install -m 644 "$ROOT/third_party/noto-emoji/OFL.txt"   "$apps/pbnotes-emoji-OFL.txt"
install -D -m 644 "$MANIFEST" "$apps/pbnotes-licenses/licenses/manifest.txt"
for t in $license_files; do
    install -D -m 644 "$ROOT/$t" "$apps/pbnotes-licenses/$t"
done
install -m 666 "$ROOT/res/icons/pbnotes.bmp"            "$apps/icons/pbnotes.bmp"
install -m 666 "$ROOT/res/icons/pbnotes_f.bmp"          "$apps/icons/pbnotes_f.bmp"
install -m 644 "$ROOT/INSTALL.md"                       "$stage/INSTALL.md"

rm -f "$ZIP"
( cd "$stage" && zip -r -q "$ZIP" . )
echo "$ZIP"
