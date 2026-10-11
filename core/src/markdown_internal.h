#pragma once
// Private to the Markdown implementation (markdown.cpp, markdown_inline.cpp, markdown_table.cpp):
// the inline parser and small character helpers. Not part of the public API.

#include <optional>
#include <string>
#include <vector>

#include "pbnotes/markdown.h"

namespace pbnotes {
namespace md {

bool is_space(char c);  // ' ' or '\t'

// The one fence rule (spec 006 research R-1), shared by the block parser, the attachment finder, the tokenizer and
// the table reader. An opener: at most 3 spaces of indent (a tab counts 4), then three or more of one character,
// '`' or '~'; the info string after it (`yaml`, trimmed) must not contain a backtick when the character is '`'.
struct FenceOpen {
    char ch = '`';
    size_t count = 3;
    std::string info;
};
std::optional<FenceOpen> open_fence(const std::string& line);
// A closer of `open`: at most 3 spaces of indent, the same character at least as often, then only blanks.
bool closes_fence(const std::string& line, const FenceOpen& open);

// A quote line: at most 3 spaces of indent, '>' and one optional space (spec 006). On success `*len` is the length of
// that prefix; the rest of the line is parsed like any other line.
bool quote_prefix(const std::string& line, size_t* len);

// Follows the fences of a text line by line, for the code that looks for references and must skip exactly what the
// reading page shows as code. A fence that was opened on a quote line ends with the quote.
class FenceTracker {
public:
    // True when `line` is a fence line or lies inside a fence; updates the state.
    bool line(const std::string& text);

private:
    std::optional<FenceOpen> open_;
    bool in_quote_ = false;
};

// The source lines without their line ends (a final line break does not create an empty line; a CR before the
// line break is dropped). The parser and copy_source share it so line indexes agree.
std::vector<std::string> split_lines(std::string_view text);

// Appends text as a span, merging it into the previous span when kind and link target match.
// `src` (per byte of `text`) and the element extent [el_begin, el_end) are recorded when given (spec 005); merged
// spans keep the union of their extents. Spans merge only when kind, link and both flags (spec 006: ~~strike~~,
// <u>underline</u>) are equal.
void add_span(std::vector<Span>* spans, Span::Kind kind, const std::string& text,
              const std::string& url = std::string(), bool link_confirm = false,
              const std::vector<int>* src = nullptr, int el_begin = -1, int el_end = -1,
              bool strike = false, bool underline = false);

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
