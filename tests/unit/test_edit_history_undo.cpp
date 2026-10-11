// Spec 005 FR-012..FR-017: undo and redo over title, text, category and favorite; compound commands as one step;
// the applying state; the opened content is never undone.
#include <string>

#include "pbnotes/edit_history.h"
#include "test_support.h"

using pbnotes::EditApplied;
using pbnotes::EditField;
using pbnotes::EditHistory;

namespace {
EditHistory opened(const std::u16string& title = u"T", const std::u16string& text = u"body",
                   const std::u16string& category = u"Cat", bool favorite = false) {
    EditHistory h;
    h.reset(title, text, category, favorite);
    return h;
}
}  // namespace

PB_TEST(a_group_is_one_step) {
    EditHistory h = opened(u"T", u"hello world");
    h.begin_group(EditField::Text);
    h.observe(EditField::Text, u"hello ", 0);        // cut: remove the selection ...
    h.observe(EditField::Text, u"hello there", 1);   // ... then paste over it
    h.end_group();
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(1));
    auto a = h.undo();
    PB_CHECK(a->field == EditField::Text);
    PB_CHECK_EQ(a->pos, static_cast<size_t>(6));
    PB_CHECK(a->insert == u"world");
    PB_CHECK_EQ(a->remove_len, static_cast<size_t>(5));
}

PB_TEST(a_group_without_a_net_change_pushes_nothing) {
    EditHistory h = opened(u"T", u"abc");
    h.begin_group(EditField::Text);
    h.observe(EditField::Text, u"ab", 0);
    h.observe(EditField::Text, u"abc", 1);
    h.end_group();
    PB_CHECK(!h.can_undo());
}

PB_TEST(a_second_begin_ends_the_first_group) {
    EditHistory h = opened(u"T", u"abc");
    h.begin_group(EditField::Text);
    h.observe(EditField::Text, u"abcd", 0);
    h.begin_group(EditField::Title);
    h.observe(EditField::Title, u"Tx", 1);
    h.end_group();
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(2));
}

PB_TEST(a_group_step_never_merges_with_typing) {
    EditHistory h = opened(u"T", u"a");
    h.begin_group(EditField::Text);
    h.observe(EditField::Text, u"ab", 0);
    h.end_group();
    h.observe(EditField::Text, u"abc", 10);
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(2));
}

PB_TEST(undo_and_redo_run_in_reverse_order_across_all_fields) {
    EditHistory h = opened(u"T", u"body", u"Cat", false);
    h.observe(EditField::Title, u"T2", 0);
    h.observe(EditField::Text, u"body!", 10);
    h.observe(EditField::Category, u"Work", 20);
    h.observe(EditField::Favorite, u"1", 30);
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(4));

    auto u = h.undo();
    PB_CHECK(u->field == EditField::Favorite);
    PB_CHECK(u->value == u"");
    h.finish_apply(u"T2", u"body!");
    u = h.undo();
    PB_CHECK(u->field == EditField::Category);
    PB_CHECK(u->value == u"Cat");
    h.finish_apply(u"T2", u"body!");
    u = h.undo();
    PB_CHECK(u->field == EditField::Text);
    h.finish_apply(u"T2", u"body");
    u = h.undo();
    PB_CHECK(u->field == EditField::Title);
    h.finish_apply(u"T", u"body");
    PB_CHECK(!h.can_undo());
    PB_CHECK_EQ(h.redo_size(), static_cast<size_t>(4));

    auto r = h.redo();
    PB_CHECK(r->field == EditField::Title);
    h.finish_apply(u"T2", u"body");
    r = h.redo();
    PB_CHECK(r->field == EditField::Text);
    h.finish_apply(u"T2", u"body!");
    r = h.redo();
    PB_CHECK(r->field == EditField::Category);
    PB_CHECK(r->value == u"Work");
    h.finish_apply(u"T2", u"body!");
    r = h.redo();
    PB_CHECK(r->field == EditField::Favorite);
    PB_CHECK(r->value == u"1");
    PB_CHECK(!h.can_redo());
}

PB_TEST(observations_are_ignored_while_applying) {
    EditHistory h = opened(u"T", u"abc");
    h.observe(EditField::Text, u"abcd", 0);
    auto u = h.undo();
    PB_CHECK(u.has_value());
    h.observe(EditField::Text, u"ab", 1);   // an intermediate state of the editor (remove then insert)
    h.observe(EditField::Text, u"abc", 2);
    h.finish_apply(u"T", u"abc");
    h.observe(EditField::Text, u"abc", 3);  // the same value: no step
    PB_CHECK(!h.can_undo());
    PB_CHECK(h.can_redo());
}

PB_TEST(finish_apply_resyncs_the_known_values_and_keeps_the_stacks) {
    EditHistory h = opened(u"T", u"abc");
    h.observe(EditField::Text, u"abcd", 0);
    h.undo();
    h.finish_apply(u"T", u"abX");   // the editor ended up holding something else
    PB_CHECK(h.can_redo());
    h.observe(EditField::Text, u"abX", 1);  // matches what was synced: nothing recorded
    PB_CHECK(!h.can_undo());
}

PB_TEST(a_new_edit_after_undo_clears_the_redo_stack) {
    EditHistory h = opened(u"T", u"abc");
    h.observe(EditField::Text, u"abcd", 0);
    h.undo();
    h.finish_apply(u"T", u"abc");
    PB_CHECK(h.can_redo());
    h.observe(EditField::Text, u"abcX", 10);
    PB_CHECK(!h.can_redo());
}

PB_TEST(the_opened_content_is_never_undone) {
    EditHistory h = opened(u"Title", u"body", u"Cat", true);
    h.observe(EditField::Text, u"body+", 0);
    h.observe(EditField::Category, u"", 10);
    h.undo(); h.finish_apply(u"Title", u"body+");
    h.undo(); h.finish_apply(u"Title", u"body");
    PB_CHECK(!h.can_undo());
    PB_CHECK(!h.undo().has_value());
}

PB_TEST(undo_of_a_removal_selects_the_restored_text) {
    EditHistory h = opened(u"T", u"hello world");
    h.observe(EditField::Text, u"hello ", 0);  // cut "world"
    auto a = h.undo();
    PB_CHECK_EQ(a->pos, static_cast<size_t>(6));
    PB_CHECK_EQ(a->sel_begin, static_cast<size_t>(6));
    PB_CHECK_EQ(a->sel_end, static_cast<size_t>(11));
    h.finish_apply(u"T", u"hello world");
    auto r = h.redo();  // redo of a removal: caret where it was
    PB_CHECK_EQ(r->pos, static_cast<size_t>(6));
    PB_CHECK_EQ(r->remove_len, static_cast<size_t>(5));
    PB_CHECK(r->insert.empty());
    PB_CHECK_EQ(r->sel_begin, static_cast<size_t>(6));
    PB_CHECK_EQ(r->sel_end, static_cast<size_t>(6));
}

PB_TEST(undo_of_an_insertion_puts_the_caret_at_its_start_and_redo_after_it) {
    EditHistory h = opened(u"T", u"ab");
    h.observe(EditField::Text, u"aXYb", 0);
    auto u = h.undo();
    PB_CHECK_EQ(u->pos, static_cast<size_t>(1));
    PB_CHECK_EQ(u->remove_len, static_cast<size_t>(2));
    PB_CHECK(u->insert.empty());
    PB_CHECK_EQ(u->sel_begin, static_cast<size_t>(1));
    PB_CHECK_EQ(u->sel_end, static_cast<size_t>(1));
    h.finish_apply(u"T", u"ab");
    auto r = h.redo();
    PB_CHECK(r->insert == u"XY");
    PB_CHECK_EQ(r->sel_begin, static_cast<size_t>(3));
    PB_CHECK_EQ(r->sel_end, static_cast<size_t>(3));
}

PB_TEST(reset_clears_everything) {
    EditHistory h = opened(u"T", u"abc");
    h.observe(EditField::Text, u"abcd", 0);
    h.undo();
    h.begin_group(EditField::Text);
    h.reset(u"T", u"new", u"", false);
    PB_CHECK(!h.can_undo());
    PB_CHECK(!h.can_redo());
    h.observe(EditField::Text, u"newX", 0);  // not applying, not in a group: recorded
    PB_CHECK_EQ(h.undo_size(), static_cast<size_t>(1));
}

PB_TEST(a_favorite_toggle_round_trips) {
    EditHistory h = opened(u"T", u"b", u"", true);
    h.observe(EditField::Favorite, u"", 0);  // turned off
    PB_CHECK(h.undo()->value == u"1");
}

PB_TEST_MAIN()
