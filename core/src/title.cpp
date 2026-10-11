#include "pbnotes/title.h"

#include <ctime>

#include "pbnotes/utf8.h"

namespace pbnotes {

namespace {

bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v'; }

std::string_view trim(std::string_view s) {
    size_t b = 0, e = s.size();
    while (b < e && is_space(s[b])) ++b;
    while (e > b && is_space(s[e - 1])) --e;
    return s.substr(b, e - b);
}

// Finds the first non-empty line; returns its [begin,end) range and the position after it.
bool first_nonempty_line(std::string_view text, size_t* begin, size_t* end, size_t* next) {
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        size_t line_end = nl == std::string_view::npos ? text.size() : nl;
        std::string_view line = text.substr(pos, line_end - pos);
        if (!trim(line).empty()) {
            *begin = pos;
            *end = line_end;
            *next = nl == std::string_view::npos ? text.size() : nl + 1;
            return true;
        }
        if (nl == std::string_view::npos) break;
        pos = nl + 1;
    }
    return false;
}

}  // namespace

std::string derive_title(std::string_view text) {
    size_t b, e, n;
    if (!first_nonempty_line(text, &b, &e, &n)) return std::string();
    return std::string(trim(text.substr(b, e - b)));
}

std::string strip_heading_marker(std::string_view line) {
    const std::string_view t = trim(line);
    size_t hashes = 0;
    while (hashes < t.size() && t[hashes] == '#') ++hashes;
    if (hashes >= 1 && hashes <= 6 && hashes < t.size() && (t[hashes] == ' ' || t[hashes] == '\t'))
        return std::string(trim(t.substr(hashes)));
    return std::string(t);
}

std::string legacy_title(std::string_view stored) {
    std::string t = strip_heading_marker(stored);
    for (char& c : t)
        if (static_cast<unsigned char>(c) < 0x20 || c == 0x7F) c = ' ';
    // cut after 255 code points, never inside a multi-byte sequence
    size_t chars = 0, cut = t.size();
    for (size_t i = 0; i < t.size(); ++i) {
        if ((static_cast<unsigned char>(t[i]) & 0xC0) == 0x80) continue;
        if (chars == 255) { cut = i; break; }
        ++chars;
    }
    t.resize(cut);
    t = std::string(trim(t));
    return t.empty() ? std::string("Untitled") : t;
}

std::string make_excerpt(std::string_view text, std::string_view title, std::size_t max_chars) {
    size_t b, e, n;
    if (!first_nonempty_line(text, &b, &e, &n)) return std::string();
    const bool repeats_title =
        !title.empty() && strip_heading_marker(text.substr(b, e - b)) == strip_heading_marker(title);
    std::string out;
    size_t chars = 0;  // code points
    bool pending_space = false;
    size_t i = repeats_title ? n : 0;
    while (i < text.size()) {
        const char c = text[i];
        if (is_space(c) || c == '\n') {
            pending_space = !out.empty();
            ++i;
            continue;
        }
        if (chars >= max_chars) break;
        if (pending_space) {
            out.push_back(' ');
            ++chars;
            pending_space = false;
            if (chars >= max_chars) break;
        }
        // one display unit at a time: an emoji sequence is never cut in the middle, so the excerpt may
        // exceed `max_chars` by the length of the last unit
        const size_t end = utf8_cluster_end(text, i);
        for (size_t k = i; k < end; ++k)
            if ((static_cast<unsigned char>(text[k]) & 0xC0) != 0x80) ++chars;
        out.append(text.substr(i, end - i));
        i = end;
    }
    return out;
}

namespace {

uint32_t to_lower(uint32_t c) {
    if (c < 0x80) return (c >= 'A' && c <= 'Z') ? c + 32 : c;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 32;
    if (c >= 0x100 && c <= 0x137) return (c % 2 == 0) ? c + 1 : c;
    if (c >= 0x139 && c <= 0x148) return (c % 2 == 1) ? c + 1 : c;
    if (c >= 0x14A && c <= 0x177) return (c % 2 == 0) ? c + 1 : c;
    if (c == 0x178) return 0xFF;
    if (c >= 0x179 && c <= 0x17E) return (c % 2 == 1) ? c + 1 : c;
    if (c == 0x386) return 0x3AC;
    if (c >= 0x388 && c <= 0x38A) return c + 37;
    if (c == 0x38C) return 0x3CC;
    if (c == 0x38E || c == 0x38F) return c + 63;
    if ((c >= 0x391 && c <= 0x3A1) || (c >= 0x3A3 && c <= 0x3AB)) return c + 32;
    if (c == 0x3C2) return 0x3C3;  // final sigma folds to sigma
    if (c >= 0x410 && c <= 0x42F) return c + 32;
    if (c >= 0x400 && c <= 0x40F) return c + 80;
    return c;
}

void append_utf8(std::string* out, uint32_t cp) {
    if (cp < 0x80) out->push_back(static_cast<char>(cp));
    else if (cp < 0x800) {
        out->push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out->push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out->push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out->push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

}  // namespace

std::string fold_case(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        const unsigned char b = static_cast<unsigned char>(s[i]);
        size_t n = b < 0x80 ? 1 : (b >> 5) == 0x6 ? 2 : (b >> 4) == 0xE ? 3 : (b >> 3) == 0x1E ? 4 : 0;
        bool valid = n > 0 && i + n <= s.size();
        uint32_t cp = n == 1 ? b : n == 2 ? (b & 0x1F) : n == 3 ? (b & 0x0F) : (b & 0x07);
        for (size_t k = 1; valid && k < n; ++k) {
            const unsigned char c = static_cast<unsigned char>(s[i + k]);
            if ((c & 0xC0) != 0x80) valid = false;
            else cp = (cp << 6) | (c & 0x3F);
        }
        if (!valid) {  // not UTF-8: keep the byte
            out.push_back(s[i]);
            ++i;
            continue;
        }
        append_utf8(&out, to_lower(cp));
        i += n;
    }
    return out;
}

std::string conflict_suffix(int64_t epoch_seconds, int number) {
    std::time_t t = static_cast<std::time_t>(epoch_seconds);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char date[16];
    std::strftime(date, sizeof date, "%Y-%m-%d", &tm);
    std::string s = std::string(" (conflict ") + date;
    if (number > 1) s += " #" + std::to_string(number);
    return s + ")";
}

}  // namespace pbnotes
