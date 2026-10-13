#include "pbnotes/sync_scheduler.h"

#include <algorithm>

namespace pbnotes {

void SyncScheduler::note_saved(int64_t now_ms) {
    armed_ = true;
    last_save_ms_ = now_ms;
}

void SyncScheduler::sync_started(int64_t now_ms) {
    have_start_ = true;
    last_start_ms_ = now_ms;
    if (armed_ && last_save_ms_ <= now_ms) armed_ = false;
}

SyncScheduler::Step SyncScheduler::next(int64_t now_ms, const Inputs& in) {
    Step step;
    if (!armed_) return step;
    auto drop = [&](const char* why) {
        armed_ = false;
        step.reason = why;
        return step;
    };
    if (!in.enabled) return drop("disabled");
    if (!in.configured) return drop("no_account");
    if (in.running) {
        step.reason = "running";   // armed stays: the caller asks again when that sync ends
        return step;
    }
    if (in.editor_open) {
        step.reason = "editor_open";
        return step;
    }
    if (!in.has_pending) return drop("nothing_pending");

    int64_t wait = last_save_ms_ + options_.quiet_ms - now_ms;
    if (have_start_) wait = std::max(wait, last_start_ms_ + options_.gap_ms - now_ms);
    if (wait > 0) {
        step.decision = Decision::Wait;
        step.wait_ms = wait;
        step.reason = "waiting";
        return step;
    }
    if (!in.online) {
        if (!in.can_connect) return drop("offline");
        step.decision = Decision::Connect;
        step.reason = "connecting";   // armed stays: the caller asks again when the connection is up or has failed
        return step;
    }
    step.decision = Decision::Start;
    return step;
}

}  // namespace pbnotes
