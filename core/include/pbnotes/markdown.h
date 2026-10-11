#pragma once
// Basic Markdown for the reader (spec FR-012; spec 002, spec 006): headings, bold/italic/strikethrough/underline,
// bullet and numbered lists, task-list items, links (titled, bare addresses, <autolinks>, external images), fenced
// code blocks, quotes and info boxes, and collapsible <details> sections. Inline code and anything else stays plain
// text. Every source line becomes one block (with its 0-based line index) so the reader can write a toggled checkbox
// back to the exact line, and select and copy per line. No Qt, no HTML: the UI escapes the text.

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace pbnotes {

// Fence: an opening or closing fence line; Code: a line inside a fence (literal); Callout: the `> [!NOTE]` header line
// of an info box; Summary: the summary line of a <details> section; Marker: a structural <details> / </details> line
// that is never shown (spec 006).
enum class BlockKind { Heading, Paragraph, ListItem, Blank, Table, Attachment, Fence, Code, Callout, Summary, Marker };

enum class QuoteKind { None, Quote, Info };
enum class CalloutKind { Note, Tip, Warning, Important, Caution };

struct Span {
    enum Kind { Plain, Bold, Italic, BoldItalic };
    Kind kind = Plain;
    bool strike = false;         // ~~text~~ (spec 006)
    bool underline = false;      // <u>text</u> (spec 006)
    std::string text;
    std::string url;             // non-empty = a link; always passes is_followable_url
    bool link_confirm = false;   // link_needs_confirmation(label, url): ask before opening
    // Where the text came from (spec 005, for copying the note's own text). Additive: empty / -1 = unknown.
    std::vector<int> src;        // per byte of `text`: byte index in the block's source line; for a character produced
                                 // by a backslash escape the index of the backslash
    int el_begin = -1;           // source extent [el_begin, el_end) of the formatted element the span belongs to,
    int el_end = -1;             // marks included (`**bold**`, `[label](url)`, `<url>`); plain text: its own characters
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
    // Source extent of the block in its line (spec 005): from column 0 (indentation and heading/list/task prefix
    // included) to the end of its text; an Attachment: the reference as written. Not set for Table and Blank.
    int src_begin = 0;
    int src_end = 0;
    // Containers (spec 006). A quote group, a code box and a details section are flags on the per-line blocks, so
    // selection, copy and paging keep working per source line. The *_first / *_last flags mark the first and last
    // block of a group or box (the UI draws one continuous frame from them).
    QuoteKind quote = QuoteKind::None;   // the block sits in a quote / info box (any kind of block can)
    CalloutKind callout = CalloutKind::Note;  // Callout blocks
    bool quote_first = false, quote_last = false;
    bool code_first = false, code_last = false;   // Fence and Code blocks: the edges of the code box
    int details_id = -1;         // -1, or the 0-based line of the <details> line of the section the block belongs to
    bool details_open = false;   // the section was written <details open>
    std::string fence_info;      // opening Fence: the info string (`yaml`); for tests only, never shown
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

// A displayed position: the block in the parse result, the row of a Table block (0 otherwise) and a byte offset into
// the block's displayed text (the concatenation of its span texts).
struct SourcePoint {
    size_t block = 0;
    size_t row = 0;
    size_t byte = 0;
};

// The note's own source text for the displayed range [from, to] of `blocks` (= parse_markdown(note_text)); empty when
// from > to. A block selected entirely, or reached from a previous block, brings its whole line extent (prefix
// included); a partly selected one only its characters, widened to the marks of every element that is selected
// completely. Table rows, attachments and blank lines are whole units: their source line / reference as written.
// Pieces of one source line stay one line; different lines are joined by "\n".
std::string copy_source(std::string_view note_text, const std::vector<Block>& blocks, SourcePoint from, SourcePoint to);

// Flips the checkbox of the task item on source line `line_index` ([ ] <-> [x]; [X] becomes [ ]) and
// leaves every other byte untouched. Returns the text unchanged if that line is not a task item
// (including lines inside code fences) or the index is out of range.
std::string toggle_task_item(std::string_view text, int line_index);

}  // namespace pbnotes
