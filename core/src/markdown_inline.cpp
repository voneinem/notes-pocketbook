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

bool has_link(const std::vector<Span>& spans) {
    for (const auto& sp : spans)
        if (!sp.url.empty()) return true;
    return false;
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

void parse_inline(const std::string& s, std::vector<Span>* out, int depth, bool links);

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
void add_link(std::vector<Span>* out, const std::vector<Span>& inner, const std::string& url) {
    const bool confirm = link_needs_confirmation(plain_of(inner), url);
    for (const auto& sp : inner) add_span(out, sp.kind, sp.text, url, confirm);
}

void parse_inline(const std::string& s, std::vector<Span>* out, int depth, bool links) {
    std::string literal;
    auto flush = [&] {
        add_span(out, Span::Plain, literal);
        literal.clear();
    };
    size_t i = 0;
    while (i < s.size()) {
        const char c = s[i];
        if (c == '`') {  // inline code: kept verbatim, including the backticks
            const size_t e = s.find('`', i + 1);
            if (e != std::string::npos) {
                literal += s.substr(i, e - i + 1);
                i = e + 1;
                continue;
            }
        } else if (c == '\\' && i + 1 < s.size() && std::string("*_[]\\`").find(s[i + 1]) != std::string::npos) {
            literal.push_back(s[i + 1]);
            i += 2;
            continue;
        } else if (c == '!' && i + 1 < s.size() && s[i + 1] == '[') {
            std::string alt, target;
            const size_t end = read_link(s, i + 1, &alt, &target);
            if (end != std::string::npos && links && is_followable_url(target)) {  // external image: a link
                flush();
                std::vector<Span> inner;
                add_span(&inner, Span::Plain, alt.empty() ? target : alt);
                add_link(out, inner, target);
                i = end;
                continue;
            }
            literal += "![";  // anything else stays literal
            i += 2;
            continue;
        } else if (c == '[' && depth < 2) {
            std::string label, target;
            const size_t end = read_link(s, i, &label, &target);
            if (end != std::string::npos) {
                std::vector<Span> inner;
                parse_inline(label, &inner, depth + 1, false);
                flush();
                if (links && is_followable_url(target)) add_link(out, inner, target);
                else for (const auto& sp : inner) add_span(out, sp.kind, sp.text);
                i = end;
                continue;
            }
        } else if (c == '<' && links && starts_with_scheme(s, i + 1)) {  // <https://autolink>
            const size_t e = s.find('>', i + 1);
            if (e != std::string::npos) {
                const std::string url = s.substr(i + 1, e - i - 1);
                if (is_followable_url(url)) {
                    flush();
                    std::vector<Span> inner;
                    add_span(&inner, Span::Plain, url);
                    add_link(out, inner, url);
                    i = e + 1;
                    continue;
                }
            }
        } else if ((c == 'h' || c == 'H') && links && starts_with_scheme(s, i) && (i == 0 || !is_alnum(s[i - 1]))) {
            const size_t e = bare_address_end(s, i);
            const std::string url = s.substr(i, e - i);
            if (is_followable_url(url)) {
                flush();
                std::vector<Span> inner;
                add_span(&inner, Span::Plain, url);
                add_link(out, inner, url);
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
                    parse_inline(s.substr(after, close - after), &inner, depth + 1, links);
                    flush();
                    const Span::Kind kind = r == 1 ? Span::Italic : r == 2 ? Span::Bold : Span::BoldItalic;
                    if (has_link(inner)) {  // keep the links, the outer emphasis wins
                        for (const auto& sp : inner) add_span(out, kind, sp.text, sp.url, sp.link_confirm);
                    } else {
                        add_span(out, kind, plain_of(inner));
                    }
                    i = close + r;
                    continue;
                }
            }
            literal.append(run, c);
            i += run;
            continue;
        }
        literal.push_back(c);
        ++i;
    }
    flush();
}

}  // namespace

void add_span(std::vector<Span>* spans, Span::Kind kind, const std::string& text, const std::string& url,
              bool link_confirm) {
    if (text.empty()) return;
    if (!spans->empty() && spans->back().kind == kind && spans->back().url == url &&
        spans->back().link_confirm == link_confirm) {
        spans->back().text += text;
    } else {
        Span sp;
        sp.kind = kind;
        sp.text = text;
        sp.url = url;
        sp.link_confirm = link_confirm;
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
    auto flush = [&] {
        if (literal.empty()) return;
        Segment seg;
        seg.text = literal;
        out.push_back(std::move(seg));
        literal.clear();
    };
    size_t i = 0;
    while (i < s.size()) {
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
            for (const Span& sp : inline_spans(seg.text)) add_span(&spans, sp.kind, sp.text, sp.url, sp.link_confirm);
        }
    }
    return spans;
}

std::vector<Span> inline_spans(const std::string& text) {
    std::vector<Span> spans;
    parse_inline(text, &spans, 0, true);
    return spans;
}

std::vector<Span> literal_spans(const std::string& text) {
    std::vector<Span> spans;
    add_span(&spans, Span::Plain, text);
    return spans;
}

}  // namespace md
}  // namespace pbnotes
