#!/usr/bin/env bash
#
# Assemble the InkPad One install bundle from a freshly built ARM binary.
# Adapted from InkFeed's packaging script (MIT, see NOTICE).
#
#   scripts/package-device.sh <built-arm-elf> [version] [out-dir]
#
# Produces <out-dir>/pbnotes-inkpad-one-<version>.zip, laid out as the PB Store package contract asks
# (appstore-pocketbook): manifest.json and the applications/ tree at the top level, nothing else. A user can
# unzip it straight onto the reader's ext1 partition (copy applications/):
#
#   manifest.json                      created at build time from packaging/manifest.template.json
#   applications/pbnotes.bin           the app (ARM ELF)
#   applications/pbnotes.app           the launcher (shell script)
#   applications/pbnotes-cacert.pem    bundled CA certificates (used when the device has none)
#   applications/pbnotes-emoji.ttf     monochrome emoji font (Noto Emoji, SIL OFL 1.1)
#   applications/pbnotes-emoji-OFL.txt the font's license text
#   applications/pbnotes-licenses/     license texts for the About screens: licenses/manifest.txt and every
#                                      file the manifest names, at the same relative path as in the repository
#   applications/icons/*.bmp           launcher icons
#
# The install / registration steps (INSTALL.md) are not part of the package; they live in the repository.
# The zip is written by `pbstore-catalog pack`, which adds the file list to the manifest. The tool comes from
# the appstore-pocketbook repository; it is found through the environment variable PBSTORE_CATALOG (the tool's
# path) or else on PATH. The package is packed twice and the two results must be identical, then it is checked
# with `pbstore-catalog check-package`; only a package that passes is put into <out-dir>.
#
# The version defaults to the contents of the VERSION file, the same source CMakeLists.txt uses for the
# version compiled into the app. The script fails when the binary does not contain that version string,
# so the zip name and the About screen cannot disagree.
set -euo pipefail

BIN="${1:?usage: package-device.sh <built-arm-elf> [version] [out-dir]}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="${2:-$(head -n 1 "$ROOT/VERSION" 2>/dev/null | tr -d '[:space:]')}"
[ -n "$VERSION" ] || { echo "no version: pass one or create $ROOT/VERSION" >&2; exit 1; }
[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "version is not MAJOR.MINOR.PATCH: $VERSION" >&2; exit 1; }
OUT_DIR="${3:-dist}"

# The store's tool: PBSTORE_CATALOG if set, else PATH. Looked up first so a missing tool is reported before any work.
if [ -n "${PBSTORE_CATALOG:-}" ]; then
    [ -f "$PBSTORE_CATALOG" ] && [ -x "$PBSTORE_CATALOG" ] || {
        echo "PBSTORE_CATALOG is set but is not an executable file: $PBSTORE_CATALOG" >&2; exit 1; }
    PBSTORE="$PBSTORE_CATALOG"
else
    PBSTORE="$(command -v pbstore-catalog || true)"
    [ -n "$PBSTORE" ] || {
        echo "pbstore-catalog not found. Put it on PATH or set PBSTORE_CATALOG to the tool" \
             "(build it from the appstore-pocketbook repository, tag v0.1.1 or newer)." >&2; exit 1; }
fi

for f in "$BIN" \
         "$ROOT/platform/pocketbook/pbnotes.app" \
         "$ROOT/third_party/cacert/cacert.pem" \
         "$ROOT/third_party/noto-emoji/NotoEmoji.ttf" "$ROOT/third_party/noto-emoji/OFL.txt" \
         "$ROOT/res/icons/pbnotes.bmp" "$ROOT/res/icons/pbnotes_f.bmp" \
         "$ROOT/packaging/manifest.template.json"; do
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
work="$(mktemp -d)"
trap 'rm -rf "$stage" "$work"' EXIT
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
sed "s/@VERSION@/$VERSION/g" "$ROOT/packaging/manifest.template.json" > "$stage/manifest.json"

left="$(grep -Eo '@[A-Z_]+@' "$stage/manifest.json" | head -n 1 || true)"
[ -z "$left" ] || { echo "manifest template has an unfilled placeholder: $left" >&2; exit 1; }

# A ZIP of the same version from an earlier build is replaced, never kept next to a failed build.
rm -f "$ZIP"
# Pack twice: the store's tool must give the same zip for the same files.
for n in a b; do
    "$PBSTORE" pack "$stage" --out "$work/$n.zip" >&2 || { echo "pbstore-catalog pack failed" >&2; exit 1; }
done
cmp -s "$work/a.zip" "$work/b.zip" || { echo "package is not reproducible: two builds from the same files differ" >&2; exit 1; }
"$PBSTORE" check-package "$work/a.zip" >&2 || { echo "package check failed" >&2; exit 1; }
mv "$work/a.zip" "$ZIP"
echo "$ZIP"
