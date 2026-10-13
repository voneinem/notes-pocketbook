// T027: single account row; "Forget account" leaves no password anywhere on disk; the logger
// never sees the password.
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "pbnotes/log.h"
#include "pbnotes/note_store.h"
#include "pbnotes/title.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::Account;
using pbnotes::NoteStore;

namespace {
Account make_account(const std::string& password) {
    Account a;
    a.server_url = "https://cloud.example.com";
    a.username = "alice";
    a.password = password;
    return a;
}
bool file_contains(const std::string& path, const std::string& needle) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    const std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return data.find(needle) != std::string::npos;
}
}  // namespace

PB_TEST(no_account_before_setup) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("pbnotes.db"));
    PB_CHECK(!store->get_account().has_value());
}

PB_TEST(save_and_get_single_account_row) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("pbnotes.db"));
    PB_CHECK(store->save_account(make_account("pw-one")));
    auto a = store->get_account();
    PB_CHECK(a.has_value());
    PB_CHECK_EQ(a->server_url, std::string("https://cloud.example.com"));
    PB_CHECK_EQ(a->username, std::string("alice"));
    PB_CHECK_EQ(a->password, std::string("pw-one"));
    PB_CHECK(a->last_sync_outcome == pbnotes::SyncOutcome::Never);

    Account second = make_account("pw-two");
    second.server_url = "https://other.example.com";
    PB_CHECK(store->save_account(second));  // replaces, never adds a second account
    PB_CHECK_EQ(store->get_account()->server_url, std::string("https://other.example.com"));
    PB_CHECK_EQ(store->get_account()->password, std::string("pw-two"));
    PB_CHECK_EQ(store->account_count(), 1);
}

PB_TEST(save_account_stores_the_sync_bookkeeping_fields_too) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("pbnotes.db"));
    Account a = make_account("pw");
    a.collection_etag = std::string("etag-1");
    a.collection_last_modified = 1700000000;
    PB_CHECK(store->save_account(a));
    PB_CHECK_EQ(store->get_account()->collection_etag.value_or(""), std::string("etag-1"));
    PB_CHECK_EQ(store->get_account()->collection_last_modified, static_cast<int64_t>(1700000000));
}

PB_TEST(forget_account_removes_it_and_leaves_no_trace_of_the_password) {
    pbtest::TmpDir d;
    const std::string password = "hunter2-SECRET-app-password";
    {
        auto store = NoteStore::open(d.file("pbnotes.db"));
        PB_CHECK(store->save_account(make_account(password)));
        PB_CHECK(file_contains(d.file("pbnotes.db"), password));  // sanity: the scan can see it
        PB_CHECK(store->forget_account());
        PB_CHECK(!store->get_account().has_value());
    }
    PB_CHECK(!file_contains(d.file("pbnotes.db"), password));
    PB_CHECK(!file_contains(d.file("pbnotes.db-journal"), password));
    PB_CHECK(!file_contains(d.file("pbnotes.db.bak"), password));
}

PB_TEST(forget_account_keeps_the_sync_after_saving_choice) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("pbnotes.db"));
    PB_CHECK(store->save_account(make_account("pw")));
    PB_CHECK(store->set_setting("sync_after_save", "on"));
    PB_CHECK(store->forget_account());
    PB_CHECK_EQ(store->get_setting("sync_after_save"), std::string("on"));
}

PB_TEST(forget_account_keeps_the_file_browser_layout) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("pbnotes.db"));
    PB_CHECK(store->save_account(make_account("pw")));
    PB_CHECK(store->set_setting("picker_view", "tile"));
    PB_CHECK(store->forget_account());
    PB_CHECK_EQ(store->get_setting("picker_view"), std::string("tile"));
}

PB_TEST(forget_account_keeps_the_note_list_layout) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("pbnotes.db"));
    PB_CHECK(store->save_account(make_account("pw")));
    PB_CHECK(store->set_setting("list_layout", "tile"));
    PB_CHECK(store->forget_account());
    PB_CHECK_EQ(store->get_setting("list_layout"), std::string("tile"));   // spec 010
}

PB_TEST(setup_draft_survives_a_restart_and_is_cleared_without_a_trace) {
    pbtest::TmpDir d;
    const std::string password = "draft-password-DRAFT-9981";
    {
        auto store = NoteStore::open(d.file("pbnotes.db"));
        PB_CHECK(!store->get_setup_draft().has_value());
        PB_CHECK(store->save_setup_draft({"https://cloud.example.com/very/long", "alice", password}));
    }
    {
        auto store = NoteStore::open(d.file("pbnotes.db"));
        auto draft = store->get_setup_draft();
        PB_CHECK(draft.has_value());
        PB_CHECK_EQ(draft->url, std::string("https://cloud.example.com/very/long"));
        PB_CHECK_EQ(draft->username, std::string("alice"));
        PB_CHECK_EQ(draft->password, password);
        PB_CHECK(file_contains(d.file("pbnotes.db"), password));  // sanity
        PB_CHECK(store->clear_setup_draft());
        PB_CHECK(!store->get_setup_draft().has_value());
    }
    PB_CHECK(!file_contains(d.file("pbnotes.db"), password));
    PB_CHECK(!file_contains(d.file("pbnotes.db-journal"), password));
}

PB_TEST(an_empty_setup_draft_is_not_stored) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("pbnotes.db"));
    PB_CHECK(store->save_setup_draft({"", "", ""}));
    PB_CHECK(!store->get_setup_draft().has_value());
}

PB_TEST(forgetting_the_account_also_clears_a_setup_draft) {
    pbtest::TmpDir d;
    const std::string password = "draft-password-FORGET-4471";
    {
        auto store = NoteStore::open(d.file("pbnotes.db"));
        PB_CHECK(store->save_account(make_account("account-pw")));
        PB_CHECK(store->save_setup_draft({"https://x.example.com", "bob", password}));
        PB_CHECK(store->forget_account());
        PB_CHECK(!store->get_setup_draft().has_value());
    }
    PB_CHECK(!file_contains(d.file("pbnotes.db"), password));
}

PB_TEST(a_password_change_leaves_no_trace_of_the_old_password) {
    pbtest::TmpDir d;
    const std::string old_pw = "old-password-OLD-12345";
    {
        auto store = NoteStore::open(d.file("pbnotes.db"));
        PB_CHECK(store->save_account(make_account(old_pw)));
        PB_CHECK(store->save_account(make_account("new-password-NEW-67890")));
    }
    PB_CHECK(!file_contains(d.file("pbnotes.db"), old_pw));
    PB_CHECK(!file_contains(d.file("pbnotes.db-journal"), old_pw));
    PB_CHECK(file_contains(d.file("pbnotes.db"), "new-password-NEW-67890"));
}

PB_TEST(the_password_never_reaches_the_log) {
    pbtest::TmpDir d;
    std::vector<std::string> lines;
    pbnotes::log::set_sink([&](const std::string& l) { lines.push_back(l); });
    {
        auto store = NoteStore::open(d.file("pbnotes.db"));
        store->save_account(make_account("log-me-not-PASSWORD"));
        store->forget_account();
    }
    pbnotes::log::reset_sink();
    for (const auto& l : lines) {
        PB_CHECK(l.find("log-me-not-PASSWORD") == std::string::npos);
        PB_CHECK(l.find("alice") == std::string::npos);
    }
}

PB_TEST(reset_local_data_removes_synced_notes_but_refuses_while_changes_are_pending) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("pbnotes.db"));
    pbnotes::ApiNote n;
    n.id = 1;
    n.etag = "e";
    n.content = "Synced\nbody";
    n.modified = 5;
    store->apply_remote({n});
    PB_CHECK(store->reset_local_data());              // everything is in sync: allowed
    PB_CHECK(store->list().empty());

    store->apply_remote({n});
    store->create_local(pbnotes::derive_title("Not yet synced\nbody"), "Not yet synced\nbody", "", false, 10);
    PB_CHECK(!store->reset_local_data());             // a pending note would be lost: refused
    PB_CHECK_EQ(store->list().size(), static_cast<size_t>(2));   // nothing was deleted

    auto pending = store->pending_notes();
    store->mark_deleted(store->get_by_remote_id(1)->local_id);   // a pending delete counts as pending too
    PB_CHECK(!store->reset_local_data());
    (void)pending;
}

PB_TEST_MAIN()
