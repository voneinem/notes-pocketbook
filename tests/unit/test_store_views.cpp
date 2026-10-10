// T085: list views (All, Favorites, Uncategorized, by category), sort orders, category list.
#include <string>
#include <vector>

#include "pbnotes/note_store.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::ListQuery;
using pbnotes::NoteStore;

namespace {

std::vector<std::string> titles(const std::vector<pbnotes::NoteSummary>& v) {
    std::vector<std::string> t;
    for (const auto& s : v) t.push_back(s.title);
    return t;
}

struct Seeded {
    pbtest::TmpDir dir;
    std::unique_ptr<NoteStore> store;
    Seeded() {
        store = NoteStore::open(dir.file("db"));
        store->create_local("Apple pie\nrecipe", "Food", true, 100);
        store->create_local("banana bread\nrecipe", "Food", false, 300);
        store->create_local("Cherry\nno category", "", true, 200);
        store->create_local("Dates/Fig\nsub category", "Food/Fruit", false, 50);
        // a synced note that was deleted locally must never be listed
        pbnotes::ApiNote gone;
        gone.id = 9;
        gone.etag = "e";
        gone.content = "Zebra\nremoved";
        gone.category = "Zoo";
        gone.favorite = true;
        gone.modified = 400;
        store->apply_remote({gone});
        store->mark_deleted(store->get_by_remote_id(9)->local_id);
    }
};

}  // namespace

PB_TEST(all_view_lists_everything_newest_first) {
    Seeded s;
    PB_CHECK(titles(s.store->list(ListQuery())) ==
             (std::vector<std::string>{"banana bread", "Cherry", "Apple pie", "Dates/Fig"}));
    PB_CHECK_EQ(s.store->list().size(), static_cast<size_t>(4));  // the default query is the same
}

PB_TEST(favorites_uncategorized_and_category_views) {
    Seeded s;
    ListQuery q;
    q.view = "favorites";
    PB_CHECK(titles(s.store->list(q)) == (std::vector<std::string>{"Cherry", "Apple pie"}));
    q.view = "uncategorized";
    PB_CHECK(titles(s.store->list(q)) == (std::vector<std::string>{"Cherry"}));
    q.view = "category:Food";
    PB_CHECK(titles(s.store->list(q)) == (std::vector<std::string>{"banana bread", "Apple pie"}));  // exact match only
    q.view = "category:Food/Fruit";
    PB_CHECK(titles(s.store->list(q)) == (std::vector<std::string>{"Dates/Fig"}));
    q.view = "category:Nope";
    PB_CHECK(s.store->list(q).empty());
}

PB_TEST(sort_by_title_is_alphabetical_ignoring_case) {
    Seeded s;
    ListQuery q;
    q.sort = "title";
    PB_CHECK(titles(s.store->list(q)) ==
             (std::vector<std::string>{"Apple pie", "banana bread", "Cherry", "Dates/Fig"}));
    q.view = "favorites";
    PB_CHECK(titles(s.store->list(q)) == (std::vector<std::string>{"Apple pie", "Cherry"}));
}

PB_TEST(unknown_view_or_sort_fall_back_to_the_defaults) {
    Seeded s;
    ListQuery q;
    q.view = "bogus";
    q.sort = "bogus";
    PB_CHECK_EQ(s.store->list(q).size(), static_cast<size_t>(4));
    PB_CHECK_EQ(s.store->list(q).front().title, std::string("banana bread"));
}

PB_TEST(summaries_carry_what_the_list_shows) {
    Seeded s;
    auto all = s.store->list();
    const auto& cherry = all[1];
    PB_CHECK_EQ(cherry.title, std::string("Cherry"));
    PB_CHECK(cherry.favorite);
    PB_CHECK_EQ(cherry.category, std::string(""));
    PB_CHECK_EQ(cherry.modified, static_cast<int64_t>(200));
    PB_CHECK(cherry.pending);
    PB_CHECK_EQ(cherry.excerpt, std::string("no category"));
}

PB_TEST(categories_are_listed_with_counts_without_the_empty_one_or_deleted_notes) {
    Seeded s;
    auto cats = s.store->categories();
    PB_CHECK_EQ(cats.size(), static_cast<size_t>(2));
    PB_CHECK_EQ(cats[0].name, std::string("Food"));
    PB_CHECK_EQ(cats[0].count, 2);
    PB_CHECK_EQ(cats[1].name, std::string("Food/Fruit"));
    PB_CHECK_EQ(cats[1].count, 1);
}

PB_TEST(a_category_disappears_with_its_last_note) {
    Seeded s;
    auto fruit = s.store->list(ListQuery{"category:Food/Fruit", "modified", ""});
    s.store->set_category(fruit[0].local_id, "", 500);
    for (const auto& c : s.store->categories()) PB_CHECK(c.name != "Food/Fruit");
}

PB_TEST(view_and_sort_choices_persist_in_the_settings) {
    pbtest::TmpDir d;
    {
        auto store = NoteStore::open(d.file("db"));
        PB_CHECK(store->set_setting("view", "category:Food"));
        PB_CHECK(store->set_setting("sort_order", "title"));
    }
    auto store = NoteStore::open(d.file("db"));
    PB_CHECK_EQ(store->get_setting("view"), std::string("category:Food"));
    PB_CHECK_EQ(store->get_setting("sort_order"), std::string("title"));
}

PB_TEST_MAIN()
