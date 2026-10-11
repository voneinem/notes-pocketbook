#pragma once
// Private to the Markdown implementation (markdown.cpp, markdown_inline.cpp, markdown_table.cpp):
// the inline parser and small character helpers. Not part of the public API.

#include <string>
#include <vector>

#include "pbnotes/markdown.h"

namespace pbnotes {
namespace md {

bool is_space(char c);  // ' ' or '\t'

// Appends text as a span, merging it into the previous span when kind and link target match.
void add_span(std::vector<Span>* spans, Span::Kind kind, const std::string& text,
              const std::string& url = std::string(), bool link_confirm = false);

// Inline spans of one line of text (emphasis, links).
std::vector<Span> inline_spans(const std::string& text);

// One plain span holding the text exactly as written (code fences).
std::vector<Span> literal_spans(const std::string& text);

// A piece of a line: plain text, or an attachment reference between two texts.
struct Segment {
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
