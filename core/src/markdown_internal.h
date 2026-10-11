#pragma once
// Private to the Markdown implementation (markdown.cpp, markdown_inline.cpp, markdown_table.cpp):
// the inline parser and small character helpers. Not part of the public API.

#include <string>
#include <vector>

#include "pbnotes/markdown.h"

namespace pbnotes {
namespace md {

bool is_space(char c);  // ' ' or '\t'

// The source lines without their line ends (a final line break does not create an empty line; a CR before the
// line break is dropped). The parser and copy_source share it so line indexes agree.
std::vector<std::string> split_lines(std::string_view text);

// Appends text as a span, merging it into the previous span when kind and link target match.
// `src` (per byte of `text`) and the element extent [el_begin, el_end) are recorded when given (spec 005); merged
// spans keep the union of their extents.
void add_span(std::vector<Span>* spans, Span::Kind kind, const std::string& text,
              const std::string& url = std::string(), bool link_confirm = false,
              const std::vector<int>* src = nullptr, int el_begin = -1, int el_end = -1);

// Inline spans of one line of text (emphasis, links). `base` is the offset of `text` inside its source line.
std::vector<Span> inline_spans(const std::string& text, int base = 0);

// One plain span holding the text exactly as written (code fences); `base` as above.
std::vector<Span> literal_spans(const std::string& text, int base = 0);

// A piece of a line: plain text, or an attachment reference between two texts.
struct Segment {
    size_t begin = 0;      // offset of the piece in the split text
    std::string text;      // is_ref == false
    bool is_ref = false;
    AttachmentRef ref;     // is_ref == true
    std::string raw;       // is_ref == true: the reference exactly as written
};

// Splits a line of text at its attachment references (outside inline code spans).
std::vector<Segment> split_attachment_refs(const std::string& text);

// Spans of a table cell: the cell's inline text, with an attachment shown as its name only.
std::vector<Span> cell_spans(const std::string& text);

// Reads a pipe table whose header line is lines[start] (the caller checked the indentation and that the
// line is not a list item, heading, quote or fence). Returns the number of source lines used, or 0 if
// there is no valid table there (then nothing is filled in).
size_t read_table(const std::vector<std::string>& lines, size_t start, Block* out);

}  // namespace md
}  // namespace pbnotes
