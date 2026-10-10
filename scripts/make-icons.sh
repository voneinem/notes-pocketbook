#!/bin/sh
# Generates the two 122x122, 8 bpp, opaque BMP launcher icons (normal and focused) into
# res/icons/. Needs python3 only (no imaging library). The generated files are committed.
#
#   scripts/make-icons.sh
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/res/icons"
mkdir -p "$OUT"
python3 -I - "$OUT" <<'PY'
import struct, sys

W = H = 122

def canvas(bg):
    return [[bg] * W for _ in range(H)]

def rect(px, x0, y0, x1, y1, v):
    for y in range(max(0, y0), min(H, y1)):
        for x in range(max(0, x0), min(W, x1)):
            px[y][x] = v

def frame(px, x0, y0, x1, y1, t, v):
    rect(px, x0, y0, x1, y0 + t, v)
    rect(px, x0, y1 - t, x1, y1, v)
    rect(px, x0, y0, x0 + t, y1, v)
    rect(px, x1 - t, y0, x1, y1, v)

def draw(bg, fg):
    px = canvas(bg)
    frame(px, 6, 6, W - 6, H - 6, 4, fg)          # outer frame
    frame(px, 28, 20, W - 28, H - 20, 3, fg)       # the note sheet
    rect(px, 28, 20, W - 28, 36, fg)               # sheet header
    for i in range(4):                             # text lines
        y = 48 + i * 14
        rect(px, 38, y, W - 38 - (0 if i < 3 else 18), y + 4, fg)
    return px

def write_bmp(path, px):
    row = (W + 3) & ~3                              # rows padded to 4 bytes
    pal = b"".join(struct.pack("<BBBB", i, i, i, 0) for i in range(256))
    data = b"".join(bytes(px[y]) + b"\0" * (row - W) for y in range(H - 1, -1, -1))
    off = 14 + 40 + len(pal)
    hdr = b"BM" + struct.pack("<IHHI", off + len(data), 0, 0, off)
    info = struct.pack("<IiiHHIIiiII", 40, W, H, 1, 8, 0, len(data), 2835, 2835, 256, 0)
    with open(path, "wb") as f:
        f.write(hdr + info + pal + data)

out = sys.argv[1]
write_bmp(out + "/pbnotes.bmp", draw(255, 0))        # black on white
write_bmp(out + "/pbnotes_f.bmp", draw(0, 255))      # focused: inverted
PY
echo "wrote $OUT/pbnotes.bmp and $OUT/pbnotes_f.bmp"
