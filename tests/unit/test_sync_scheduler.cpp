// Spec 008: SyncScheduler decides WHEN an automatic sync starts after a save. Pure arithmetic: the time and the
// situation are parameters, so every rule is checked here without a clock, a timer or a network.
#include <cstring>
#include <string>

#include "pbnotes/sync_scheduler.h"
#include "test_support.h"

using pbnotes::SyncScheduler;
using Decision = SyncScheduler::Decision;

namespace {
SyncScheduler::Inputs good() {
    SyncScheduler::Inputs in;
    in.enabled = true;
    in.configured = true;
    in.running = false;
    in.editor_open = false;
    in.has_pending = true;
    in.online = true;
    return in;
}
std::string reason(const SyncScheduler::Step& s) { return s.reason ? s.reason : ""; }
}  // namespace

PB_TEST(nothing_saved_means_nothing_to_do) {
    SyncScheduler s;
    PB_CHECK(!s.armed());
    PB_CHECK(s.next(0, good()).decision == Decision::Idle);
    PB_CHECK(s.next(100000, good()).decision == Decision::Idle);
}

PB_TEST(a_save_waits_for_the_quiet_period_then_starts) {
    SyncScheduler s;
    s.note_saved(0);
    PB_CHECK(s.armed());
    const auto wait = s.next(0, good());
    PB_CHECK(wait.decision == Decision::Wait);
    PB_CHECK_EQ(wait.wait_ms, static_cast<int64_t>(4000));
    PB_CHECK_EQ(reason(wait), std::string("waiting"));
    const auto later = s.next(1000, good());
    PB_CHECK(later.decision == Decision::Wait);
    PB_CHECK_EQ(later.wait_ms, static_cast<int64_t>(3000));
    PB_CHECK(s.next(4000, good()).decision == Decision::Start);
    PB_CHECK(s.armed());   // armed until the caller reports the start
}

PB_TEST(the_quiet_period_is_measured_from_the_latest_save) {
    SyncScheduler s;
    s.note_saved(0);
    s.note_saved(2000);
    const auto w = s.next(2000, good());
    PB_CHECK(w.decision == Decision::Wait);
    PB_CHECK_EQ(w.wait_ms, static_cast<int64_t>(4000));
    PB_CHECK(s.next(5999, good()).decision == Decision::Wait);
    PB_CHECK(s.next(6000, good()).decision == Decision::Start);
}

PB_TEST(a_sync_started_after_the_save_disarms) {
    SyncScheduler s;
    s.note_saved(1000);
    s.sync_started(1000);   // the same instant counts: the sync reads the list after the save
    PB_CHECK(!s.armed());
    PB_CHECK(s.next(60000, good()).decision == Decision::Idle);
    s.note_saved(2000);
    s.sync_started(3000);
    PB_CHECK(!s.armed());
}

PB_TEST(a_sync_started_before_the_save_leaves_it_armed) {
    SyncScheduler s;
    s.sync_started(1000);
    s.note_saved(2000);
    PB_CHECK(s.armed());
    s.note_saved(5000);
    s.sync_started(4000);   // started before the latest save: that change is not in it
    PB_CHECK(s.armed());
}

PB_TEST(the_gap_after_a_sync_start_delays_the_next_start) {
    SyncScheduler s;
    s.sync_started(0);
    s.note_saved(10000);
    // quiet period over at 14000, gap over at 15000: the longer remainder wins
    const auto at_save = s.next(10000, good());
    PB_CHECK(at_save.decision == Decision::Wait);
    PB_CHECK_EQ(at_save.wait_ms, static_cast<int64_t>(5000));   // gap: 15000 - 10000 > quiet: 4000
    const auto after_quiet = s.next(14000, good());
    PB_CHECK(after_quiet.decision == Decision::Wait);
    PB_CHECK_EQ(after_quiet.wait_ms, static_cast<int64_t>(1000));
    PB_CHECK(s.next(15000, good()).decision == Decision::Start);
}

PB_TEST(the_wait_is_the_longer_of_the_two_remainders) {
    SyncScheduler s;
    s.sync_started(0);
    s.note_saved(1000);
    const auto w = s.next(1000, good());   // quiet ends at 5000 (4000 left), gap ends at 15000 (14000 left)
    PB_CHECK_EQ(w.wait_ms, static_cast<int64_t>(14000));
    SyncScheduler t;
    t.sync_started(0);
    t.note_saved(14000);
    const auto w2 = t.next(14000, good());   // quiet 4000 left, gap 1000 left
    PB_CHECK_EQ(w2.wait_ms, static_cast<int64_t>(4000));
}

PB_TEST(a_disabled_switch_drops_the_pending_start) {
    SyncScheduler s;
    s.note_saved(0);
    auto in = good();
    in.enabled = false;
    const auto step = s.next(10000, in);
    PB_CHECK(step.decision == Decision::Idle);
    PB_CHECK_EQ(reason(step), std::string("disabled"));
    PB_CHECK(!s.armed());
    PB_CHECK(s.next(20000, good()).decision == Decision::Idle);   // switching on again does not bring it back
}

PB_TEST(no_account_drops_the_pending_start) {
    SyncScheduler s;
    s.note_saved(0);
    auto in = good();
    in.configured = false;
    const auto step = s.next(10000, in);
    PB_CHECK(step.decision == Decision::Idle);
    PB_CHECK_EQ(reason(step), std::string("no_account"));
    PB_CHECK(!s.armed());
}

PB_TEST(a_running_sync_defers_and_the_save_stays_armed_for_one_follow_up) {
    SyncScheduler s;
    s.note_saved(0);
    auto in = good();
    in.running = true;
    const auto step = s.next(10000, in);
    PB_CHECK(step.decision == Decision::Idle);
    PB_CHECK_EQ(reason(step), std::string("running"));
    PB_CHECK(s.armed());
    in.running = false;   // the sync ended; the caller asks again
    PB_CHECK(s.next(10000, in).decision == Decision::Start);
}

PB_TEST(an_open_editor_defers_and_the_save_stays_armed) {
    SyncScheduler s;
    s.note_saved(0);
    auto in = good();
    in.editor_open = true;
    const auto step = s.next(10000, in);
    PB_CHECK(step.decision == Decision::Idle);
    PB_CHECK_EQ(reason(step), std::string("editor_open"));
    PB_CHECK(s.armed());
    in.editor_open = false;
    PB_CHECK(s.next(10000, in).decision == Decision::Start);
}

PB_TEST(nothing_pending_drops_the_start) {
    SyncScheduler s;
    s.note_saved(0);
    auto in = good();
    in.has_pending = false;
    const auto step = s.next(10000, in);
    PB_CHECK(step.decision == Decision::Idle);
    PB_CHECK_EQ(reason(step), std::string("nothing_pending"));
    PB_CHECK(!s.armed());
}

PB_TEST(offline_after_the_waits_drops_the_start_and_never_retries) {
    SyncScheduler s;
    s.note_saved(0);
    auto in = good();
    in.online = false;
    PB_CHECK(s.next(1000, in).decision == Decision::Wait);   // waits are served first
    const auto step = s.next(4000, in);
    PB_CHECK(step.decision == Decision::Idle);
    PB_CHECK_EQ(reason(step), std::string("offline"));
    PB_CHECK(!s.armed());
    PB_CHECK(s.next(60000, good()).decision == Decision::Idle);   // Wi-Fi coming back does not start anything
}

PB_TEST(offline_with_a_way_to_connect_asks_for_a_connection_and_stays_armed) {
    SyncScheduler s;
    s.note_saved(0);
    auto in = good();
    in.online = false;
    in.can_connect = true;
    PB_CHECK(s.next(1000, in).decision == Decision::Wait);   // waits are served first
    const auto step = s.next(4000, in);
    PB_CHECK(step.decision == Decision::Connect);
    PB_CHECK_EQ(reason(step), std::string("connecting"));
    PB_CHECK(s.armed());
    in.online = true;                                         // the connection came up: now the sync starts
    PB_CHECK(s.next(4500, in).decision == Decision::Start);
}

PB_TEST(after_a_failed_connection_the_start_is_dropped_like_any_offline_start) {
    SyncScheduler s;
    s.note_saved(0);
    auto in = good();
    in.online = false;
    in.can_connect = false;   // the caller already tried once for this save
    const auto step = s.next(4000, in);
    PB_CHECK(step.decision == Decision::Idle);
    PB_CHECK_EQ(reason(step), std::string("offline"));
    PB_CHECK(!s.armed());
}

PB_TEST(the_checks_run_in_the_documented_order) {
    SyncScheduler s;
    s.note_saved(0);
    auto in = good();
    in.enabled = false;
    in.configured = false;
    in.running = true;
    in.editor_open = true;
    in.has_pending = false;
    in.online = false;
    PB_CHECK_EQ(reason(s.next(0, in)), std::string("disabled"));

    SyncScheduler t;
    t.note_saved(0);
    in.enabled = true;
    PB_CHECK_EQ(reason(t.next(0, in)), std::string("no_account"));

    SyncScheduler u;
    u.note_saved(0);
    in.configured = true;
    PB_CHECK_EQ(reason(u.next(0, in)), std::string("running"));

    in.running = false;
    PB_CHECK_EQ(reason(u.next(0, in)), std::string("editor_open"));
    in.editor_open = false;
    PB_CHECK_EQ(reason(u.next(0, in)), std::string("nothing_pending"));

    SyncScheduler v;
    v.note_saved(0);
    in.has_pending = true;
    PB_CHECK_EQ(reason(v.next(0, in)), std::string("waiting"));   // waits come before the Wi-Fi check
    PB_CHECK_EQ(reason(v.next(4000, in)), std::string("offline"));
}

PB_TEST(the_quiet_period_and_the_gap_come_from_the_options) {
    SyncScheduler::Options o;
    o.quiet_ms = 50;
    o.gap_ms = 200;
    SyncScheduler s(o);
    s.sync_started(0);
    s.note_saved(10);
    const auto w = s.next(10, good());
    PB_CHECK(w.decision == Decision::Wait);
    PB_CHECK_EQ(w.wait_ms, static_cast<int64_t>(190));
    PB_CHECK(s.next(200, good()).decision == Decision::Start);

    SyncScheduler::Options z;
    z.quiet_ms = 0;
    z.gap_ms = 0;
    SyncScheduler zero(z);
    zero.note_saved(5);
    PB_CHECK(zero.next(5, good()).decision == Decision::Start);   // a zero wait is a start, never Wait(0)
}

PB_TEST(a_wait_is_never_zero_or_negative) {
    SyncScheduler s;
    s.note_saved(0);
    for (int64_t t = 0; t < 20000; t += 7) {
        const auto step = s.next(t, good());
        if (step.decision == Decision::Wait) PB_CHECK(step.wait_ms > 0);
        if (step.decision == Decision::Start) break;
    }
}

PB_TEST(ten_saves_six_seconds_apart_cause_at_most_five_syncs) {
    SyncScheduler s;
    int starts = 0;
    int saves = 0;
    for (int64_t t = 0; t <= 120000; t += 1000) {
        if (t % 6000 == 0 && saves < 10) {
            s.note_saved(t);
            ++saves;
        }
        const auto step = s.next(t, good());
        if (step.decision == Decision::Start) {
            ++starts;
            s.sync_started(t);
        }
    }
    PB_CHECK_EQ(saves, 10);
    PB_CHECK(starts >= 1);
    PB_CHECK(starts <= 5);
    PB_CHECK(!s.armed());   // everything saved has been sent
}

PB_TEST(a_save_during_a_sync_is_sent_by_exactly_one_follow_up) {
    SyncScheduler s;
    s.note_saved(0);
    PB_CHECK(s.next(4000, good()).decision == Decision::Start);
    s.sync_started(4000);
    PB_CHECK(!s.armed());
    s.note_saved(6000);   // saved while that sync runs
    auto in = good();
    in.running = true;
    PB_CHECK_EQ(reason(s.next(9000, in)), std::string("running"));
    in.running = false;   // sync ended at 10000
    const auto after = s.next(10000, in);   // gap: 4000 + 15000 = 19000
    PB_CHECK(after.decision == Decision::Wait);
    PB_CHECK_EQ(after.wait_ms, static_cast<int64_t>(9000));
    PB_CHECK(s.next(19000, in).decision == Decision::Start);
    s.sync_started(19000);
    PB_CHECK(!s.armed());
    PB_CHECK(s.next(60000, in).decision == Decision::Idle);   // exactly one follow-up
}

PB_TEST_MAIN()
