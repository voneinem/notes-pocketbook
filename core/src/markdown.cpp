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
    bool in_fence = false;
    const std::vector<std::string> lines = split_lines(text);
    for (size_t index = 0; index < lines.size(); ++index) {
        const std::string& line = lines[index];

        Block b;
        b.line_index = static_cast<int>(index);

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
            b.spans = md::literal_spans(rest, static_cast<int>(first));
            b.src_end = static_cast<int>(line.size());
            blocks.push_back(std::move(b));
            continue;
        }
        if (in_fence) {
            b.spans = md::literal_spans(line, 0);
            b.src_end = static_cast<int>(line.size());
            blocks.push_back(std::move(b));
            continue;
        }

        // heading
        size_t hashes = 0;
        while (hashes < rest.size() && rest[hashes] == '#') ++hashes;
        if (hashes >= 1 && hashes <= 6 && hashes < rest.size() && rest[hashes] == ' ' && indent_cols < 4) {
            b.kind = BlockKind::Heading;
            b.level = static_cast<int>(hashes);
            push_text_blocks(&blocks, std::move(b), rest.substr(hashes + 1), static_cast<int>(first + hashes + 1));
            continue;
        }

        std::string body;
        size_t body_at = first;  // where `body` starts in the line
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

        if (indent_cols < 4 && rest[0] != '>' && rest.size() <= kMaxParsedLine) {
            const size_t used = md::read_table(lines, index, &b);
            if (used > 0) {
                blocks.push_back(std::move(b));
                index += used - 1;
                continue;
            }
        }

        b.kind = BlockKind::Paragraph;
        push_text_blocks(&blocks, std::move(b), rest, static_cast<int>(first));
    }
    return blocks;
}

std::vector<AttachmentRef> find_attachment_refs(std::string_view text) {
    std::vector<AttachmentRef> refs;
    bool in_fence = false;
    for (const std::string& line : split_lines(text)) {
        size_t first = 0;
        while (first < line.size() && is_space(line[first])) ++first;
        if (line.compare(first, 3, "```") == 0) {
            in_fence = !in_fence;
            continue;
        }
        if (in_fence || line.size() > kMaxParsedLine) continue;
        for (const md::Segment& seg : md::split_attachment_refs(line))
            if (seg.is_ref) refs.push_back(seg.ref);
    }
    return refs;
}

std::string tokenize_attachment_refs(std::string_view text, const std::map<std::string, int64_t>& token_for_path) {
    if (token_for_path.empty()) return std::string(text);
    std::string out;
    bool in_fence = false;
    size_t pos = 0;
    while (pos < text.size()) {
        const size_t nl = text.find('\n', pos);
        const size_t end = nl == std::string_view::npos ? text.size() : nl;
        const std::string line(text.substr(pos, end - pos));
        size_t first = 0;
        while (first < line.size() && is_space(line[first])) ++first;
        if (line.compare(first, 3, "```") == 0) in_fence = !in_fence;
        else if (!in_fence && line.size() <= kMaxParsedLine) {
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
