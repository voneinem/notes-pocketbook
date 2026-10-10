#pragma once
// What the user is told about the last sync: a state key, the time, and the counts. The UI turns
// this into text (translated); the logic lives here so it can be tested on the host.

#include <optional>
#include <string>

#include "pbnotes/types.h"

namespace pbnotes {

struct SyncStatus {
    std::string state;  // never | running | success | partial | failed | offline | maintenance
    std::optional<int64_t> at;  // epoch seconds of the last finished attempt
    SyncCounts counts;
    std::string message_key;
};

SyncStatus sync_status(const Account& account, bool running);

}  // namespace pbnotes
