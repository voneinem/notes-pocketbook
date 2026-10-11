// Spec 005 FR-011..FR-016: the edit history records every change of title, text, category and favorite as a step,
// groups typing, and keeps a bounded number of steps. Observation, diffs, grouping and caps; undo/redo itself is in
// test_edit_history_undo.
#include <chrono>
#include <string>

#include "pbnotes/edit_history.h"
#include "test_support.h"

using pbnotes::EditApplied;
using pbnotes::EditField;
using pbnotes::EditHistory;

namespace {

EditHistory opened(const std::u16string& text = u"", const std::u16string& title = u"T",
                   const std::u16string& category = u"", bool favorite = false) {
    EditHistory h;
    h.reset(title, text, category, favorite);
    return h;
}

// Types `s` one character at a time into the text, `gap_ms` apart, starting at `t`; returns the final value.
std::u16string type(EditHistory& h, std::u16string value, const std::u16string& s, int64_t* t, int64_t gap_ms) {
    for (char16_t c : s) {
        value.push_back(c);
        h.observe(EditField::Text, value, *t);
        *t += gap_ms;
    }
    return value;
}

}  // namespace

PB_TEST(inactive_before_reset_ignores_observations) {
    EditHistory h;
    PB_CHECK(!h.active());
    h.observe(EditField::Text, u"abc", 0);
    PB_CHECK(!h.can_undo());
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(0));
}

PB_TEST(a_fresh_history_has_nothing_to_undo_or_redo) {
    EditHistory h = opened(u"abc");
    PB_CHECK(h.active());
    PB_CHECK(!h.can_undo());
    PB_CHECK(!h.can_redo());
    PB_CHECK(!h.undo().has_value());
    PB_CHECK(!h.redo().has_value());
}

PB_TEST(observing_the_known_value_is_a_no_op) {
    EditHistory h = opened(u"abc");
    h.observe(EditField::Text, u"abc", 0);
    h.observe(EditField::Title, u"T", 1);
    h.observe(EditField::Category, u"", 2);
    h.observe(EditField::Favorite, u"", 3);
    PB_CHECK(!h.can_undo());
}

PB_TEST(a_replacement_is_one_minimal_step) {
    EditHistory h = opened(u"abcdef");
    h.observe(EditField::Text, u"abXdef", 0);
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(1));
    auto a = h.undo();
    PB_CHECK(a.has_value());
    PB_CHECK(a->field == EditField::Text);
    PB_CHECK_EQ(a->pos, static_cast<size_t>(2));
    PB_CHECK_EQ(a->remove_len, static_cast<size_t>(1));
    PB_CHECK(a->insert == u"c");
    PB_CHECK_EQ(a->sel_begin, static_cast<size_t>(2));  // the restored text is selected
    PB_CHECK_EQ(a->sel_end, static_cast<size_t>(3));
}

PB_TEST(repeated_characters_diff_after_the_common_prefix) {
    EditHistory h = opened(u"aaa");
    h.observe(EditField::Text, u"aaaa", 0);
    auto a = h.undo();
    PB_CHECK_EQ(a->pos, static_cast<size_t>(3));
    PB_CHECK_EQ(a->remove_len, static_cast<size_t>(1));
    PB_CHECK(a->insert.empty());
}

PB_TEST(a_surrogate_pair_is_never_cut_in_half) {
    const std::u16string smile = u"\U0001F600", grin = u"\U0001F601";
    {
        EditHistory h = opened(u"a" + smile + u"b");
        h.observe(EditField::Text, u"ab", 0);
        auto a = h.undo();
        PB_CHECK_EQ(a->pos, static_cast<size_t>(1));
        PB_CHECK(a->insert == smile);  // the whole pair
        PB_CHECK_EQ(a->remove_len, static_cast<size_t>(0));
    }
    {
        EditHistory h = opened(u"a" + smile + u"b");
        h.observe(EditField::Text, u"a" + grin + u"b", 0);  // same high surrogate, different low one
        auto a = h.undo();
        PB_CHECK_EQ(a->pos, static_cast<size_t>(1));
        PB_CHECK_EQ(a->remove_len, static_cast<size_t>(2));
        PB_CHECK(a->insert == smile);
    }
}

PB_TEST(continuous_typing_is_one_step) {
    EditHistory h = opened(u"");
    int64_t t = 0;
    type(h, u"", u"hello", &t, 100);
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(1));
}

PB_TEST(a_pause_starts_a_new_step) {
    EditHistory h = opened(u"");
    int64_t t = 0;
    std::u16string v = type(h, u"", u"ab", &t, 100);
    t += 1500 - 100 + 1;  // 1501 ms since the previous observation
    v = type(h, v, u"c", &t, 100);
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(2));
}

PB_TEST(exactly_1500_ms_still_merges) {
    EditHistory h = opened(u"");
    h.observe(EditField::Text, u"a", 0);
    h.observe(EditField::Text, u"ab", 1500);
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(1));
}

PB_TEST(a_step_holds_at_most_80_typed_units) {
    EditHistory h = opened(u"");
    int64_t t = 0;
    type(h, u"", std::u16string(80, u'x'), &t, 10);
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(1));
    EditHistory g = opened(u"");
    t = 0;
    type(g, u"", std::u16string(81, u'x'), &t, 10);
    PB_CHECK_EQ(g.undo_size(), static_cast<size_t>(2));
}

PB_TEST(a_line_break_is_a_step_of_its_own) {
    EditHistory h = opened(u"");
    h.observe(EditField::Text, u"a", 0);
    h.observe(EditField::Text, u"a\n", 10);
    h.observe(EditField::Text, u"a\nb", 20);
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(3));
}

PB_TEST(typing_elsewhere_or_in_another_field_starts_a_new_step) {
    EditHistory h = opened(u"abc");
    h.observe(EditField::Text, u"abcd", 0);
    h.observe(EditField::Text, u"Xabcd", 10);  // not adjacent: the caret moved to the start
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(2));
    h.observe(EditField::Title, u"Tx", 20);
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(3));
}

PB_TEST(a_backspace_run_is_one_step) {
    EditHistory h = opened(u"hello");
    h.observe(EditField::Text, u"hell", 0);
    h.observe(EditField::Text, u"hel", 100);
    h.observe(EditField::Text, u"he", 200);
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(1));
    auto a = h.undo();
    PB_CHECK_EQ(a->pos, static_cast<size_t>(2));
    PB_CHECK(a->insert == u"llo");  // the whole run comes back
    PB_CHECK_EQ(a->remove_len, static_cast<size_t>(0));
}

PB_TEST(a_forward_delete_run_is_one_step) {
    EditHistory h = opened(u"hello");
    h.observe(EditField::Text, u"ello", 0);
    h.observe(EditField::Text, u"llo", 100);
    h.observe(EditField::Text, u"lo", 200);
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(1));
    PB_CHECK(h.undo()->insert == u"hel");
}

PB_TEST(typing_and_deleting_do_not_merge) {
    EditHistory h = opened(u"abc");
    h.observe(EditField::Text, u"abcd", 0);
    h.observe(EditField::Text, u"abc", 10);  // backspace right after typing
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(2));
}

PB_TEST(a_paste_never_merges_with_typing) {
    EditHistory h = opened(u"");
    h.observe(EditField::Text, u"a", 0);
    h.observe(EditField::Text, u"aXYZ", 10);   // pasted run
    h.observe(EditField::Text, u"aXYZb", 20);  // typing after it
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(3));
}

PB_TEST(category_and_favorite_steps_never_merge) {
    EditHistory h = opened(u"");
    h.observe(EditField::Category, u"Work", 0);
    h.observe(EditField::Category, u"Home", 10);
    h.observe(EditField::Favorite, u"1", 20);
    h.observe(EditField::Favorite, u"", 30);
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(4));
}

PB_TEST(forty_characters_typed_continuously_undo_in_at_most_three_steps) {
    EditHistory h = opened(u"");
    int64_t t = 0;
    type(h, u"", u"The quick brown fox jumps over the lazy", &t, 150);  // 38 chars
    PB_CHECK(h.undo_size() <= 3);
}

PB_TEST(the_step_cap_drops_the_oldest_steps) {
    EditHistory h(3, 2000000);
    h.reset(u"T", u"", u"", false);
    std::u16string v;
    int64_t t = 0;
    for (int i = 0; i < 5; ++i) {
        v += u"x";
        t += 5000;  // each its own step
        h.observe(EditField::Text, v, t);
    }
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(3));
    // the three newest remain: undoing them leaves the two oldest characters
    h.undo(); h.undo(); auto a = h.undo();
    PB_CHECK(a.has_value());
    PB_CHECK(!h.can_undo());
    PB_CHECK_EQ(a->pos, static_cast<size_t>(2));
}

PB_TEST(the_character_cap_drops_older_steps_but_keeps_the_newest) {
    EditHistory h(200, 10);
    h.reset(u"T", u"", u"", false);
    std::u16string v;
    int64_t t = 0;
    for (int i = 0; i < 4; ++i) {
        v += u"abcdef";  // a 6-character paste each
        t += 5000;
        h.observe(EditField::Text, v, t);
    }
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(1));  // 6 + 6 > 10: only the newest fits
    PB_CHECK(h.can_undo());
}

PB_TEST(observing_in_a_large_text_is_fast) {
    EditHistory h = opened(std::u16string(100000, u'a'));
    std::u16string v(100000, u'a');
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
        v.push_back(u'b');
        h.observe(EditField::Text, v, i * 10);
    }
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    PB_CHECK(ms < 1000);
}

// ---- spec 006 (research R-8): a group opened before the typed "/" makes the insertion one step ----

PB_TEST(a_group_across_the_typed_slash_and_the_replacement_is_one_undo_step) {
    EditHistory h = opened(u"ab");
    h.begin_group(EditField::Text);              // opened when the "/" is detected, before it is reported
    h.observe(EditField::Text, u"ab/", 1000);    // the typed "/"
    h.observe(EditField::Text, u"ab//", 1100);   // a second "/" (the date-time trigger)
    h.observe(EditField::Text, u"ab2026-10-11 14:30", 5000);  // the picker's Insert replaced "//"
    h.end_group();
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(1));
    auto a = h.undo();
    PB_CHECK(a.has_value());
    PB_CHECK_EQ(a->pos, static_cast<size_t>(2));
    PB_CHECK_EQ(a->remove_len, static_cast<size_t>(16));
    PB_CHECK(a->insert.empty());
    PB_CHECK(!h.can_undo());                      // back at the opened text: "ab"
    auto r = h.redo();
    PB_CHECK(r.has_value());
    PB_CHECK(r->insert == u"2026-10-11 14:30");
}

PB_TEST(a_dismissed_slash_group_is_an_ordinary_step_that_later_typing_does_not_merge_into) {
    EditHistory h = opened(u"a ");
    h.begin_group(EditField::Text);
    h.observe(EditField::Text, u"a /", 1000);
    h.end_group();                                // the menu was dismissed
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(1));
    h.observe(EditField::Text, u"a /x", 1100);    // typing goes on right away
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(2));   // not merged into the "/" step
    auto a = h.undo();
    PB_CHECK(a.has_value());
    PB_CHECK_EQ(a->remove_len, static_cast<size_t>(1));   // only the "x"
    PB_CHECK(h.can_undo());
}

PB_TEST(a_slash_group_does_not_merge_with_earlier_typing) {
    EditHistory h = opened(u"");
    int64_t t = 0;
    std::u16string v = type(h, u"", u"hello ", &t, 50);
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(1));   // "hello " typed in one step
    h.begin_group(EditField::Text);
    h.observe(EditField::Text, v + u"/", t);
    h.observe(EditField::Text, v + u"```\n\n```", t + 10);
    h.end_group();
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(2));
    auto a = h.undo();
    PB_CHECK_EQ(a->remove_len, static_cast<size_t>(8));   // the inserted fence only, "hello " stays
    PB_CHECK(h.can_undo());
}

PB_TEST(undo_with_a_group_open_ends_the_group_first) {
    EditHistory h = opened(u"ab");
    h.begin_group(EditField::Text);
    h.observe(EditField::Text, u"ab/", 1000);
    auto a = h.undo();                            // the editor's long-press menu does this
    PB_CHECK(a.has_value());
    PB_CHECK(a->insert.empty());
    PB_CHECK_EQ(a->remove_len, static_cast<size_t>(1));
    h.finish_apply(u"T", u"ab");
    PB_CHECK(!h.can_undo());
    PB_CHECK(h.can_redo());
}

PB_TEST_MAIN()
