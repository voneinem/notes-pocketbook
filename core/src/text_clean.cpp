#include "pbnotes/text_clean.h"

#include <cstdint>

namespace pbnotes {

namespace {

// Decodes one code point at s[pos]; returns its length in bytes, or 0 for an invalid sequence.
size_t decode(std::string_view s, size_t pos, uint32_t* cp) {
    const unsigned char b0 = static_cast<unsigned char>(s[pos]);
    if (b0 < 0x80) {
        *cp = b0;
        return 1;
    }
    size_t len;
    uint32_t value, min;
    if (b0 >= 0xC2 && b0 <= 0xDF) {
        len = 2; value = b0 & 0x1F; min = 0x80;
    } else if (b0 >= 0xE0 && b0 <= 0xEF) {
        len = 3; value = b0 & 0x0F; min = 0x800;
    } else if (b0 >= 0xF0 && b0 <= 0xF4) {
        len = 4; value = b0 & 0x07; min = 0x10000;
    } else {
        return 0;  // continuation byte, C0/C1 (overlong), F5..FF
    }
    if (pos + len > s.size()) return 0;
    for (size_t i = 1; i < len; ++i) {
        const unsigned char b = static_cast<unsigned char>(s[pos + i]);
        if ((b & 0xC0) != 0x80) return 0;
        value = (value << 6) | (b & 0x3F);
    }
    if (value < min || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) return 0;
    *cp = value;
    return len;
}

}  // namespace

CleanedText clean_pasted_text(std::string_view utf8, size_t max_chars) {
    CleanedText out;
    std::string text;
    text.reserve(utf8.size());
    size_t chars = 0, pos = 0;
    while (pos < utf8.size()) {
        uint32_t cp = 0;
        const size_t len = decode(utf8, pos, &cp);
        if (len == 0) {
            out.error_key = "paste_invalid";
            return out;
        }
        if (++chars > max_chars) {
            out.error_key = "paste_too_large";
            return out;
        }
        const size_t at = pos;
        pos += len;
        if (cp == '\r') {
            if (pos < utf8.size() && utf8[pos] == '\n') ++pos;  // "\r\n" is one break
            text.push_back('\n');
        } else if (cp == 0x2028 || cp == 0x2029) {
            text.push_back('\n');
        } else if (cp == '\n' || cp == '\t') {
            text.push_back(static_cast<char>(cp));
        } else if (cp < 0x20 || (cp >= 0x7F && cp <= 0x9F)) {
            // control character: dropped
        } else {
            text.append(utf8.substr(at, len));
        }
    }
    out.ok = true;
    out.text = std::move(text);
    return out;
}

std::string single_line(std::string_view cleaned) {
    std::string out;
    out.reserve(cleaned.size());
    bool in_break = false;
    for (char c : cleaned) {
        if (c == '\n' || c == '\r' || c == '\t') {
            if (!in_break) out.push_back(' ');
            in_break = true;
        } else {
            out.push_back(c);
            in_break = false;
        }
    }
    return out;
}

}  // namespace pbnotes
