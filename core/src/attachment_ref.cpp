#include "pbnotes/attachment_ref.h"

#include <algorithm>
#include <cctype>

namespace pbnotes {

namespace {

constexpr size_t kMaxPathBytes = 1024;
constexpr size_t kMaxNameChars = 255;
constexpr size_t kMaxTokenNameChars = 100;

bool is_control(unsigned char c) { return c < 0x20 || c == 0x7f; }

// "scheme:" at the start: a letter, then letters, digits, "+", "." or "-", then a colon.
bool starts_with_scheme(std::string_view s) {
    if (s.empty() || !std::isalpha(static_cast<unsigned char>(s[0]))) return false;
    for (size_t i = 1; i < s.size(); ++i) {
        const char c = s[i];
        if (c == ':') return true;
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '+' || c == '.' || c == '-')) return false;
    }
    return false;
}

std::string_view last_segment(std::string_view path) {
    const size_t slash = path.rfind('/');
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string lower(std::string_view s) {
    std::string out(s);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// Truncates UTF-8 text to at most `max_chars` code points (never inside a sequence).
std::string truncate_chars(const std::string& s, size_t max_chars) {
    size_t chars = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) {
            if (chars == max_chars) return s.substr(0, i);
            ++chars;
        }
    }
    return s;
}

}  // namespace

std::optional<std::string> validate_attachment_path(std::string_view path) {
    if (path.empty() || path.size() > kMaxPathBytes) return std::nullopt;
    for (unsigned char c : path)
        if (is_control(c) || c == '\\') return std::nullopt;
    if (path.front() == '/') return std::nullopt;
    if (starts_with_scheme(path)) return std::nullopt;
    if (last_segment(path).empty()) return std::nullopt;
    return std::string(path);
}

std::optional<std::string> decode_attachment_target(std::string_view target) {
    if (target.empty()) return std::nullopt;
    std::string out;
    out.reserve(target.size());
    size_t i = 0;
    while (i < target.size()) {
        const char c = target[i];
        if (c == '%') {
            const int hi = i + 1 < target.size() ? hex_value(target[i + 1]) : -1;
            const int lo = i + 2 < target.size() ? hex_value(target[i + 2]) : -1;
            if (hi < 0 || lo < 0) return std::nullopt;  // a bad or truncated escape
            const char decoded = static_cast<char>(hi * 16 + lo);
            if (decoded == '/') return std::nullopt;  // a segment never contains a slash
            out.push_back(decoded);
            i += 3;
        } else {
            out.push_back(c);
            ++i;
        }
    }
    return validate_attachment_path(out);
}

std::string encode_attachment_path(std::string_view path) {
    static const char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(path.size() + 8);
    for (unsigned char c : path) {
        if (std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~' || c == '/') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[c >> 4]);
            out.push_back(kHex[c & 0xF]);
        }
    }
    return out;
}

std::string safe_extension(std::string_view file_name) {
    const std::string_view name = last_segment(file_name);
    const size_t dot = name.rfind('.');
    if (dot == std::string_view::npos || dot == 0 || dot + 1 >= name.size()) return std::string();
    const std::string_view ext = name.substr(dot + 1);
    if (ext.size() > 8) return std::string();
    for (unsigned char c : ext)
        if (c >= 0x80 || !std::isalnum(c)) return std::string();
    return lower(ext);
}

AttachmentKind attachment_kind_for(std::string_view file_name) {
    const std::string ext = safe_extension(file_name);
    for (const char* image : {"jpg", "jpeg", "png", "gif", "bmp", "webp"})
        if (ext == image) return AttachmentKind::Image;
    return AttachmentKind::File;
}

bool is_viewable_extension(std::string_view extension) {
    const std::string ext = lower(extension);
    for (const char* v : {"pdf", "epub", "fb2", "txt", "rtf", "doc", "docx", "html", "htm", "djvu", "mobi", "cbz", "cbr"})
        if (ext == v) return true;
    return false;
}

std::string attachment_file_name(std::string_view path) {
    std::string name;
    for (unsigned char c : last_segment(path))
        if (!is_control(c)) name.push_back(static_cast<char>(c));
    return truncate_chars(name, kMaxNameChars);
}

// ---- tokens ----------------------------------------------------------------------------------------

namespace {

struct TokenSpan {
    size_t begin, end;  // the whole "![name](pbatt:<id>)"
    int64_t id;
    size_t target_begin;  // start of "pbatt:<id>" (just after "](")
};

std::vector<TokenSpan> token_spans(std::string_view s) {
    std::vector<TokenSpan> out;
    static const std::string_view kMark = "](pbatt:";
    size_t pos = 0;
    while ((pos = s.find(kMark, pos)) != std::string_view::npos) {
        const size_t digits = pos + kMark.size();
        size_t e = digits;
        int64_t id = 0;
        while (e < s.size() && std::isdigit(static_cast<unsigned char>(s[e])) && e - digits < 15) id = id * 10 + (s[e++] - '0');
        const bool closed = e > digits && e < s.size() && s[e] == ')';
        // the label: back from "]" on the same line to "![" with no further "]" in between
        size_t j = pos;
        bool found = false;
        while (j > 0) {
            --j;
            if (s[j] == '\n' || s[j] == ']') break;
            if (s[j] == '[') {
                found = j > 0 && s[j - 1] == '!';
                if (found) --j;
                break;
            }
        }
        if (closed && found) out.push_back({j, e + 1, id, pos + 2});
        pos += kMark.size();
    }
    return out;
}

}  // namespace

std::string make_token(int64_t id, std::string_view name) {
    std::string clean;
    for (unsigned char c : name) {
        if (is_control(c)) clean.push_back(' ');
        else if (c == '[' || c == ']' || c == '\\') clean.push_back('_');
        else clean.push_back(static_cast<char>(c));
    }
    return "![" + truncate_chars(clean, kMaxTokenNameChars) + "](pbatt:" + std::to_string(id) + ")";
}

std::vector<int64_t> find_tokens(std::string_view content) {
    std::vector<int64_t> ids;
    for (const TokenSpan& t : token_spans(content)) ids.push_back(t.id);
    return ids;
}

std::string replace_token(std::string_view content, int64_t id, std::string_view remote_path) {
    std::string out(content);
    const std::vector<TokenSpan> spans = token_spans(content);
    const std::string encoded = encode_attachment_path(remote_path);
    for (auto it = spans.rbegin(); it != spans.rend(); ++it) {
        if (it->id != id) continue;
        // replace "pbatt:<id>" (between "](" and the final ")") by the path
        out.replace(it->target_begin, it->end - 1 - it->target_begin, encoded);
    }
    return out;
}

std::string strip_tokens(std::string_view content, const std::vector<int64_t>& ids) {
    std::string out(content);
    const std::vector<TokenSpan> spans = token_spans(content);
    for (auto it = spans.rbegin(); it != spans.rend(); ++it) {
        if (std::find(ids.begin(), ids.end(), it->id) == ids.end()) continue;
        size_t begin = it->begin, end = it->end;
        const bool line_start = begin == 0 || out[begin - 1] == '\n';
        size_t after = end;
        if (after < out.size() && out[after] == '\r') ++after;
        const bool line_end = end == out.size() || (after < out.size() && out[after] == '\n');
        if (line_start && line_end && after < out.size()) {
            end = after + 1;  // the whole line including its break
        } else if (line_start && end == out.size() && begin > 0) {
            --begin;  // the last line: the break before it goes instead
            if (begin > 0 && out[begin - 1] == '\r') --begin;
        }
        out.erase(begin, end - begin);
    }
    return out;
}

}  // namespace pbnotes
