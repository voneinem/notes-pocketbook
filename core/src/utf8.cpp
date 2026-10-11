#include "pbnotes/utf8.h"

#include <cstdint>

namespace pbnotes {

namespace {

struct Decoded {
    bool valid = false;
    uint32_t cp = 0;
    size_t length = 1;
};

Decoded decode(std::string_view s, size_t pos) {
    Decoded d;
    if (pos >= s.size()) return d;
    const unsigned char b = static_cast<unsigned char>(s[pos]);
    size_t n;
    uint32_t cp;
    uint32_t min;
    if (b < 0x80) { d.valid = true; d.cp = b; return d; }
    if ((b & 0xE0) == 0xC0) { n = 2; cp = b & 0x1F; min = 0x80; }
    else if ((b & 0xF0) == 0xE0) { n = 3; cp = b & 0x0F; min = 0x800; }
    else if ((b & 0xF8) == 0xF0) { n = 4; cp = b & 0x07; min = 0x10000; }
    else return d;
    if (pos + n > s.size()) return d;
    for (size_t k = 1; k < n; ++k) {
        const unsigned char c = static_cast<unsigned char>(s[pos + k]);
        if ((c & 0xC0) != 0x80) return d;
        cp = (cp << 6) | (c & 0x3F);
    }
    if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return d;  // overlong, range, surrogate
    d.valid = true;
    d.cp = cp;
    d.length = n;
    return d;
}

bool is_regional_indicator(uint32_t cp) { return cp >= 0x1F1E6 && cp <= 0x1F1FF; }

// Code points that attach to the previous character.
bool is_extender(uint32_t cp) {
    return (cp >= 0x0300 && cp <= 0x036F) ||   // combining diacritical marks
           (cp >= 0x1AB0 && cp <= 0x1AFF) || (cp >= 0x1DC0 && cp <= 0x1DFF) ||
           (cp >= 0x20D0 && cp <= 0x20FF) ||   // combining marks for symbols, including the keycap U+20E3
           (cp >= 0xFE00 && cp <= 0xFE0F) ||   // variation selectors
           (cp >= 0xFE20 && cp <= 0xFE2F) ||   // combining half marks
           (cp >= 0x1F3FB && cp <= 0x1F3FF) || // emoji skin tone modifiers
           (cp >= 0xE0020 && cp <= 0xE007F) || // tag characters
           (cp >= 0xE0100 && cp <= 0xE01EF);   // variation selectors supplement
}

constexpr uint32_t kZwj = 0x200D;

}  // namespace

size_t utf8_cluster_end(std::string_view text, size_t pos) {
    if (pos >= text.size()) return text.size();
    const Decoded first = decode(text, pos);
    if (!first.valid) return pos + 1;
    size_t end = pos + first.length;

    if (is_regional_indicator(first.cp)) {  // a flag is a pair of regional indicators
        const Decoded second = decode(text, end);
        if (second.valid && is_regional_indicator(second.cp)) end += second.length;
    }
    for (;;) {
        const Decoded next = decode(text, end);
        if (!next.valid) break;
        if (is_extender(next.cp)) {
            end += next.length;
            continue;
        }
        if (next.cp == kZwj) {
            const Decoded joined = decode(text, end + next.length);
            if (joined.valid && joined.cp >= 0x80) {  // ZWJ + a character: one unit
                end += next.length + joined.length;
                continue;
            }
        }
        break;
    }
    return end;
}

}  // namespace pbnotes
