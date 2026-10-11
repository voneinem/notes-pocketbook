#include <algorithm>
#include <cctype>

#include "markdown_internal.h"
#include "pbnotes/attachment_ref.h"
#include "pbnotes/links.h"

namespace pbnotes {
namespace md {

bool is_space(char c) { return c == ' ' || c == '\t'; }

namespace {

bool is_alnum(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0; }

std::string plain_of(const std::vector<Span>& spans) {
    std::string s;
    for (const auto& sp : spans) s += sp.text;
    return s;
}

// Length of the run of `c` starting at s[i].
size_t run_length(const std::string& s, size_t i, char c) {
    size_t n = 0;
    while (i + n < s.size() && s[i + n] == c) ++n;
    return n;
}

// Finds the closing delimiter for an opener of `r` x `c` whose content starts at `from`.
// Returns npos if there is none.
size_t find_closer(const std::string& s, size_t from, char c, size_t r) {
    for (size_t j = from; j < s.size(); ++j) {
        if (s[j] == '`') {  // skip code spans
            size_t e = s.find('`', j + 1);
            if (e == std::string::npos) return std::string::npos;
            j = e;
            continue;
        }
        if (s[j] != c || (j > from && s[j - 1] == c)) continue;  // only the start of a run
        const size_t len = run_length(s, j, c);
        if (len != r) { j += len - 1; continue; }
        if (j == from || is_space(s[j - 1])) continue;                     // closer must follow non-space
        if (c == '_' && j + len < s.size() && is_alnum(s[j + len])) continue;  // not inside a word
        return j;
    }
    return std::string::npos;
}

void parse_inline(const std::string& s, std::vector<Span>* out, int depth, bool links, int base);

// "[label](target)" or "[label](target "title")" at s[i] (s[i] is '[', or the '[' after a '!').
// Returns the index after the closing ')' or npos; fills the label and the target.
size_t read_link(const std::string& s, size_t i, std::string* label, std::string* target) {
    int nest = 0;
    size_t j = i;
    for (; j < s.size(); ++j) {
        if (s[j] == '\\') { ++j; continue; }
        if (s[j] == '[') ++nest;
        else if (s[j] == ']' && --nest == 0) break;
    }
    if (j >= s.size() || j + 1 >= s.size() || s[j + 1] != '(') return std::string::npos;
    size_t k = j + 2;
    while (k < s.size() && is_space(s[k])) ++k;
    std::string t;
    if (k < s.size() && s[k] == '<') {  // <target>
        const size_t e = s.find('>', k + 1);
        if (e == std::string::npos) return std::string::npos;
        t = s.substr(k + 1, e - k - 1);
        k = e + 1;
    } else {  // up to a blank or an unbalanced ')', parentheses inside must balance
        int parens = 0;
        const size_t start = k;
        while (k < s.size() && !std::isspace(static_cast<unsigned char>(s[k]))) {
            if (s[k] == '(') ++parens;
            else if (s[k] == ')') {
                if (parens == 0) break;
                --parens;
            }
            ++k;
        }
        t = s.substr(start, k - start);
    }
    while (k < s.size() && is_space(s[k])) ++k;
    if (k < s.size() && (s[k] == '"' || s[k] == '\'')) {  // optional title, ignored
        const size_t e = s.find(s[k], k + 1);
        if (e == std::string::npos) return std::string::npos;
        k = e + 1;
        while (k < s.size() && is_space(s[k])) ++k;
    }
    if (k >= s.size() || s[k] != ')') return std::string::npos;
    *label = s.substr(i + 1, j - i - 1);
    *target = t;
    return k + 1;
}

bool starts_with_scheme(const std::string& s, size_t i) {
    auto at = [&](const char* p) {
        for (size_t n = 0; p[n]; ++n) {
            if (i + n >= s.size()) return false;
            const char c = s[i + n];
            if (std::tolower(static_cast<unsigned char>(c)) != p[n]) return false;
        }
        return true;
    };
    return at("http://") || at("https://");
}

// End (exclusive) of a bare address starting at s[i]: up to a blank or angle bracket, without
// trailing sentence punctuation or an unbalanced closing parenthesis.
size_t bare_address_end(const std::string& s, size_t i) {
    size_t e = i;
    while (e < s.size() && !std::isspace(static_cast<unsigned char>(s[e])) && s[e] != '<' && s[e] != '>') ++e;
    while (e > i) {
        const char c = s[e - 1];
        if (c == '.' || c == ',' || c == ';' || c == ':' || c == '!' || c == '?' || c == '\'' || c == '"') {
            --e;
            continue;
        }
        if (c == ')') {
            int open = 0, close = 0;
            for (size_t k = i; k < e; ++k) {
                if (s[k] == '(') ++open;
                else if (s[k] == ')') ++close;
            }
            if (close > open) { --e; continue; }
        }
        break;
    }
    return e;
}

// Adds `inner` (already parsed label spans) as a link to `url`; every span keeps its emphasis.
// `el_begin`/`el_end`: the source extent of the whole element, which every span of it gets (an outer element wins
// over the extents of what is inside it).
void add_link(std::vector<Span>* out, const std::vector<Span>& inner, const std::string& url, int el_begin, int el_end) {
    const bool confirm = link_needs_confirmation(plain_of(inner), url);
    for (const auto& sp : inner) add_span(out, sp.kind, sp.text, url, confirm, &sp.src, el_begin, el_end, sp.strike, sp.underline);
}

// The source index of every byte of `n` bytes that start at `from` (text copied verbatim from the source).
std::vector<int> run_of(int from, size_t n) {
    std::vector<int> v(n);
    for (size_t k = 0; k < n; ++k) v[k] = from + static_cast<int>(k);
    return v;
}

// A plain span of text copied verbatim from the source at `from`, as `inner` for a link or an emphasis.
std::vector<Span> verbatim(const std::string& text, int from, int el_begin, int el_end) {
    std::vector<Span> v;
    const std::vector<int> src = run_of(from, text.size());
    add_span(&v, Span::Plain, text, std::string(), false, &src, el_begin, el_end);
    return v;
}

// `base` is the offset of `s` inside its source line; every recorded source index is line-relative.
void parse_inline(const std::string& s, std::vector<Span>* out, int depth, bool links, int base) {
    std::string literal;
    std::vector<int> literal_src;
    int literal_begin = -1, literal_end = -1;
    // appends one character that came from s[at .. at+consumed)
    auto add = [&](char ch, size_t at, size_t consumed) {
        if (literal.empty()) literal_begin = base + static_cast<int>(at);
        literal.push_back(ch);
        literal_src.push_back(base + static_cast<int>(at));
        literal_end = base + static_cast<int>(at + consumed);
    };
    auto flush = [&] {
        add_span(out, Span::Plain, literal, std::string(), false, &literal_src, literal_begin, literal_end);
        literal.clear();
        literal_src.clear();
    };
    size_t i = 0;
    while (i < s.size()) {
        const char c = s[i];
        if (c == '`') {  // inline code: kept verbatim, including the backticks
            const size_t e = s.find('`', i + 1);
            if (e != std::string::npos) {
                for (size_t k = i; k <= e; ++k) add(s[k], k, 1);
                i = e + 1;
                continue;
            }
        } else if (c == '\\' && i + 1 < s.size() && std::string("*_[]\\`").find(s[i + 1]) != std::string::npos) {
            add(s[i + 1], i, 2);  // points at the backslash
            i += 2;
            continue;
        } else if (c == '!' && i + 1 < s.size() && s[i + 1] == '[') {
            std::string alt, target;
            const size_t end = read_link(s, i + 1, &alt, &target);
            if (end != std::string::npos && links && is_followable_url(target)) {  // external image: a link
                flush();
                const int eb = base + static_cast<int>(i), ee = base + static_cast<int>(end);
                std::vector<Span> inner;
                if (alt.empty()) add_span(&inner, Span::Plain, target);  // shown text that is not in the line: no offsets
                else inner = verbatim(alt, base + static_cast<int>(i) + 2, eb, ee);
                add_link(out, inner, target, eb, ee);
                i = end;
                continue;
            }
            add('!', i, 1);  // anything else stays literal
            add('[', i + 1, 1);
            i += 2;
            continue;
        } else if (c == '[' && depth < 2) {
            std::string label, target;
            const size_t end = read_link(s, i, &label, &target);
            if (end != std::string::npos) {
                std::vector<Span> inner;
                parse_inline(label, &inner, depth + 1, false, base + static_cast<int>(i) + 1);
                flush();
                const int eb = base + static_cast<int>(i), ee = base + static_cast<int>(end);
                if (links && is_followable_url(target)) add_link(out, inner, target, eb, ee);
                else for (const auto& sp : inner) add_span(out, sp.kind, sp.text, std::string(), false, &sp.src, eb, ee, sp.strike, sp.underline);
                i = end;
                continue;
            }
        } else if (c == '<' && depth < 2 && s.compare(i, 3, "<u>") == 0) {  // <u>underline</u> (spec 006)
            const size_t close = s.find("</u>", i + 3);
            bool blank = true;
            for (size_t k = i + 3; close != std::string::npos && k < close; ++k) blank = blank && is_space(s[k]);
            if (close != std::string::npos && !blank) {
                std::vector<Span> inner;
                parse_inline(s.substr(i + 3, close - i - 3), &inner, depth + 1, links, base + static_cast<int>(i) + 3);
                flush();
                const int eb = base + static_cast<int>(i), ee = base + static_cast<int>(close) + 4;
                for (const auto& sp : inner)
                    add_span(out, sp.kind, sp.text, sp.url, sp.link_confirm, &sp.src, eb, ee, sp.strike, true);
                i = close + 4;
                continue;
            }
        } else if (c == '~' && depth < 2) {  // ~~strikethrough~~ (spec 006): exactly two tildes on both sides
            const size_t run = run_length(s, i, c);
            const size_t after = i + run;
            if (run == 2 && after < s.size() && !is_space(s[after])) {
                const size_t close = find_closer(s, after, c, 2);
                if (close != std::string::npos && close > after) {
                    std::vector<Span> inner;
                    parse_inline(s.substr(after, close - after), &inner, depth + 1, links, base + static_cast<int>(after));
                    flush();
                    const int eb = base + static_cast<int>(i), ee = base + static_cast<int>(close) + 2;
                    for (const auto& sp : inner)
                        add_span(out, sp.kind, sp.text, sp.url, sp.link_confirm, &sp.src, eb, ee, true, sp.underline);
                    i = close + 2;
                    continue;
                }
            }
            for (size_t k = 0; k < run; ++k) add(c, i + k, 1);
            i += run;
            continue;
        } else if (c == '<' && links && starts_with_scheme(s, i + 1)) {  // <https://autolink>
            const size_t e = s.find('>', i + 1);
            if (e != std::string::npos) {
                const std::string url = s.substr(i + 1, e - i - 1);
                if (is_followable_url(url)) {
                    flush();
                    const int eb = base + static_cast<int>(i), ee = base + static_cast<int>(e) + 1;
                    add_link(out, verbatim(url, base + static_cast<int>(i) + 1, eb, ee), url, eb, ee);
                    i = e + 1;
                    continue;
                }
            }
        } else if ((c == 'h' || c == 'H') && links && starts_with_scheme(s, i) && (i == 0 || !is_alnum(s[i - 1]))) {
            const size_t e = bare_address_end(s, i);
            const std::string url = s.substr(i, e - i);
            if (is_followable_url(url)) {
                flush();
                const int eb = base + static_cast<int>(i), ee = base + static_cast<int>(e);
                add_link(out, verbatim(url, eb, eb, ee), url, eb, ee);
                i = e;
                continue;
            }
        } else if ((c == '*' || c == '_') && depth < 2) {
            const size_t run = run_length(s, i, c);
            const size_t r = run > 3 ? 3 : run;
            const size_t after = i + run;
            const bool opener = after < s.size() && !is_space(s[after]) && run <= 3 &&
                                !(c == '_' && i > 0 && is_alnum(s[i - 1]));
            if (opener) {
                const size_t close = find_closer(s, after, c, r);
                if (close != std::string::npos && close > after) {
                    std::vector<Span> inner;
                    parse_inline(s.substr(after, close - after), &inner, depth + 1, links, base + static_cast<int>(after));
                    flush();
                    const Span::Kind kind = r == 1 ? Span::Italic : r == 2 ? Span::Bold : Span::BoldItalic;
                    const int eb = base + static_cast<int>(i), ee = base + static_cast<int>(close + r);
                    // the outer emphasis wins over the kinds inside; links and the strike / underline flags are kept
                    for (const auto& sp : inner)
                        add_span(out, kind, sp.text, sp.url, sp.link_confirm, &sp.src, eb, ee, sp.strike, sp.underline);
                    i = close + r;
                    continue;
                }
            }
            for (size_t k = 0; k < run; ++k) add(c, i + k, 1);
            i += run;
            continue;
        }
        add(c, i, 1);
        ++i;
    }
    flush();
}

}  // namespace

void add_span(std::vector<Span>* spans, Span::Kind kind, const std::string& text, const std::string& url,
              bool link_confirm, const std::vector<int>* src, int el_begin, int el_end, bool strike, bool underline) {
    if (text.empty()) return;
    if (!spans->empty() && spans->back().kind == kind && spans->back().url == url &&
        spans->back().link_confirm == link_confirm && spans->back().strike == strike &&
        spans->back().underline == underline) {
        Span& back = spans->back();
        back.text += text;
        if (src) back.src.insert(back.src.end(), src->begin(), src->end());
        if (el_begin >= 0) {
            back.el_begin = back.el_begin < 0 ? el_begin : std::min(back.el_begin, el_begin);
            back.el_end = std::max(back.el_end, el_end);
        }
    } else {
        Span sp;
        sp.kind = kind;
        sp.text = text;
        sp.url = url;
        sp.link_confirm = link_confirm;
        sp.strike = strike;
        sp.underline = underline;
        if (src) sp.src = *src;
        sp.el_begin = el_begin;
        sp.el_end = el_end;
        spans->push_back(std::move(sp));
    }
}

namespace {

// "pbatt:<digits>" -> id, else 0.
int64_t token_id_of(const std::string& target) {
    static const std::string kScheme = "pbatt:";
    if (target.compare(0, kScheme.size(), kScheme) != 0 || target.size() == kScheme.size() || target.size() > kScheme.size() + 15) return 0;
    int64_t id = 0;
    for (size_t i = kScheme.size(); i < target.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(target[i]))) return 0;
        id = id * 10 + (target[i] - '0');
    }
    return id;
}

// Tries to read an attachment reference whose "[" is at s[bracket]; `image` says a "!" precedes it.
size_t read_attachment_ref(const std::string& s, size_t bracket, bool image, AttachmentRef* ref) {
    std::string label, target;
    const size_t end = read_link(s, bracket, &label, &target);
    if (end == std::string::npos) return std::string::npos;
    AttachmentRef r;
    r.name = label;
    r.is_image_syntax = image;
    if (image && (r.token_id = token_id_of(target)) > 0) {
        *ref = r;
        return end;
    }
    auto path = decode_attachment_target(target);
    if (!path) return std::string::npos;
    // an image may point anywhere relative; a plain link only into the attachment folder
    if (!image && path->compare(0, 13, ".attachments.") != 0) return std::string::npos;
    r.path = *path;
    *ref = r;
    return end;
}

}  // namespace

std::vector<Segment> split_attachment_refs(const std::string& s) {
    std::vector<Segment> out;
    std::string literal;
    size_t literal_begin = 0;
    auto flush = [&] {
        if (literal.empty()) return;
        Segment seg;
        seg.begin = literal_begin;
        seg.text = literal;
        out.push_back(std::move(seg));
        literal.clear();
    };
    size_t i = 0;
    while (i < s.size()) {
        if (literal.empty()) literal_begin = i;
        const char c = s[i];
        if (c == '`') {  // inline code: copied through
            const size_t e = s.find('`', i + 1);
            if (e != std::string::npos) {
                literal += s.substr(i, e - i + 1);
                i = e + 1;
                continue;
            }
        } else if (c == '\\' && i + 1 < s.size()) {
            literal.append(s, i, 2);
            i += 2;
            continue;
        } else if (c == '!' && i + 1 < s.size() && s[i + 1] == '[') {
            AttachmentRef ref;
            const size_t end = read_attachment_ref(s, i + 1, true, &ref);
            if (end != std::string::npos) {
                flush();
                Segment seg;
                seg.begin = i;
                seg.is_ref = true;
                seg.ref = ref;
                seg.raw = s.substr(i, end - i);
                out.push_back(std::move(seg));
                i = end;
                continue;
            }
        } else if (c == '[' && !(i > 0 && s[i - 1] == '!')) {
            AttachmentRef ref;
            const size_t end = read_attachment_ref(s, i, false, &ref);
            if (end != std::string::npos) {
                flush();
                Segment seg;
                seg.begin = i;
                seg.is_ref = true;
                seg.ref = ref;
                seg.raw = s.substr(i, end - i);
                out.push_back(std::move(seg));
                i = end;
                continue;
            }
        }
        literal.push_back(c);
        ++i;
    }
    flush();
    return out;
}

std::vector<Span> cell_spans(const std::string& text) {
    std::vector<Span> spans;
    for (const Segment& seg : split_attachment_refs(text)) {
        if (seg.is_ref) {
            add_span(&spans, Span::Plain, seg.ref.name);
        } else {
            for (const Span& sp : inline_spans(seg.text))
                add_span(&spans, sp.kind, sp.text, sp.url, sp.link_confirm, nullptr, -1, -1, sp.strike, sp.underline);
        }
    }
    return spans;
}

std::vector<Span> inline_spans(const std::string& text, int base) {
    std::vector<Span> spans;
    parse_inline(text, &spans, 0, true, base);
    return spans;
}

std::vector<Span> literal_spans(const std::string& text, int base) {
    std::vector<Span> spans;
    const std::vector<int> src = run_of(base, text.size());
    add_span(&spans, Span::Plain, text, std::string(), false, &src, base, base + static_cast<int>(text.size()));
    return spans;
}

}  // namespace md
}  // namespace pbnotes
