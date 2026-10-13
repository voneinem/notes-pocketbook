#!/bin/sh
# Generates the two 122x122, 8 bpp, opaque BMP launcher icons (normal and focused) into
# res/icons/. Needs python3 only (no imaging library). The generated files are committed.
# The glyph has no outer frame (spec 010), so it sits with the reader's own frameless line-art icons.
#
#   scripts/make-icons.sh
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/res/icons"
mkdir -p "$OUT"
python3 -I - "$OUT" <<'PY'
import struct, sys

W = H = 122
S = 4                      # supersampling factor: drawn at 4x, box-filtered down, so strokes are anti-aliased

def canvas():
    return [[0] * (W * S) for _ in range(H * S)]   # 0 = background, 1 = ink

def rect(px, x0, y0, x1, y1):
    for y in range(max(0, y0 * S), min(H * S, y1 * S)):
        row = px[y]
        for x in range(max(0, x0 * S), min(W * S, x1 * S)):
            row[x] = 1

def inside_round_rect(x, y, x0, y0, x1, y1, r):
    # x, y in supersampled units; the box and the radius in canvas pixels
    X0, Y0, X1, Y1, R = x0 * S, y0 * S, x1 * S, y1 * S, r * S
    if x < X0 or x >= X1 or y < Y0 or y >= Y1:
        return False
    cx = X0 + R if x < X0 + R else (X1 - R if x >= X1 - R else x)
    cy = Y0 + R if y < Y0 + R else (Y1 - R if y >= Y1 - R else y)
    return (x - cx) ** 2 + (y - cy) ** 2 <= R * R

def draw():
    """The notes glyph, no frame: a page outline (3 px, rounded corners), a header strip and four text lines.
    The built-in icons of the reader are line art of about 75-90 px in a 122 px cell; this one is 76 x 92."""
    px = canvas()
    x0, y0, x1, y1, t, r = 23, 15, 99, 107, 3, 12
    for y in range(H * S):
        row = px[y]
        for x in range(W * S):
            if inside_round_rect(x, y, x0, y0, x1, y1, r) and not inside_round_rect(x, y, x0 + t, y0 + t, x1 - t, y1 - t, max(0, r - t)):
                row[x] = 1
    for y in range(y0 * S, (y0 + 9) * S):                 # header strip, clipped to the rounded sheet
        for x in range(x0 * S, x1 * S):
            if inside_round_rect(x, y, x0, y0, x1, y1, r):
                px[y][x] = 1
    for i, end in enumerate((88, 88, 88, 70)):             # text lines with round ends
        y = 42 + i * 14
        for yy in range(y * S, (y + 4) * S):
            for xx in range(34 * S, end * S):
                if inside_round_rect(xx, yy, 34, y, end, y + 4, 2):
                    px[yy][xx] = 1
    return px

def down(px, invert):
    out = [[0] * W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            n = 0
            for dy in range(S):
                n += sum(px[y * S + dy][x * S:(x + 1) * S])
            ink = n / float(S * S)                          # 0..1
            v = int(round(255 * (1 - ink)))
            out[y][x] = 255 - v if invert else v
    return out

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
glyph = draw()
write_bmp(out + "/pbnotes.bmp", down(glyph, False))   # black on white
write_bmp(out + "/pbnotes_f.bmp", down(glyph, True))  # focused: inverted
PY
echo "wrote $OUT/pbnotes.bmp and $OUT/pbnotes_f.bmp"
