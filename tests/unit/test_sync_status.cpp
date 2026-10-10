// T070: the status a user sees after a sync: state, time and counts (persisted across restarts).
#include <string>

#include "pbnotes/note_store.h"
#include "pbnotes/sync_status.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::Account;
using pbnotes::SyncOutcome;
using pbnotes::SyncResult;

namespace {
Account fresh_account() {
    Account a;
    a.server_url = "https://cloud.example.com";
    a.username = "alice";
    a.password = "pw";
    return a;
}
}  // namespace

PB_TEST(a_new_account_has_never_been_synced) {
    auto st = pbnotes::sync_status(fresh_account(), false);
    PB_CHECK_EQ(st.state, std::string("never"));
    PB_CHECK(!st.at.has_value());
    PB_CHECK_EQ(st.counts.pushed, 0);
}

PB_TEST(a_running_sync_overrides_the_stored_state) {
    Account a = fresh_account();
    a.last_sync_outcome = SyncOutcome::Success;
    a.last_sync_at = 1700000000;
    PB_CHECK_EQ(pbnotes::sync_status(a, true).state, std::string("running"));
    PB_CHECK_EQ(pbnotes::sync_status(a, false).state, std::string("success"));
}

PB_TEST(every_outcome_is_stored_and_reported_with_time_and_counts) {
    pbtest::TmpDir d;
    auto store = pbnotes::NoteStore::open(d.file("db"));
    store->save_account(fresh_account());
    const struct {
        SyncOutcome outcome;
        const char* state;
    } all[] = {{SyncOutcome::Success, "success"}, {SyncOutcome::Partial, "partial"}, {SyncOutcome::Failed, "failed"},
               {SyncOutcome::Offline, "offline"}, {SyncOutcome::Maintenance, "maintenance"}};
    int64_t t = 1700000100;
    for (const auto& c : all) {
        SyncResult r;
        r.outcome = c.outcome;
        r.message_key = "msg_" + std::string(c.state);
        r.counts.pushed = 2;
        r.counts.pulled = 3;
        r.counts.deleted = 1;
        r.counts.conflicts = 4;
        PB_CHECK(store->record_sync_result(r, ++t));
        auto st = pbnotes::sync_status(*store->get_account(), false);
        PB_CHECK_EQ(st.state, std::string(c.state));
        PB_CHECK_EQ(st.message_key, "msg_" + std::string(c.state));
        PB_CHECK_EQ(st.at.value_or(0), t);
        PB_CHECK_EQ(st.counts.pushed, 2);
        PB_CHECK_EQ(st.counts.pulled, 3);
        PB_CHECK_EQ(st.counts.deleted, 1);
        PB_CHECK_EQ(st.counts.conflicts, 4);
    }
}

PB_TEST(the_status_survives_a_restart) {
    pbtest::TmpDir d;
    {
        auto store = pbnotes::NoteStore::open(d.file("db"));
        store->save_account(fresh_account());
        SyncResult r;
        r.outcome = SyncOutcome::Partial;
        r.counts.recreated = 1;
        r.counts.pushed = 5;
        store->record_sync_result(r, 1700000999);
    }
    auto store = pbnotes::NoteStore::open(d.file("db"));
    auto st = pbnotes::sync_status(*store->get_account(), false);
    PB_CHECK_EQ(st.state, std::string("partial"));
    PB_CHECK_EQ(st.at.value_or(0), static_cast<int64_t>(1700000999));
    PB_CHECK_EQ(st.counts.pushed, 5);
    PB_CHECK_EQ(st.counts.recreated, 1);
}

PB_TEST_MAIN()
