#include "pbnotes/markdown.h"

#include <cctype>

namespace pbnotes {

namespace {

bool is_space(char c) { return c == ' ' || c == '\t'; }
bool is_alnum(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0; }

void add_span(std::vector<Span>* spans, Span::Kind kind, const std::string& text) {
    if (text.empty()) return;
    if (!spans->empty() && spans->back().kind == kind) spans->back().text += text;
    else spans->push_back({kind, text});
}

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

void parse_inline(const std::string& s, std::vector<Span>* out, int depth = 0);

// Tries to read a link "[label](url)" at s[i]; returns the index after it or npos.
size_t read_link(const std::string& s, size_t i, std::string* label) {
    int nest = 0;
    size_t j = i;
    for (; j < s.size(); ++j) {
        if (s[j] == '[') ++nest;
        else if (s[j] == ']' && --nest == 0) break;
    }
    if (j >= s.size() || j + 1 >= s.size() || s[j + 1] != '(') return std::string::npos;
    const size_t close = s.find(')', j + 2);
    if (close == std::string::npos) return std::string::npos;
    *label = s.substr(i + 1, j - i - 1);
    return close + 1;
}

void parse_inline(const std::string& s, std::vector<Span>* out, int depth) {
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
        } else if (c == '!' && i + 1 < s.size() && s[i + 1] == '[') {  // images stay literal
            literal += "![";
            i += 2;
            continue;
        } else if (c == '[' && depth < 2) {
            std::string label;
            const size_t end = read_link(s, i, &label);
            if (end != std::string::npos) {
                std::vector<Span> inner;
                parse_inline(label, &inner, depth + 1);
                flush();
                for (const auto& sp : inner) add_span(out, sp.kind, sp.text);
                i = end;
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
                    parse_inline(s.substr(after, close - after), &inner, depth + 1);
                    flush();
                    add_span(out, r == 1 ? Span::Italic : r == 2 ? Span::Bold : Span::BoldItalic, plain_of(inner));
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

std::vector<Span> inline_spans(const std::string& text) {
    std::vector<Span> spans;
    parse_inline(text, &spans);
    return spans;
}

std::vector<Span> literal_spans(const std::string& text) {
    std::vector<Span> spans;
    add_span(&spans, Span::Plain, text);
    return spans;
}

}  // namespace

std::vector<Block> parse_markdown(std::string_view text) {
    std::vector<Block> blocks;
    bool in_fence = false;
    int line_index = 0;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        std::string line(text.substr(pos, (nl == std::string_view::npos ? text.size() : nl) - pos));
        pos = nl == std::string_view::npos ? text.size() : nl + 1;
        while (!line.empty() && line.back() == '\r') line.pop_back();

        Block b;
        b.line_index = line_index++;

        size_t indent_cols = 0, first = 0;
        while (first < line.size() && is_space(line[first])) indent_cols += line[first++] == '\t' ? 4 : 1;
        const std::string rest = line.substr(first);

        if (rest.empty()) {
            b.kind = BlockKind::Blank;
            blocks.push_back(std::move(b));
            continue;
        }
        if (rest.compare(0, 3, "```") == 0) {  // code fence markers and their contents are plain text
            in_fence = !in_fence;
            b.spans = literal_spans(rest);
            blocks.push_back(std::move(b));
            continue;
        }
        if (in_fence) {
            b.spans = literal_spans(line);
            blocks.push_back(std::move(b));
            continue;
        }

        // heading
        size_t hashes = 0;
        while (hashes < rest.size() && rest[hashes] == '#') ++hashes;
        if (hashes >= 1 && hashes <= 6 && hashes < rest.size() && rest[hashes] == ' ' && indent_cols < 4) {
            b.kind = BlockKind::Heading;
            b.level = static_cast<int>(hashes);
            b.spans = inline_spans(rest.substr(hashes + 1));
            blocks.push_back(std::move(b));
            continue;
        }

        std::string body;
        bool is_list = false;
        if (rest.size() >= 2 && (rest[0] == '-' || rest[0] == '*' || rest[0] == '+') && rest[1] == ' ') {
            is_list = true;
            body = rest.substr(2);
        } else {
            size_t d = 0;
            while (d < rest.size() && std::isdigit(static_cast<unsigned char>(rest[d]))) ++d;
            if (d > 0 && d <= 9 && d + 1 < rest.size() && (rest[d] == '.' || rest[d] == ')') && rest[d + 1] == ' ') {
                is_list = true;
                b.ordered = true;
                b.number = std::stoi(rest.substr(0, d));
                body = rest.substr(d + 2);
            }
        }
        if (is_list) {
            b.kind = BlockKind::ListItem;
            b.depth = static_cast<int>(indent_cols / 2);
            if (body.size() >= 3 && body[0] == '[' && body[2] == ']' &&
                (body[1] == ' ' || body[1] == 'x' || body[1] == 'X') && (body.size() == 3 || body[3] == ' ')) {
                b.is_task = true;
                b.checked = body[1] != ' ';
                body = body.size() > 4 ? body.substr(4) : std::string();
            }
            b.spans = inline_spans(body);
            blocks.push_back(std::move(b));
            continue;
        }

        b.kind = BlockKind::Paragraph;
        b.spans = inline_spans(rest);
        blocks.push_back(std::move(b));
    }
    return blocks;
}

std::string toggle_task_item(std::string_view text, int line_index) {
    std::string out(text);
    if (line_index < 0) return out;

    // The parser decides what is a task item (it knows about code fences).
    bool is_task = false;
    for (const Block& b : parse_markdown(text))
        if (b.line_index == line_index && b.is_task) is_task = true;
    if (!is_task) return out;

    // Locate the start of that line.
    size_t pos = 0;
    for (int i = 0; i < line_index; ++i) {
        const size_t nl = out.find('\n', pos);
        if (nl == std::string::npos) return std::string(text);
        pos = nl + 1;
    }
    // indent, list marker, one space, then "[?]"
    while (pos < out.size() && is_space(out[pos])) ++pos;
    if (pos < out.size() && (out[pos] == '-' || out[pos] == '*' || out[pos] == '+')) {
        ++pos;
    } else {
        while (pos < out.size() && std::isdigit(static_cast<unsigned char>(out[pos]))) ++pos;
        ++pos;  // '.' or ')'
    }
    if (pos < out.size() && out[pos] == ' ') ++pos;
    if (pos + 2 >= out.size() || out[pos] != '[' || out[pos + 2] != ']') return std::string(text);
    out[pos + 1] = (out[pos + 1] == ' ') ? 'x' : ' ';
    return out;
}

}  // namespace pbnotes
