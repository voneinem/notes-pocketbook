// Spec 010 US1 (FR-001..FR-004): the launcher icon has no frame, is sized like the built-in icons, is anti-aliased
// and has two different variants. Contract: specs/010-builtin-look-gui-overhaul/contracts/ui-components.md section 6.
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "test_support.h"

namespace {

struct Bmp {
    int w = 0, h = 0, bpp = 0;
    std::vector<int> px;  // grey level, row 0 = top
    int at(int x, int y) const { return px[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)]; }
};

uint32_t u32(const std::vector<unsigned char>& d, size_t o) {
    return d[o] | (d[o + 1] << 8) | (d[o + 2] << 16) | (static_cast<uint32_t>(d[o + 3]) << 24);
}

// An 8 bpp BMP with a grey palette; rows are bottom-up and padded to 4 bytes.
bool load(const std::string& name, Bmp& out) {
    std::ifstream f(std::string(PBNOTES_SOURCE_DIR) + "/res/icons/" + name, std::ios::binary);
    std::vector<unsigned char> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (d.size() < 54 || d[0] != 'B' || d[1] != 'M') return false;
    const size_t off = u32(d, 10);
    out.w = static_cast<int>(u32(d, 18));
    out.h = static_cast<int>(u32(d, 22));
    out.bpp = d[28] | (d[29] << 8);
    if (out.bpp != 8 || out.w <= 0 || out.h <= 0) return false;
    const size_t pal = 14 + u32(d, 14);  // palette follows the info header
    const size_t row = (static_cast<size_t>(out.w) + 3) & ~static_cast<size_t>(3);
    if (d.size() < off + row * static_cast<size_t>(out.h)) return false;
    out.px.assign(static_cast<size_t>(out.w) * static_cast<size_t>(out.h), 0);
    for (int y = 0; y < out.h; ++y)
        for (int x = 0; x < out.w; ++x) {
            const unsigned char idx = d[off + row * static_cast<size_t>(out.h - 1 - y) + static_cast<size_t>(x)];
            out.px[static_cast<size_t>(y) * static_cast<size_t>(out.w) + static_cast<size_t>(x)] = d[pal + 4u * idx];  // blue = grey
        }
    return true;
}

// Checks one variant: size, no frame (the outer 10 px ring is all background), glyph box 60..85 % and centred,
// anti-aliased strokes.
void check_variant(const std::string& name, int background) {
    Bmp b;
    PB_CHECK(load(name, b));
    PB_CHECK_EQ(b.w, 122);
    PB_CHECK_EQ(b.h, 122);
    PB_CHECK_EQ(b.bpp, 8);
    bool ring_clear = true;
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x)
            if ((x < 10 || y < 10 || x >= b.w - 10 || y >= b.h - 10) && b.at(x, y) != background) ring_clear = false;
    PB_CHECK(ring_clear);  // no frame: nothing but background in the outer ring

    int x0 = b.w, x1 = -1, y0 = b.h, y1 = -1, soft = 0;
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x) {
            const int v = b.at(x, y);
            if (std::abs(v - background) > 64) {
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
            if (v > 40 && v < 215) ++soft;
        }
    PB_CHECK(x1 >= x0 && y1 >= y0);
    const int gw = x1 - x0 + 1, gh = y1 - y0 + 1;
    PB_CHECK(gw >= 73 && gw <= 103);  // 60..85 % of 122
    PB_CHECK(gh >= 73 && gh <= 103);
    PB_CHECK(std::abs((x0 + x1) / 2 - 61) <= 4);
    PB_CHECK(std::abs((y0 + y1) / 2 - 61) <= 4);
    PB_CHECK(soft >= 100);  // anti-aliased
}

PB_TEST(the_normal_icon_has_no_frame_and_the_glyph_is_sized_and_centred) { check_variant("pbnotes.bmp", 255); }

PB_TEST(the_focused_icon_has_no_frame_and_the_glyph_is_sized_and_centred) { check_variant("pbnotes_f.bmp", 0); }

PB_TEST(the_two_variants_are_different_pictures) {
    Bmp a, b;
    PB_CHECK(load("pbnotes.bmp", a));
    PB_CHECK(load("pbnotes_f.bmp", b));
    PB_CHECK_EQ(a.px.size(), b.px.size());
    size_t differing = 0;
    for (size_t i = 0; i < a.px.size(); ++i)
        if (a.px[i] != b.px[i]) ++differing;
    PB_CHECK(differing * 2 >= a.px.size());
}

}  // namespace

PB_TEST_MAIN()
