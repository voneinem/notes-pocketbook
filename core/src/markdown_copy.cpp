// copy_source (spec 005 FR-031): the note's own source text for a range selected on the reading page.
#include <algorithm>
#include <string>
#include <vector>

#include "markdown_internal.h"
#include "pbnotes/markdown.h"

namespace pbnotes {

namespace {

// A stretch [begin, end) of one source line; `empty` stands for a selected line break with nothing of the line.
struct Piece {
    size_t line = 0;
    int begin = 0;
    int end = 0;
    bool empty = false;
};

bool is_text(const Block& b) {
    return b.kind == BlockKind::Heading || b.kind == BlockKind::Paragraph || b.kind == BlockKind::ListItem ||
           b.kind == BlockKind::Code || b.kind == BlockKind::Summary;
}

size_t displayed_length(const Block& b) {
    size_t n = 0;
    for (const Span& sp : b.spans) n += sp.text.size();
    return n;
}

std::string displayed_text(const Block& b) {
    std::string s;
    for (const Span& sp : b.spans) s += sp.text;
    return s;
}

// A character produced by a backslash escape occupies two bytes of the source line.
int source_length_of_char(const std::string& line, int at) {
    static const std::string kEscapable = "*_[]\\`";
    if (at >= 0 && static_cast<size_t>(at) + 1 < line.size() && line[static_cast<size_t>(at)] == '\\' &&
        kEscapable.find(line[static_cast<size_t>(at) + 1]) != std::string::npos)
        return 2;
    return 1;
}

// The line index of row `row` of a table block: header, delimiter line, then the data rows.
size_t table_line(const Block& b, size_t row) { return static_cast<size_t>(b.line_index) + (row == 0 ? 0 : row + 1); }

bool before(const SourcePoint& a, const SourcePoint& b) {
    if (a.block != b.block) return a.block < b.block;
    if (a.row != b.row) return a.row < b.row;
    return a.byte < b.byte;
}

// The piece of a text block for displayed bytes [s, e); `reached` = the selection comes from a previous block,
// `continues` = it goes on into a later one.
Piece text_piece(const Block& b, const std::string& line, size_t s, size_t e, bool reached, bool continues) {
    const size_t len = displayed_length(b);
    const std::string plain = displayed_text(b);
    s = std::min(s, len);
    e = std::min(e, len);
    while (s < len && (static_cast<unsigned char>(plain[s]) & 0xC0) == 0x80) ++s;  // not inside a character
    while (e < len && (static_cast<unsigned char>(plain[e]) & 0xC0) == 0x80) ++e;
    Piece p;
    p.line = static_cast<size_t>(b.line_index);
    if (s >= e) {
        p.empty = true;
        return p;
    }
    if (s == 0 && e == len) {  // selected entirely: the whole block with its prefix
        p.begin = b.src_begin;
        p.end = b.src_end;
        return p;
    }
    // where the first and the last selected byte come from
    int begin = -1, end = -1;
    size_t t0 = 0;
    for (const Span& sp : b.spans) {
        const size_t t1 = t0 + sp.text.size();
        if (begin < 0 && s >= t0 && s < t1) begin = sp.src.empty() ? sp.el_begin : sp.src[s - t0];
        if (end < 0 && e - 1 >= t0 && e - 1 < t1) {
            end = sp.src.empty() ? sp.el_end : sp.src[e - 1 - t0] + source_length_of_char(line, sp.src[e - 1 - t0]);
        }
        t0 = t1;
    }
    if (begin < 0 || end < 0) {
        p.begin = b.src_begin;
        p.end = b.src_end;
        return p;
    }
    // elements that are selected completely bring their marks
    t0 = 0;
    for (size_t i = 0; i < b.spans.size(); ++i) {
        const Span& sp = b.spans[i];
        const size_t t1 = t0 + sp.text.size();
        const bool inside = t0 >= s && t1 <= e;
        t0 = t1;
        if (!inside || sp.el_begin < 0) continue;
        bool all = true;  // every span of the same element must be inside, or the marks are not complete
        size_t u0 = 0;
        for (const Span& other : b.spans) {
            const size_t u1 = u0 + other.text.size();
            if (other.el_begin == sp.el_begin && other.el_end == sp.el_end && !(u0 >= s && u1 <= e)) all = false;
            u0 = u1;
        }
        if (all) {
            begin = std::min(begin, sp.el_begin);
            end = std::max(end, sp.el_end);
        }
    }
    if (reached && s == 0) begin = std::min(begin, b.src_begin);
    if (continues && e == len) end = std::max(end, b.src_end);
    p.begin = begin;
    p.end = end;
    return p;
}

}  // namespace

std::string copy_source(std::string_view note_text, const std::vector<Block>& blocks, SourcePoint from, SourcePoint to) {
    if (blocks.empty() || from.block >= blocks.size() || to.block >= blocks.size() || before(to, from)) return std::string();
    const std::vector<std::string> lines = md::split_lines(note_text);

    std::vector<Piece> pieces;
    auto add = [&](Piece p) {
        if (!pieces.empty() && !p.empty && !pieces.back().empty && pieces.back().line == p.line) {
            pieces.back().begin = std::min(pieces.back().begin, p.begin);
            pieces.back().end = std::max(pieces.back().end, p.end);
        } else {
            pieces.push_back(p);
        }
    };

    for (size_t bi = from.block; bi <= to.block; ++bi) {
        const Block& b = blocks[bi];
        const bool first = bi == from.block, last = bi == to.block;
        if (b.kind == BlockKind::Table) {
            const size_t rows = b.table.rows.size();
            if (rows == 0) continue;
            const size_t r0 = first ? std::min(from.row, rows - 1) : 0;
            const size_t r1 = last ? std::min(to.row, rows - 1) : rows - 1;
            for (size_t l = table_line(b, r0); l <= table_line(b, r1); ++l) {
                Piece p;
                p.line = l;
                p.begin = 0;
                p.end = l < lines.size() ? static_cast<int>(lines[l].size()) : 0;
                add(p);
            }
        } else if (b.kind == BlockKind::Attachment) {
            Piece p;
            p.line = static_cast<size_t>(b.line_index);
            p.begin = b.src_begin;
            p.end = b.src_end;
            add(p);
        } else if (b.kind == BlockKind::Fence || b.kind == BlockKind::Callout || b.kind == BlockKind::Marker) {
            // whole units: the line as written (spec 006)
            Piece p;
            p.line = static_cast<size_t>(b.line_index);
            p.begin = 0;
            p.end = b.line_index >= 0 && static_cast<size_t>(b.line_index) < lines.size()
                        ? static_cast<int>(lines[static_cast<size_t>(b.line_index)].size()) : 0;
            pieces.push_back(p);
        } else if (b.kind == BlockKind::Blank) {
            Piece p;
            p.line = static_cast<size_t>(b.line_index);
            pieces.push_back(p);  // an empty line of its own
        } else if (is_text(b)) {
            const size_t len = displayed_length(b);
            const size_t s = first ? from.byte : 0;
            const size_t e = last ? to.byte : len;
            const std::string& line = static_cast<size_t>(b.line_index) < lines.size() ? lines[static_cast<size_t>(b.line_index)] : std::string();
            add(text_piece(b, line, s, e, !first, !last));
        }
    }

    std::string out;
    for (size_t i = 0; i < pieces.size(); ++i) {
        if (i > 0) out += '\n';
        const Piece& p = pieces[i];
        if (p.empty || p.line >= lines.size()) continue;
        const std::string& line = lines[p.line];
        const size_t begin = static_cast<size_t>(std::max(0, p.begin));
        const size_t end = std::min(line.size(), static_cast<size_t>(std::max(0, p.end)));
        if (begin < end) out.append(line, begin, end - begin);
    }
    return out;
}

}  // namespace pbnotes
