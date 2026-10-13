#pragma once
// Decides WHEN an automatic sync starts after a local save (spec 008). It holds no clock, no timer and no thread:
// the caller passes the time and the situation in and does what the answer says. Not thread-safe; use it from one
// thread (the bridge's UI thread). Qt-free (constitution Principle V).

#include <cstdint>

namespace pbnotes {

class SyncScheduler {
public:
    struct Options {
        int64_t quiet_ms = 4000;   // no sync until this long after the latest save (saves close together share one)
        int64_t gap_ms = 15000;    // no sync until this long after the latest sync of any kind started
    };
    struct Inputs {
        bool enabled = false;      // "Auto Sync" is on
        bool configured = false;   // an account exists
        bool running = false;      // a sync is running right now
        bool editor_open = false;  // an editor page is open
        bool has_pending = false;  // at least one note has changes the server has not confirmed
        bool online = false;       // Wi-Fi is connected
        bool can_connect = false;  // offline, but the caller can ask the device to connect (and has not yet for this save)
    };
    enum class Decision { Idle, Wait, Connect, Start };   // Connect: ask the device for Wi-Fi, then ask again
    struct Step {
        Decision decision = Decision::Idle;
        int64_t wait_ms = 0;       // for Wait only: ask again after this long (> 0)
        // For the log: "", "disabled", "no_account", "nothing_pending", "offline", "running", "editor_open", "waiting", "connecting".
        const char* reason = "";
    };

    SyncScheduler() = default;
    explicit SyncScheduler(Options options) : options_(options) {}

    // A local change was persisted: arm and remember when.
    void note_saved(int64_t now_ms);
    // ANY sync started (manual, at app start, automatic): it reads the pending list now, so a save made at or before
    // this instant is part of it and disarms; a save made after it stays armed.
    void sync_started(int64_t now_ms);
    // What to do now. Check order: armed, enabled, configured, running, editor_open, has_pending, the waits, online.
    // Drops (disarms) for: disabled, no_account, nothing_pending, offline (and no way to connect). Stays armed for:
    // running, editor_open, waiting, connecting.
    Step next(int64_t now_ms, const Inputs& in);
    bool armed() const { return armed_; }

private:
    Options options_;
    bool armed_ = false;
    int64_t last_save_ms_ = 0;
    bool have_start_ = false;
    int64_t last_start_ms_ = 0;
};

}  // namespace pbnotes
