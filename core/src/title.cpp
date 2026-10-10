#include "pbnotes/title.h"

#include <ctime>

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

std::string make_excerpt(std::string_view text, std::size_t max_chars) {
    size_t b, e, n;
    if (!first_nonempty_line(text, &b, &e, &n)) return std::string();
    std::string out;
    size_t chars = 0;
    bool pending_space = false;
    for (size_t i = n; i < text.size(); ++i) {
        const char c = text[i];
        if (is_space(c) || c == '\n') {
            pending_space = !out.empty();
            continue;
        }
        const bool lead = (static_cast<unsigned char>(c) & 0xC0) != 0x80;  // start of a code point
        if (lead) {
            if (chars == max_chars) break;
            if (pending_space) {
                if (chars + 1 > max_chars) break;
                out.push_back(' ');
                ++chars;
                pending_space = false;
                if (chars == max_chars) break;
            }
            ++chars;
        }
        out.push_back(c);
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

std::string with_title_suffix(std::string_view content, const std::string& suffix) {
    size_t b, e, n;
    if (!first_nonempty_line(content, &b, &e, &n)) return std::string(content);
    size_t end = e;                       // end of the line, before the line break
    const bool cr = end > b && content[end - 1] == '\r';
    if (cr) --end;
    while (end > b && (content[end - 1] == ' ' || content[end - 1] == '\t')) --end;
    std::string out(content.substr(0, end));
    out += suffix;
    if (cr) out += '\r';
    out.append(content.substr(e));
    return out;
}

}  // namespace pbnotes
