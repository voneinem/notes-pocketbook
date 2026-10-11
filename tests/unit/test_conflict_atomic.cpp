// T119: resolve_conflict is atomic — the local text is never in zero rows (Principle VI, SC-005).
#include <string>

#include "pbnotes/note_store.h"
#include "pbnotes/title.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::NoteStatus;
using pbnotes::NoteStore;

namespace {

constexpr int64_t kDay = 1791633600;  // 2026-10-10 12:00:00 UTC
const char* kLocal = "Plan\nmy careful local edit\nsecond line\n";

pbnotes::ApiNote server_version(const std::string& content = "Plan\nthe server's different edit\n") {
    pbnotes::ApiNote n;
    n.id = 7;
    n.etag = "server-v2";
    n.content = content;
    n.server_title = "Plan";
    n.category = "Work";
    n.favorite = true;
    n.modified = 900;
    return n;
}

// A synced note (server v1) that was then edited locally.
int64_t make_conflicting_note(NoteStore& store) {
    pbnotes::ApiNote v1 = server_version("Plan\noriginal\n");
    v1.etag = "server-v1";
    v1.modified = 100;
    store.apply_remote({v1});
    const int64_t id = store.get_by_remote_id(7)->local_id;
    store.save_local(id, pbnotes::derive_title(kLocal), kLocal, "Home", false, 500);
    return id;
}

bool local_text_exists(NoteStore& store, const std::string& text) {
    for (const auto& s : store.list())
        if (store.get(s.local_id)->content.find(text) != std::string::npos) return true;
    return false;
}

}  // namespace

PB_TEST(success_leaves_the_server_version_as_original_and_the_local_text_as_a_pending_copy) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    const int64_t id = make_conflicting_note(*store);
    auto res = store->resolve_conflict(id, server_version(), kDay);
    PB_CHECK(res.ok && res.created_copy);

    auto original = store->get(id);
    PB_CHECK(original->status == NoteStatus::Void);
    PB_CHECK_EQ(original->content, server_version().content);
    PB_CHECK_EQ(original->etag.value_or(""), std::string("server-v2"));
    PB_CHECK_EQ(original->modified, static_cast<int64_t>(900));
    PB_CHECK_EQ(original->category, std::string("Work"));
    PB_CHECK(original->favorite);

    auto copy = store->get(res.copy_local_id);
    PB_CHECK(copy->status == NoteStatus::Edited);
    PB_CHECK(!copy->remote_id.has_value());
    PB_CHECK(!copy->etag.has_value());
    PB_CHECK_EQ(copy->category, std::string("Home"));
    PB_CHECK(!copy->favorite);
    // the local text, byte for byte; the marker is in the title (spec 004)
    PB_CHECK_EQ(copy->content, std::string(kLocal));
    PB_CHECK_EQ(copy->title, std::string("Plan (conflict 2026-10-10)"));
}

PB_TEST(identical_versions_need_no_copy_and_just_become_void) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    const int64_t id = make_conflicting_note(*store);
    pbnotes::ApiNote same = server_version(kLocal);
    same.server_title = "Plan";
    same.category = "Home";
    same.favorite = false;
    auto res = store->resolve_conflict(id, same, kDay);
    PB_CHECK(res.ok);
    PB_CHECK(!res.created_copy);
    auto n = store->get(id);
    PB_CHECK(n->status == NoteStatus::Void);
    PB_CHECK_EQ(n->etag.value_or(""), std::string("server-v2"));  // adopts the server's validator and time
    PB_CHECK_EQ(n->modified, static_cast<int64_t>(900));
    PB_CHECK_EQ(store->list().size(), static_cast<size_t>(1));
}

PB_TEST(a_failure_at_any_statement_leaves_the_complete_before_or_after_state) {
    bool reached_success = false;
    for (int fail_at = 1; fail_at <= 20 && !reached_success; ++fail_at) {
        pbtest::TmpDir d;
        auto store = NoteStore::open(d.file("db"));
        const int64_t id = make_conflicting_note(*store);
        store->test_fail_nth_write(fail_at);
        auto res = store->resolve_conflict(id, server_version(), kDay);
        store->test_fail_nth_write(0);

        // the local text exists in at least one row, whatever happened
        PB_CHECK(local_text_exists(*store, "my careful local edit"));

        if (res.ok) {
            reached_success = true;  // no statement was left to fail: the "after" state
            PB_CHECK(res.created_copy);
            PB_CHECK_EQ(store->list().size(), static_cast<size_t>(2));
            PB_CHECK(store->get(id)->content == server_version().content);
        } else {
            // the complete "before" state: original still edited with the local text, no copy
            auto n = store->get(id);
            PB_CHECK(n->status == NoteStatus::Edited);
            PB_CHECK_EQ(n->content, std::string(kLocal));
            PB_CHECK_EQ(n->etag.value_or(""), std::string("server-v1"));
            PB_CHECK_EQ(store->list().size(), static_cast<size_t>(1));
        }
    }
    PB_CHECK(reached_success);
}

PB_TEST(rerunning_after_a_rollback_produces_exactly_one_copy) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    const int64_t id = make_conflicting_note(*store);
    store->test_fail_nth_write(2);
    PB_CHECK(!store->resolve_conflict(id, server_version(), kDay).ok);
    store->test_fail_nth_write(0);
    PB_CHECK(store->resolve_conflict(id, server_version(), kDay).created_copy);
    PB_CHECK_EQ(store->list().size(), static_cast<size_t>(2));
    // and a third attempt on the now-resolved note creates nothing more
    auto again = store->resolve_conflict(id, server_version(), kDay);
    PB_CHECK(!again.created_copy);
    PB_CHECK_EQ(store->list().size(), static_cast<size_t>(2));
}

PB_TEST(an_edit_made_before_the_transaction_ends_up_in_the_copy) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    const int64_t id = make_conflicting_note(*store);
    // the user keeps typing between the GET of the server version and the transaction
    store->save_local(id, pbnotes::derive_title("Plan\ntyped just now\n"), "Plan\ntyped just now\n", "Home", false, 600);
    auto res = store->resolve_conflict(id, server_version(), kDay);
    PB_CHECK(res.created_copy);
    PB_CHECK_EQ(store->get(res.copy_local_id)->content, std::string("Plan\ntyped just now\n"));
    PB_CHECK_EQ(store->get(res.copy_local_id)->title, std::string("Plan (conflict 2026-10-10)"));
}

PB_TEST(a_copy_that_cannot_be_verified_rolls_back_with_its_title_and_text) {
    // every injected failure keeps the local TITLE as well as the text in at least one row
    for (int fail_at = 1; fail_at <= 20; ++fail_at) {
        pbtest::TmpDir d;
        auto store = NoteStore::open(d.file("db"));
        const int64_t id = make_conflicting_note(*store);
        store->save_local(id, "My own title", kLocal, "Home", false, 550);
        store->test_fail_nth_write(fail_at);
        store->resolve_conflict(id, server_version(), kDay);
        store->test_fail_nth_write(0);
        bool title_kept = false;
        for (const auto& s : store->list())
            if (store->get(s.local_id)->title.rfind("My own title", 0) == 0) title_kept = true;
        PB_CHECK(title_kept);
    }
}

PB_TEST(conflicts_only_apply_to_pending_edited_notes) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({server_version("Plan\nsynced\n")});
    const int64_t id = store->get_by_remote_id(7)->local_id;  // void: nothing local to protect
    PB_CHECK(!store->resolve_conflict(id, server_version(), kDay).ok);
    PB_CHECK(!store->resolve_conflict(9999, server_version(), kDay).ok);
}

PB_TEST_MAIN()
