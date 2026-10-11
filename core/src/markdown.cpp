#include "pbnotes/markdown.h"

#include <algorithm>
#include <cctype>

#include "markdown_internal.h"
#include "pbnotes/attachment_ref.h"

namespace pbnotes {

namespace {
bool is_space(char c) { return md::is_space(c); }
}  // namespace

namespace md {

// The source lines without their line ends (a final line break does not create an empty line).
std::vector<std::string> split_lines(std::string_view text) {
    std::vector<std::string> lines;
    size_t pos = 0;
    while (pos < text.size()) {
        const size_t nl = text.find('\n', pos);
        std::string line(text.substr(pos, (nl == std::string_view::npos ? text.size() : nl) - pos));
        pos = nl == std::string_view::npos ? text.size() : nl + 1;
        while (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(std::move(line));
    }
    return lines;
}

std::optional<FenceOpen> open_fence(const std::string& line) {
    size_t i = 0, cols = 0;
    while (i < line.size() && is_space(line[i])) cols += line[i++] == '\t' ? 4 : 1;
    if (cols > 3 || i >= line.size() || (line[i] != '`' && line[i] != '~')) return std::nullopt;
    const char ch = line[i];
    size_t n = 0;
    while (i + n < line.size() && line[i + n] == ch) ++n;
    if (n < 3) return std::nullopt;
    size_t a = i + n, b = line.size();
    while (a < b && (is_space(line[a]) || line[a] == '\r')) ++a;
    while (b > a && (is_space(line[b - 1]) || line[b - 1] == '\r')) --b;
    FenceOpen open;
    open.ch = ch;
    open.count = n;
    open.info = line.substr(a, b - a);
    if (ch == '`' && open.info.find('`') != std::string::npos) return std::nullopt;
    return open;
}

bool closes_fence(const std::string& line, const FenceOpen& open) {
    size_t i = 0, cols = 0;
    while (i < line.size() && is_space(line[i])) cols += line[i++] == '\t' ? 4 : 1;
    if (cols > 3) return false;
    size_t n = 0;
    while (i + n < line.size() && line[i + n] == open.ch) ++n;
    if (n < open.count) return false;
    for (size_t k = i + n; k < line.size(); ++k)
        if (!is_space(line[k]) && line[k] != '\r') return false;
    return true;
}

bool quote_prefix(const std::string& line, size_t* len) {
    size_t i = 0, cols = 0;
    while (i < line.size() && is_space(line[i])) cols += line[i++] == '\t' ? 4 : 1;
    if (cols > 3 || i >= line.size() || line[i] != '>') return false;
    ++i;
    if (i < line.size() && line[i] == ' ') ++i;
    *len = i;
    return true;
}

bool FenceTracker::line(const std::string& text) {
    if (open_) {
        if (!in_quote_) {
            if (closes_fence(text, *open_)) open_.reset();
            return true;
        }
        size_t q = 0;
        if (quote_prefix(text, &q)) {
            if (closes_fence(text.substr(q), *open_)) open_.reset();
            return true;
        }
        open_.reset();  // the quote ended, and the fence with it
    }
    size_t q = 0;
    const bool quoted = quote_prefix(text, &q);
    if (auto open = open_fence(quoted ? text.substr(q) : text)) {
        open_ = std::move(open);
        in_quote_ = quoted;
        return true;
    }
    return false;
}

}  // namespace md

namespace {

using md::split_lines;

std::string trimmed(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && is_space(s[a])) ++a;
    while (b > a && is_space(s[b - 1])) --b;
    return s.substr(a, b - a);
}

// A line longer than this is shown as plain text (no formatting, links, tables or attachments): the parser
// looks for closing delimiters from every opener, so a hostile line must not be able to make that quadratic.
constexpr size_t kMaxParsedLine = 16384;

std::string lower_trimmed(const std::string& s, size_t from = 0) {
    size_t a = from, b = s.size();
    while (a < b && (is_space(s[a]) || s[a] == '\r')) ++a;
    while (b > a && (is_space(s[b - 1]) || s[b - 1] == '\r')) --b;
    std::string out = s.substr(a, b - a);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// The kind of an info box header ("[!NOTE]" alone, any case, blanks around) or false.
bool info_header(const std::string& work, CalloutKind* kind) {
    const std::string t = lower_trimmed(work);
    if (t == "[!note]") *kind = CalloutKind::Note;
    else if (t == "[!tip]") *kind = CalloutKind::Tip;
    else if (t == "[!warning]") *kind = CalloutKind::Warning;
    else if (t == "[!important]") *kind = CalloutKind::Important;
    else if (t == "[!caution]") *kind = CalloutKind::Caution;
    else return false;
    return true;
}

size_t indent_of(const std::string& line, size_t* cols) {
    size_t i = 0;
    *cols = 0;
    while (i < line.size() && is_space(line[i])) *cols += line[i++] == '\t' ? 4 : 1;
    return i;
}

// "<details>" or "<details open>" alone on a line (any case, at most 3 spaces of indent, blanks around).
bool details_open_line(const std::string& line, bool* open) {
    size_t cols = 0;
    const size_t i = indent_of(line, &cols);
    if (cols > 3) return false;
    const std::string t = lower_trimmed(line, i);
    if (t == "<details>") *open = false;
    else if (t == "<details open>") *open = true;
    else return false;
    return true;
}

bool details_close_line(const std::string& line) {
    size_t cols = 0;
    const size_t i = indent_of(line, &cols);
    return cols <= 3 && lower_trimmed(line, i) == "</details>";
}

// "<summary>text</summary>" alone on a line; `*at` is where the text starts in the line.
bool summary_line(const std::string& line, std::string* text, int* at) {
    size_t cols = 0;
    const size_t i = indent_of(line, &cols);
    if (cols > 3) return false;
    size_t end = line.size();
    while (end > i && (is_space(line[end - 1]) || line[end - 1] == '\r')) --end;
    if (end - i < 19) return false;  // "<summary>" + "</summary>"
    const std::string open = lower_trimmed(line.substr(i, 9)), close = lower_trimmed(line.substr(end - 10, 10));
    if (open != "<summary>" || close != "</summary>") return false;
    *text = line.substr(i + 9, end - 10 - (i + 9));
    *at = static_cast<int>(i + 9);
    return true;
}

// Adds the block(s) of one line of text. `proto` carries the kind and its fields (heading level, list
// marker, task state); `body` is the text after the marker, which starts at byte `body_at` of the source
// line. An attachment reference ends the text segment: the first text keeps `proto`'s kind, an attachment
// block follows for every reference, later text becomes paragraphs; all with the line's index. Every block
// records its extent in the line (Block::src_begin/src_end) and every span where its bytes came from.
void push_text_blocks(std::vector<Block>* blocks, Block proto, const std::string& body, int body_at) {
    const int at = body_at;
    if (body.size() > kMaxParsedLine) {
        proto.spans = md::literal_spans(body, at);
        proto.src_begin = 0;
        proto.src_end = at + static_cast<int>(body.size());
        blocks->push_back(std::move(proto));
        return;
    }
    const std::vector<md::Segment> segments = md::split_attachment_refs(body);
    bool any_ref = false;
    for (const auto& seg : segments) any_ref = any_ref || seg.is_ref;
    if (!any_ref) {
        proto.spans = md::inline_spans(body, at);
        proto.src_begin = 0;
        proto.src_end = at + static_cast<int>(body.size());
        blocks->push_back(std::move(proto));
        return;
    }
    bool first = true;
    for (const md::Segment& seg : segments) {
        const int seg_at = at + static_cast<int>(seg.begin);
        if (seg.is_ref) {
            if (first && proto.is_task) {  // a task whose text starts with the reference: keep the box
                Block box = proto;
                box.src_begin = 0;
                box.src_end = at;
                blocks->push_back(std::move(box));
            }
            Block a;
            a.kind = BlockKind::Attachment;
            a.line_index = proto.line_index;
            a.attachment = seg.ref;
            a.src_begin = seg_at;
            a.src_end = seg_at + static_cast<int>(seg.raw.size());
            blocks->push_back(std::move(a));
        } else if (!trimmed(seg.text).empty()) {
            Block t;
            if (first) {
                t = proto;
            } else {
                t.kind = BlockKind::Paragraph;
                t.line_index = proto.line_index;
            }
            size_t lead = 0;
            while (lead < seg.text.size() && is_space(seg.text[lead])) ++lead;
            const std::string text = trimmed(seg.text);
            t.spans = md::inline_spans(text, seg_at + static_cast<int>(lead));
            t.src_begin = first ? 0 : seg_at + static_cast<int>(lead);
            t.src_end = seg_at + static_cast<int>(lead + text.size());
            blocks->push_back(std::move(t));
        }
        first = false;
    }
}

}  // namespace

std::vector<Block> parse_markdown(std::string_view text) {
    std::vector<Block> blocks;
    const std::vector<std::string> lines = split_lines(text);
    std::vector<QuoteKind> quote_of(lines.size(), QuoteKind::None);   // per source line
    std::vector<int> details_of(lines.size(), -1);
    std::vector<char> open_of(lines.size(), 0);

    std::optional<md::FenceOpen> fence;  // set while inside a fenced block
    bool fence_in_quote = false;         // the fence was opened on a quote line: it ends with the quote
    bool prev_quote_line = false;
    QuoteKind group_kind = QuoteKind::None;
    int section = -1;                    // the <details> section the lines belong to (its first line), or -1
    bool section_open = false;

    for (size_t index = 0; index < lines.size(); ++index) {
        const std::string& line = lines[index];

        Block b;
        b.line_index = static_cast<int>(index);

        // A fence opened outside a quote swallows every line, quote markers included.
        size_t qlen = 0;
        bool quote_line = !(fence && !fence_in_quote) && md::quote_prefix(line, &qlen);
        if (fence && fence_in_quote && !quote_line) {  // the quote ended: so does its fence
            if (!blocks.empty()) blocks.back().code_last = true;
            fence.reset();
        }
        const bool group_continues = quote_line && prev_quote_line;
        prev_quote_line = quote_line;
        const std::string work = quote_line ? line.substr(qlen) : line;
        const size_t base = quote_line ? qlen : 0;  // where `work` starts in the line
        CalloutKind callout = CalloutKind::Note;
        if (quote_line && !group_continues && !fence)
            group_kind = info_header(work, &callout) ? QuoteKind::Info : QuoteKind::Quote;
        if (quote_line) quote_of[index] = group_kind;
        if (section >= 0) {
            details_of[index] = section;
            open_of[index] = section_open;
        }

        if (fence) {  // a code line, or the closing fence; nothing inside is parsed
            b.src_begin = 0;
            b.src_end = static_cast<int>(line.size());
            if (md::closes_fence(work, *fence)) {
                b.kind = BlockKind::Fence;
                b.code_last = true;
                fence.reset();
            } else {
                b.kind = BlockKind::Code;
                b.spans = md::literal_spans(work, static_cast<int>(base));
            }
            blocks.push_back(std::move(b));
            continue;
        }
        if (auto open = md::open_fence(work)) {
            b.kind = BlockKind::Fence;
            b.code_first = true;
            b.fence_info = open->info;
            b.src_end = static_cast<int>(line.size());
            blocks.push_back(std::move(b));
            fence = std::move(open);
            fence_in_quote = quote_line;
            continue;
        }

        if (quote_line && !group_continues && group_kind == QuoteKind::Info) {  // the header of an info box
            b.kind = BlockKind::Callout;
            b.callout = callout;
            b.src_end = static_cast<int>(line.size());
            blocks.push_back(std::move(b));
            continue;
        }

        // <details> sections: only at top level (not in a quote, a fence or another section)
        if (!quote_line) {
            bool open = false;
            if (section < 0 && details_open_line(line, &open)) {
                section = static_cast<int>(index);
                section_open = open;
                std::string summary;
                int summary_at = 0;
                if (index + 1 < lines.size() && summary_line(lines[index + 1], &summary, &summary_at)) {
                    b.kind = BlockKind::Marker;  // the <details> line itself shows nothing
                    b.src_end = static_cast<int>(line.size());
                    blocks.push_back(std::move(b));
                    ++index;  // the summary line
                    Block s;
                    s.kind = BlockKind::Summary;
                    s.line_index = static_cast<int>(index);
                    s.spans = md::inline_spans(summary, summary_at);
                    s.src_begin = 0;
                    s.src_end = static_cast<int>(lines[index].size());
                    blocks.push_back(std::move(s));
                    details_of[index] = section;
                    open_of[index] = section_open;
                } else {  // no summary line: the <details> line is the summary, named "Details"
                    b.kind = BlockKind::Summary;
                    md::add_span(&b.spans, Span::Plain, "Details");
                    b.src_end = static_cast<int>(line.size());
                    blocks.push_back(std::move(b));
                    details_of[index] = section;
                    open_of[index] = section_open;
                }
                continue;
            }
            if (section >= 0 && details_close_line(line)) {
                details_of[index] = -1;
                open_of[index] = 0;
                section = -1;
                b.kind = BlockKind::Marker;
                b.src_end = static_cast<int>(line.size());
                blocks.push_back(std::move(b));
                continue;
            }
        }

        size_t indent_cols = 0, first = 0;
        while (first < work.size() && is_space(work[first])) indent_cols += work[first++] == '\t' ? 4 : 1;
        const std::string rest = work.substr(first);

        if (rest.empty()) {
            b.kind = BlockKind::Blank;
            blocks.push_back(std::move(b));
            continue;
        }

        // heading
        size_t hashes = 0;
        while (hashes < rest.size() && rest[hashes] == '#') ++hashes;
        if (hashes >= 1 && hashes <= 6 && hashes < rest.size() && rest[hashes] == ' ' && indent_cols < 4) {
            b.kind = BlockKind::Heading;
            b.level = static_cast<int>(hashes);
            push_text_blocks(&blocks, std::move(b), rest.substr(hashes + 1), static_cast<int>(base + first + hashes + 1));
            continue;
        }

        std::string body;
        size_t body_at = base + first;  // where `body` starts in the line
        bool is_list = false;
        if (rest.size() >= 2 && (rest[0] == '-' || rest[0] == '*' || rest[0] == '+') && rest[1] == ' ') {
            is_list = true;
            body = rest.substr(2);
            body_at += 2;
        } else {
            size_t d = 0;
            while (d < rest.size() && std::isdigit(static_cast<unsigned char>(rest[d]))) ++d;
            if (d > 0 && d <= 9 && d + 1 < rest.size() && (rest[d] == '.' || rest[d] == ')') && rest[d + 1] == ' ') {
                is_list = true;
                b.ordered = true;
                b.number = std::stoi(rest.substr(0, d));
                body = rest.substr(d + 2);
                body_at += d + 2;
            }
        }
        if (is_list) {
            b.kind = BlockKind::ListItem;
            b.depth = static_cast<int>(indent_cols / 2);
            if (body.size() >= 3 && body[0] == '[' && body[2] == ']' &&
                (body[1] == ' ' || body[1] == 'x' || body[1] == 'X') && (body.size() == 3 || body[3] == ' ')) {
                b.is_task = true;
                b.checked = body[1] != ' ';
                body_at += std::min<size_t>(4, body.size());
                body = body.size() > 4 ? body.substr(4) : std::string();
            }
            push_text_blocks(&blocks, std::move(b), body, static_cast<int>(body_at));
            continue;
        }

        if (!quote_line && indent_cols < 4 && rest[0] != '>' && rest.size() <= kMaxParsedLine) {
            const size_t used = md::read_table(lines, index, &b);
            if (used > 0) {
                blocks.push_back(std::move(b));
                index += used - 1;
                continue;
            }
        }

        b.kind = BlockKind::Paragraph;
        push_text_blocks(&blocks, std::move(b), rest, static_cast<int>(base + first));
    }
    if (fence && !blocks.empty()) blocks.back().code_last = true;  // an unclosed fence runs to the end of the note

    // Per-line facts onto the blocks, then the first / last block of every quote group.
    for (Block& b : blocks) {
        if (b.kind == BlockKind::Table) continue;
        const size_t line = static_cast<size_t>(b.line_index);
        if (line >= lines.size()) continue;
        b.quote = quote_of[line];
        if (b.kind != BlockKind::Marker) {
            b.details_id = details_of[line];
            b.details_open = open_of[line] != 0;
        }
    }
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (blocks[i].quote == QuoteKind::None) continue;
        if (i == 0 || blocks[i - 1].quote == QuoteKind::None) blocks[i].quote_first = true;
        if (i + 1 == blocks.size() || blocks[i + 1].quote == QuoteKind::None) blocks[i].quote_last = true;
    }
    return blocks;
}

std::vector<AttachmentRef> find_attachment_refs(std::string_view text) {
    std::vector<AttachmentRef> refs;
    md::FenceTracker fences;
    for (const std::string& line : split_lines(text)) {
        if (fences.line(line)) continue;
        if (line.size() > kMaxParsedLine) continue;
        for (const md::Segment& seg : md::split_attachment_refs(line))
            if (seg.is_ref) refs.push_back(seg.ref);
    }
    return refs;
}

std::string tokenize_attachment_refs(std::string_view text, const std::map<std::string, int64_t>& token_for_path) {
    if (token_for_path.empty()) return std::string(text);
    std::string out;
    md::FenceTracker fences;
    size_t pos = 0;
    while (pos < text.size()) {
        const size_t nl = text.find('\n', pos);
        const size_t end = nl == std::string_view::npos ? text.size() : nl;
        const std::string line(text.substr(pos, end - pos));
        const bool fence_line = fences.line(line);
        if (!fence_line && line.size() <= kMaxParsedLine) {
            for (const md::Segment& seg : md::split_attachment_refs(line)) {
                auto it = seg.is_ref ? token_for_path.find(seg.ref.path) : token_for_path.end();
                if (it != token_for_path.end()) {
                    const std::string name = seg.ref.name.empty() ? attachment_file_name(seg.ref.path) : seg.ref.name;
                    out += make_token(it->second, name);
                } else {
                    out += seg.is_ref ? seg.raw : seg.text;
                }
            }
            if (nl != std::string_view::npos) out += '\n';
            pos = end + (nl == std::string_view::npos ? 0 : 1);
            continue;
        }
        out += line;
        if (nl != std::string_view::npos) out += '\n';
        pos = end + (nl == std::string_view::npos ? 0 : 1);
    }
    return out;
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
    // indent, an optional quote marker and more indent, the list marker, one space, then "[?]"
    while (pos < out.size() && is_space(out[pos])) ++pos;
    if (pos < out.size() && out[pos] == '>') {
        ++pos;
        while (pos < out.size() && is_space(out[pos])) ++pos;
    }
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
