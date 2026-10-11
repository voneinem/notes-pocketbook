// US2 (spec 002 FR-012, research R-11): the reader's page step that never cuts an unsplittable block.
#include <vector>

#include "pbnotes/paging.h"
#include "test_support.h"

using pbnotes::Interval;
using pbnotes::page_target;

namespace {
const std::vector<Interval> none;
}

PB_TEST(without_unsplittable_blocks_the_plain_step_is_used) {
    PB_CHECK_EQ(page_target(0, 1000, 5000, 920, 1, none), 920.0);
    PB_CHECK_EQ(page_target(920, 1000, 5000, 920, 1, none), 1840.0);
    PB_CHECK_EQ(page_target(1840, 1000, 5000, 920, -1, none), 920.0);
}

PB_TEST(a_block_across_the_bottom_edge_moves_the_next_page_start_to_its_top) {
    // the next page would start at 920; a row spans 900..960 and straddles that edge
    std::vector<Interval> rows = {{800, 860}, {900, 960}, {960, 1020}};
    PB_CHECK_EQ(page_target(0, 1000, 5000, 920, 1, rows), 900.0);
}

PB_TEST(a_block_exactly_at_the_edge_is_not_moved) {
    std::vector<Interval> rows = {{800, 920}, {920, 980}};
    PB_CHECK_EQ(page_target(0, 1000, 5000, 920, 1, rows), 920.0);
}

PB_TEST(going_back_moves_the_new_top_up_to_the_blocks_top) {
    // from 1840 back by 920 gives 920; a block spans 900..960 across that top edge
    std::vector<Interval> rows = {{900, 960}};
    PB_CHECK_EQ(page_target(1840, 1000, 5000, 920, -1, rows), 900.0);
}

PB_TEST(a_heading_and_the_line_below_it_stay_on_one_page) {
    // spec 006 FR-005: the reader lists a heading's rectangle widened by one body line; the heading (860..900) is the
    // last thing that would fit above the edge at 920, its line (900..945) would start the next page: the page ends
    // above the heading instead.
    std::vector<Interval> heading = {{860, 900 + 53}};
    PB_CHECK_EQ(page_target(0, 1000, 5000, 920, 1, heading), 860.0);
    // a heading that is wholly above the edge is not touched
    std::vector<Interval> earlier = {{700, 800 + 53}};
    PB_CHECK_EQ(page_target(0, 1000, 5000, 920, 1, earlier), 920.0);
}

PB_TEST(a_block_taller_than_the_view_is_ignored) {
    std::vector<Interval> big = {{500, 2000}};
    PB_CHECK_EQ(page_target(0, 1000, 5000, 920, 1, big), 920.0);
}

PB_TEST(the_target_always_makes_progress) {
    // the straddling block starts at the current top: moving to its top would not move at all
    std::vector<Interval> rows = {{0, 950}};
    const double t = page_target(0, 1000, 5000, 920, 1, rows);
    PB_CHECK(t > 0.0);
    // moving back: the block's top is at or below the current top
    std::vector<Interval> back = {{1840, 2500}};
    const double u = page_target(1840, 1000, 5000, 920, -1, back);
    PB_CHECK(u < 1840.0);
}

PB_TEST(the_result_is_clamped_to_the_content) {
    PB_CHECK_EQ(page_target(0, 1000, 1500, 920, 1, none), 500.0);   // end: content_height - view_height
    PB_CHECK_EQ(page_target(500, 1000, 1500, 920, 1, none), 500.0); // already at the end
    PB_CHECK_EQ(page_target(300, 1000, 5000, 920, -1, none), 0.0);
    PB_CHECK_EQ(page_target(0, 1000, 5000, 920, -1, none), 0.0);    // already at the start
    PB_CHECK_EQ(page_target(0, 1000, 800, 920, 1, none), 0.0);      // content shorter than the view
}

PB_TEST(adjacent_blocks_form_a_row_by_row_table) {
    std::vector<Interval> rows;
    for (int i = 0; i < 40; ++i) rows.push_back({i * 70.0, i * 70.0 + 70.0});
    double y = 0;
    for (int page = 0; page < 6; ++page) {
        const double next = page_target(y, 1000, 2800, 920, 1, rows);
        // no row is cut at the top of a page after the first
        bool on_boundary = false;
        for (const auto& r : rows) on_boundary |= (r.top == next);
        PB_CHECK(on_boundary || next == 2800 - 1000);
        PB_CHECK(next >= y);
        y = next;
    }
}

PB_TEST(an_empty_list_equals_the_plain_step) {
    PB_CHECK_EQ(page_target(100, 800, 3000, 700, 1, none), 800.0);
}

PB_TEST_MAIN()
