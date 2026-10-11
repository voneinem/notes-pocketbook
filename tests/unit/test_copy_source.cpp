// Spec 005 FR-031 / Q2: what Copy on the reading page puts on the clipboard is the note's own source text for the
// selected range: marks of completely selected elements are included, marks of partly selected ones are not, and
// the block prefix (heading marker, list marker, task box) comes with every block the selection reaches from a
// previous block or selects entirely.
#include <string>

#include "pbnotes/markdown.h"
#include "test_support.h"

using pbnotes::SourcePoint;
using pbnotes::copy_source;
using pbnotes::parse_markdown;

namespace {

std::string copy(const std::string& text, size_t from_block, size_t from_byte, size_t to_block, size_t to_byte,
                 size_t from_row = 0, size_t to_row = 0) {
    const auto blocks = parse_markdown(text);
    return copy_source(text, blocks, SourcePoint{from_block, from_row, from_byte}, SourcePoint{to_block, to_row, to_byte});
}

}  // namespace

PB_TEST(a_range_inside_plain_text) {
    PB_CHECK_EQ(copy("hello world", 0, 6, 0, 11), std::string("hello world").substr(6));
    PB_CHECK_EQ(copy("hello world", 0, 0, 0, 5), std::string("hello"));
}

PB_TEST(a_partly_selected_bold_phrase_gets_no_marks) {
    PB_CHECK_EQ(copy("**bold text** x", 0, 0, 0, 4), std::string("bold"));
    PB_CHECK_EQ(copy("**one two three**", 0, 4, 0, 7), std::string("two"));
}

PB_TEST(a_completely_selected_bold_phrase_brings_its_marks) {
    PB_CHECK_EQ(copy("**bold text** x", 0, 0, 0, 9), std::string("**bold text**"));
    PB_CHECK_EQ(copy("**bold text** x", 0, 0, 0, 10), std::string("**bold text** "));
    PB_CHECK_EQ(copy("**bold text** x", 0, 0, 0, 11), std::string("**bold text** x"));
    PB_CHECK_EQ(copy("a *soft* b", 0, 2, 0, 6), std::string("*soft*"));
}

PB_TEST(a_heading_copies_its_marker_only_when_selected_entirely) {
    PB_CHECK_EQ(copy("# Title", 0, 0, 0, 3), std::string("Tit"));
    PB_CHECK_EQ(copy("# Title", 0, 0, 0, 5), std::string("# Title"));
    PB_CHECK_EQ(copy("## Sub **x**", 0, 0, 0, 5), std::string("## Sub **x**"));
}

PB_TEST(list_and_task_items_copy_their_markers_when_selected_entirely) {
    PB_CHECK_EQ(copy("- item", 0, 0, 0, 4), std::string("- item"));
    PB_CHECK_EQ(copy("  - [ ] task", 0, 0, 0, 4), std::string("  - [ ] task"));
    PB_CHECK_EQ(copy("- [x] done", 0, 0, 0, 4), std::string("- [x] done"));
    PB_CHECK_EQ(copy("1. one", 0, 0, 0, 3), std::string("1. one"));
    PB_CHECK_EQ(copy("- item", 0, 1, 0, 3), std::string("te"));   // partly: characters only
}

PB_TEST(links_are_copied_as_written_when_selected_completely) {
    const std::string t = "see [label](https://example.org/x) now";
    PB_CHECK_EQ(copy(t, 0, 4, 0, 9), std::string("[label](https://example.org/x)"));
    PB_CHECK_EQ(copy(t, 0, 5, 0, 9), std::string("abel"));
    PB_CHECK_EQ(copy(t, 0, 0, 0, 9), std::string("see [label](https://example.org/x)"));
    PB_CHECK_EQ(copy("go <https://example.org/a> now", 0, 3, 0, 24), std::string("<https://example.org/a>"));
}

PB_TEST(a_range_over_several_blocks) {
    const std::string t = "alpha beta\n# Mid\n- gamma";
    PB_CHECK_EQ(copy(t, 0, 6, 2, 3), std::string("beta\n# Mid\n- gam"));
    PB_CHECK_EQ(copy(t, 0, 0, 2, 5), t);  // everything
}

PB_TEST(a_reversed_range_is_empty) {
    PB_CHECK_EQ(copy("alpha\nbeta", 1, 2, 0, 3), std::string());
    PB_CHECK_EQ(copy("hello", 0, 4, 0, 2), std::string());
}

PB_TEST(a_blank_line_inside_the_range_is_an_empty_line) {
    PB_CHECK_EQ(copy("a\n\nb", 0, 0, 2, 1), std::string("a\n\nb"));
}

PB_TEST(table_rows_are_copied_as_their_source_lines) {
    const std::string t = "| a | b |\n|---|---|\n| 1 | 2 |";
    PB_CHECK_EQ(copy(t, 0, 0, 0, 0, 0, 0), std::string("| a | b |"));
    PB_CHECK_EQ(copy(t, 0, 0, 0, 0, 1, 1), std::string("| 1 | 2 |"));
    PB_CHECK_EQ(copy(t, 0, 0, 0, 0, 0, 1), t);
}

PB_TEST(an_attachment_is_copied_as_the_reference_written_in_the_note) {
    PB_CHECK_EQ(copy("![n](.attachments.4/pic.png)", 0, 0, 0, 0), std::string("![n](.attachments.4/pic.png)"));
    // pieces of one source line stay one line
    PB_CHECK_EQ(copy("see ![n](pbatt:3) after", 0, 0, 2, 5), std::string("see ![n](pbatt:3) after"));
}

PB_TEST(escaped_characters_bring_their_backslash) {
    PB_CHECK_EQ(copy("\\*lit\\*", 0, 0, 0, 4), std::string("\\*lit"));
    PB_CHECK_EQ(copy("\\*lit\\*", 0, 1, 0, 4), std::string("lit"));
}

PB_TEST(offsets_inside_a_multibyte_character_move_to_the_next_boundary) {
    // "ä **ö** ü": displayed "ä ö ü" = ä(0-1) ' '(2) ö(3-4) ' '(5) ü(6-7)
    const std::string t = "\xC3\xA4 **\xC3\xB6** \xC3\xBC";
    PB_CHECK_EQ(copy(t, 0, 1, 0, 3), std::string(" "));
    PB_CHECK_EQ(copy(t, 0, 0, 0, 2), std::string("\xC3\xA4"));
}

PB_TEST(crlf_notes_join_lines_with_a_newline) {
    PB_CHECK_EQ(copy("a\r\nb", 0, 0, 1, 1), std::string("a\nb"));
}

PB_TEST(offsets_beyond_a_block_are_clamped) {
    PB_CHECK_EQ(copy("hello", 0, 0, 0, 999), std::string("hello"));
    PB_CHECK_EQ(copy("hello", 0, 3, 0, 999), std::string("lo"));
}

PB_TEST(a_block_selected_entirely_keeps_its_indentation) {
    PB_CHECK_EQ(copy("  - item", 0, 0, 0, 4), std::string("  - item"));
}

PB_TEST_MAIN()
