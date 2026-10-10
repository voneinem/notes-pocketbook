#include "pbnotes/sync_status.h"

namespace pbnotes {

SyncStatus sync_status(const Account& account, bool running) {
    SyncStatus s;
    s.at = account.last_sync_at;
    s.counts = account.last_sync_counts;
    s.message_key = account.last_sync_message;
    if (running) s.state = "running";
    else if (!account.last_sync_at) s.state = "never";
    else s.state = to_string(account.last_sync_outcome);
    return s;
}

}  // namespace pbnotes
