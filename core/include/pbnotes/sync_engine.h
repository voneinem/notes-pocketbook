#pragma once
// Sync between the local store and the Notes server. One run = push local changes first, then
// pull remote changes (the push half arrives with User Story 4). Blocking; one run at a time.
// No Qt.

#include <atomic>
#include <cstdint>
#include <functional>

#include "pbnotes/http_transport.h"
#include "pbnotes/note_store.h"
#include "pbnotes/types.h"

namespace pbnotes {

struct SyncOptions {
    // Send DELETE with If-Match instead of the conditional GET + DELETE sequence. Only to be
    // enabled once the server is verified to answer 412 for a stale If-Match on DELETE (T022).
    bool delete_uses_if_match = false;
    // The folder of the attachment files (AttachmentService). Needed to upload pending attachments; without it
    // they stay pending (with the reason "unreadable") and the notes are still pushed without their tokens.
    std::string attachments_dir;
};

class SyncEngine {
public:
    using Clock = std::function<int64_t()>;  // epoch seconds

    using Options = SyncOptions;

    SyncEngine(NoteStore& store, HttpTransport& transport, Clock clock = nullptr, Options options = Options());

    // Runs one sync and records its outcome on the account. A second call while a run is in
    // flight returns immediately with outcome Running / message_key "already_running" and does
    // not touch the store or the network.
    //
    // Outcomes: Success (incl. "up_to_date" on 304); Offline (no network); Maintenance (503);
    // Failed with error_class Auth / Tls / Server / Unsupported. 304, 503 and "offline" never
    // alter notes.
    SyncResult run();
    bool is_running() const { return running_.load(); }

private:
    SyncResult run_locked();

    NoteStore& store_;
    HttpTransport& transport_;
    Clock clock_;
    Options options_;
    std::atomic<bool> running_{false};
};

}  // namespace pbnotes
