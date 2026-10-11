#pragma once
// Basic Markdown for the reader (spec FR-012; spec 002): headings, bold/italic, bullet and numbered
// lists, task-list items and links (titled, bare addresses, <autolinks>, external images). Code and
// anything else stays plain text. Every source line becomes one block (with its 0-based line index) so the reader can
// write a toggled checkbox back to the exact line. No Qt, no HTML: the UI escapes the text.

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace pbnotes {

enum class BlockKind { Heading, Paragraph, ListItem, Blank, Table, Attachment };

struct Span {
    enum Kind { Plain, Bold, Italic, BoldItalic };
    Kind kind = Plain;
    std::string text;
    std::string url;             // non-empty = a link; always passes is_followable_url
    bool link_confirm = false;   // link_needs_confirmation(label, url): ask before opening
};

// A GitHub-style pipe table (spec 002 FR-007..FR-013). Every row has as many cells as there are
// columns (the largest row length); rows[0] is the header.
struct TableData {
    enum Align { None, Left, Center, Right };
    std::vector<Align> align;                          // per column
    std::vector<double> weight;                        // per column, > 0, sums to 1 (relative width)
    std::vector<std::vector<std::vector<Span>>> rows;  // rows[row][column] = the cell's spans
};

// An attachment reference found in the text (spec 002 FR-019): `![name](path)` with a relative path,
// `[name](.attachments.N/path)`, or a local token `![name](pbatt:<id>)`.
struct AttachmentRef {
    std::string name;             // alt / link text (may be empty)
    std::string path;             // validated, decoded path as the server knows it; empty for a token
    int64_t token_id = 0;         // the id of a local token, else 0
    bool is_image_syntax = false; // `![..](..)` rather than `[..](..)`
};

struct Block {
    BlockKind kind = BlockKind::Paragraph;
    int line_index = 0;      // 0-based line in the source text of the block's first line
    int line_count = 1;      // source lines the block covers (a Table: header + delimiter + rows)
    int level = 0;           // Heading: 1..6
    bool ordered = false;    // ListItem
    int number = 0;          // ordered ListItem: the written number
    int depth = 0;           // ListItem nesting: indent / 2 spaces
    bool is_task = false;    // ListItem with a [ ] / [x] marker
    bool checked = false;
    std::vector<Span> spans;
    TableData table;         // Table
    AttachmentRef attachment;  // Attachment
};

// An attachment reference ends the text segment it stands in: the line becomes text block(s) and Attachment
// block(s) in source order, all with the same line_index. Inside a code fence nothing is parsed; inside a
// table cell an attachment is a plain-text span with its name.
std::vector<Block> parse_markdown(std::string_view text);

// Every attachment reference in the text in source order, including those in table cells (outside code
// fences and inline code).
std::vector<AttachmentRef> find_attachment_refs(std::string_view text);

// Rewrites every attachment reference whose decoded path is a key of `token_for_path` as the local token
// `![name](pbatt:<id>)`; everything else (other references, code, text) is unchanged byte for byte.
std::string tokenize_attachment_refs(std::string_view text, const std::map<std::string, int64_t>& token_for_path);

// Flips the checkbox of the task item on source line `line_index` ([ ] <-> [x]; [X] becomes [ ]) and
// leaves every other byte untouched. Returns the text unchanged if that line is not a task item
// (including lines inside code fences) or the index is out of range.
std::string toggle_task_item(std::string_view text, int line_index);

}  // namespace pbnotes
