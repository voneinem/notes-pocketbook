#pragma once
// Basic Markdown for the reader (spec FR-012): headings, bold/italic, bullet and numbered lists,
// task-list items, links shown as their label. Tables, code, images and anything else stay plain
// text. Every source line becomes one block (with its 0-based line index) so the reader can
// write a toggled checkbox back to the exact line. No Qt, no HTML: the UI escapes the text.

#include <string>
#include <string_view>
#include <vector>

namespace pbnotes {

enum class BlockKind { Heading, Paragraph, ListItem, Blank };

struct Span {
    enum Kind { Plain, Bold, Italic, BoldItalic };
    Kind kind = Plain;
    std::string text;
};

struct Block {
    BlockKind kind = BlockKind::Paragraph;
    int line_index = 0;      // 0-based line in the source text
    int level = 0;           // Heading: 1..6
    bool ordered = false;    // ListItem
    int number = 0;          // ordered ListItem: the written number
    int depth = 0;           // ListItem nesting: indent / 2 spaces
    bool is_task = false;    // ListItem with a [ ] / [x] marker
    bool checked = false;
    std::vector<Span> spans;
};

std::vector<Block> parse_markdown(std::string_view text);

// Flips the checkbox of the task item on source line `line_index` ([ ] <-> [x]; [X] becomes [ ]) and
// leaves every other byte untouched. Returns the text unchanged if that line is not a task item
// (including lines inside code fences) or the index is out of range.
std::string toggle_task_item(std::string_view text, int line_index);

}  // namespace pbnotes
