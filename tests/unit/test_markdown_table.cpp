// US2 (spec 002 FR-007..FR-013, research R-10): GitHub-style pipe tables in the block parser.
#include <chrono>
#include <cmath>
#include <string>
#include <vector>

#include "pbnotes/markdown.h"
#include "test_support.h"

using pbnotes::Block;
using pbnotes::BlockKind;
using pbnotes::parse_markdown;
using pbnotes::Span;
using pbnotes::TableData;

namespace {

std::string plain(const std::vector<Span>& spans) {
    std::string s;
    for (const auto& sp : spans) s += sp.text;
    return s;
}
std::string plain(const Block& b) { return plain(b.spans); }

std::vector<Block> content_blocks(const std::string& text) {
    std::vector<Block> out;
    for (auto& b : parse_markdown(text))
        if (b.kind != BlockKind::Blank) out.push_back(b);
    return out;
}
std::string cell(const Block& t, size_t r, size_t c) { return plain(t.table.rows.at(r).at(c)); }

const char* kBasic = "| Item | Qty | Note |\n|:-----|:---:|-----:|\n| Tea | 2 | **loose** leaf |\n| Rice | 10 | ok |\n";

}  // namespace

PB_TEST(a_basic_table_has_header_delimiter_and_rows) {
    auto b = content_blocks(kBasic);
    PB_CHECK_EQ(b.size(), static_cast<size_t>(1));
    PB_CHECK(b[0].kind == BlockKind::Table);
    PB_CHECK_EQ(b[0].table.rows.size(), static_cast<size_t>(3));  // header + 2 body rows
    PB_CHECK_EQ(cell(b[0], 0, 0), std::string("Item"));
    PB_CHECK_EQ(cell(b[0], 0, 2), std::string("Note"));
    PB_CHECK_EQ(cell(b[0], 1, 0), std::string("Tea"));
    PB_CHECK_EQ(cell(b[0], 2, 1), std::string("10"));
    PB_CHECK_EQ(b[0].line_index, 0);
    PB_CHECK_EQ(b[0].line_count, 4);
}

PB_TEST(alignment_markers) {
    auto b = content_blocks("| a | b | c | d |\n|:--|:-:|--:|---|\n| 1 | 2 | 3 | 4 |");
    PB_CHECK(b[0].kind == BlockKind::Table);
    const auto& a = b[0].table.align;
    PB_CHECK_EQ(a.size(), static_cast<size_t>(4));
    PB_CHECK(a[0] == TableData::Left);
    PB_CHECK(a[1] == TableData::Center);
    PB_CHECK(a[2] == TableData::Right);
    PB_CHECK(a[3] == TableData::None);
}

PB_TEST(leading_and_trailing_pipes_are_optional) {
    auto b = content_blocks("a | b\n--|--\n1 | 2\n| 3 | 4\n5 | 6 |");
    PB_CHECK(b[0].kind == BlockKind::Table);
    PB_CHECK_EQ(b[0].table.rows.size(), static_cast<size_t>(4));
    PB_CHECK_EQ(cell(b[0], 0, 0), std::string("a"));
    PB_CHECK_EQ(cell(b[0], 2, 0), std::string("3"));
    PB_CHECK_EQ(cell(b[0], 3, 1), std::string("6"));
    PB_CHECK_EQ(b[0].table.rows[3].size(), static_cast<size_t>(2));
}

PB_TEST(an_escaped_pipe_stays_in_the_cell) {
    auto b = content_blocks("| a | b |\n|---|---|\n| x \\| y | z |");
    PB_CHECK(b[0].kind == BlockKind::Table);
    PB_CHECK_EQ(cell(b[0], 1, 0), std::string("x | y"));
    PB_CHECK_EQ(cell(b[0], 1, 1), std::string("z"));
}

PB_TEST(a_header_and_delimiter_with_different_cell_counts_are_plain_paragraphs) {
    auto b = content_blocks("| a | b | c |\n|---|---|\n| 1 | 2 | 3 |");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(3));
    for (const auto& x : b) PB_CHECK(x.kind == BlockKind::Paragraph);
    PB_CHECK_EQ(plain(b[0]), std::string("| a | b | c |"));
    PB_CHECK_EQ(plain(b[1]), std::string("|---|---|"));
}

PB_TEST(a_missing_or_invalid_delimiter_row_is_never_a_partial_table) {
    for (const char* text : {"| a | b |\n| 1 | 2 |\n| 3 | 4 |", "| a | b |\n|--x|---|\n| 1 | 2 |",
                             "| a | b |\n|:|---|\n| 1 | 2 |", "| a | b |\n| - - | --- |\n| 1 | 2 |"}) {
        auto b = content_blocks(text);
        for (const auto& x : b) PB_CHECK(x.kind != BlockKind::Table);
    }
}

PB_TEST(uneven_rows_pad_to_the_longest_row) {
    auto b = content_blocks("| a | b |\n|---|---|\n| 1 |\n| 1 | 2 | 3 |");
    PB_CHECK(b[0].kind == BlockKind::Table);
    PB_CHECK_EQ(b[0].table.weight.size(), static_cast<size_t>(3));
    PB_CHECK_EQ(b[0].table.align.size(), static_cast<size_t>(3));
    for (const auto& row : b[0].table.rows) PB_CHECK_EQ(row.size(), static_cast<size_t>(3));
    PB_CHECK_EQ(cell(b[0], 0, 2), std::string(""));  // header padded
    PB_CHECK_EQ(cell(b[0], 1, 1), std::string(""));  // short row padded
    PB_CHECK_EQ(cell(b[0], 2, 2), std::string("3"));  // the extra cell is kept
}

PB_TEST(cells_are_trimmed_and_parsed_inline) {
    auto b = content_blocks("| a | b |\n|---|---|\n|  **bold**  | [l](https://x.example) 😀 |\n|   |  |");
    PB_CHECK(b[0].kind == BlockKind::Table);
    const auto& bold = b[0].table.rows[1][0];
    PB_CHECK_EQ(bold.size(), static_cast<size_t>(1));
    PB_CHECK(bold[0].kind == Span::Bold && bold[0].text == "bold");
    const auto& link = b[0].table.rows[1][1];
    PB_CHECK_EQ(link.size(), static_cast<size_t>(2));
    PB_CHECK_EQ(link[0].url, std::string("https://x.example"));
    PB_CHECK_EQ(link[1].text, std::string(" 😀"));
    PB_CHECK(b[0].table.rows[2][0].empty());  // an empty cell has no spans
    PB_CHECK(b[0].table.rows[2][1].empty());
}

PB_TEST(a_table_ends_at_a_blank_line_or_a_line_without_a_pipe) {
    auto b = content_blocks("| a |\n|---|\n| 1 |\n\nafter blank\n| b |\n|---|\n| 2 |\nno pipe here\nlast");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(5));
    PB_CHECK(b[0].kind == BlockKind::Table);
    PB_CHECK_EQ(plain(b[1]), std::string("after blank"));
    PB_CHECK(b[2].kind == BlockKind::Table);
    PB_CHECK_EQ(b[2].table.rows.size(), static_cast<size_t>(2));
    PB_CHECK_EQ(plain(b[3]), std::string("no pipe here"));
    PB_CHECK_EQ(plain(b[4]), std::string("last"));
}

PB_TEST(a_table_is_not_recognized_in_lists_quotes_fences_or_indented_code) {
    for (const char* text : {"- | a | b |\n  |---|---|\n  | 1 | 2 |", "> | a | b |\n> |---|---|\n> | 1 | 2 |",
                             "```\n| a | b |\n|---|---|\n| 1 | 2 |\n```", "    | a | b |\n    |---|---|\n    | 1 | 2 |"}) {
        for (const auto& x : parse_markdown(text)) PB_CHECK(x.kind != BlockKind::Table);
    }
}

PB_TEST(a_fence_line_ends_a_table_whatever_its_kind) {
    for (const char* fence : {"```", "~~~", "````"}) {
        auto b = parse_markdown(std::string("| a |\n|---|\n| 1 |\n") + fence + "\n| x |\n" + fence);
        PB_CHECK(b[0].kind == BlockKind::Table);
        PB_CHECK_EQ(b[0].table.rows.size(), static_cast<size_t>(2));  // header and the one body row
        PB_CHECK(b[1].kind == BlockKind::Fence);
    }
}

PB_TEST(two_tables_in_one_note) {
    auto b = content_blocks("| a |\n|---|\n| 1 |\n\ntext\n\n| x | y |\n|---|---|\n| 2 | 3 |");
    int tables = 0;
    for (const auto& x : b) tables += x.kind == BlockKind::Table;
    PB_CHECK_EQ(tables, 2);
}

PB_TEST(line_indexes_continue_after_a_table) {
    auto all = parse_markdown("intro\n| a | b |\n|---|---|\n| 1 | 2 |\n| 3 | 4 |\nafter\n\n- [ ] task");
    // intro(0) table(1..4) after(5) blank(6) task(7)
    PB_CHECK_EQ(all.size(), static_cast<size_t>(5));
    PB_CHECK_EQ(all[0].line_index, 0);
    PB_CHECK(all[1].kind == BlockKind::Table);
    PB_CHECK_EQ(all[1].line_index, 1);
    PB_CHECK_EQ(all[1].line_count, 4);
    PB_CHECK_EQ(all[2].line_index, 5);
    PB_CHECK_EQ(all[3].line_index, 6);
    PB_CHECK_EQ(all[4].line_index, 7);
    PB_CHECK_EQ(all[4].line_count, 1);
}

PB_TEST(column_weights_are_positive_sum_to_one_and_follow_content) {
    auto b = content_blocks("| Item | Qty | Note |\n|---|---|---|\n| A very long cell that has to wrap inside its column | 1 | ok |");
    PB_CHECK(b[0].kind == BlockKind::Table);
    const auto& w = b[0].table.weight;
    PB_CHECK_EQ(w.size(), static_cast<size_t>(3));
    double sum = 0;
    for (double x : w) {
        PB_CHECK(x > 0);
        sum += x;
    }
    PB_CHECK(std::fabs(sum - 1.0) < 1e-9);
    PB_CHECK(w[0] > w[1]);
    PB_CHECK(w[0] > w[2]);
}

PB_TEST(a_very_long_cell_is_clamped) {
    auto b = content_blocks("| a | b |\n|---|---|\n| " + std::string(5000, 'x') + " | " + std::string(40, 'y') + " |");
    const auto& w = b[0].table.weight;
    // both columns hit the clamp: neither dominates
    PB_CHECK(std::fabs(w[0] - w[1]) < 1e-9);
}

PB_TEST(a_500_row_table_parses_quickly) {
    std::string text = "| a | b | c |\n|---|---|---|\n";
    for (int i = 0; i < 500; ++i) text += "| row " + std::to_string(i) + " | **bold** | [l](https://x.example) |\n";
    const auto t0 = std::chrono::steady_clock::now();
    auto b = parse_markdown(text);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    PB_CHECK_EQ(b.size(), static_cast<size_t>(1));
    PB_CHECK_EQ(b[0].table.rows.size(), static_cast<size_t>(501));
    PB_CHECK(ms < 50);
}

PB_TEST(toggle_task_item_is_unaffected_by_tables) {
    const std::string text = "| a |\n|---|\n| [ ] x |\n- [ ] real task";
    PB_CHECK_EQ(pbnotes::toggle_task_item(text, 2), text);  // inside a table: nothing changes
    PB_CHECK_EQ(pbnotes::toggle_task_item(text, 3), std::string("| a |\n|---|\n| [ ] x |\n- [x] real task"));
}

PB_TEST_MAIN()
