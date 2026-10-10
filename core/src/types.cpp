#include "pbnotes/types.h"

namespace pbnotes {

const char* to_string(NoteStatus s) {
    switch (s) {
        case NoteStatus::Void: return "void";
        case NoteStatus::Edited: return "edited";
        case NoteStatus::Deleted: return "deleted";
    }
    return "void";
}

std::optional<NoteStatus> note_status_from_string(const std::string& s) {
    if (s == "void") return NoteStatus::Void;
    if (s == "edited") return NoteStatus::Edited;
    if (s == "deleted") return NoteStatus::Deleted;
    return std::nullopt;
}

const char* to_string(SyncOutcome o) {
    switch (o) {
        case SyncOutcome::Never: return "never";
        case SyncOutcome::Running: return "running";
        case SyncOutcome::Success: return "success";
        case SyncOutcome::Partial: return "partial";
        case SyncOutcome::Failed: return "failed";
        case SyncOutcome::Offline: return "offline";
        case SyncOutcome::Maintenance: return "maintenance";
    }
    return "never";
}

std::optional<SyncOutcome> sync_outcome_from_string(const std::string& s) {
    for (SyncOutcome o : {SyncOutcome::Never, SyncOutcome::Running, SyncOutcome::Success, SyncOutcome::Partial,
                          SyncOutcome::Failed, SyncOutcome::Offline, SyncOutcome::Maintenance}) {
        if (s == to_string(o)) return o;
    }
    return std::nullopt;
}

const char* to_string(ErrorClass e) {
    switch (e) {
        case ErrorClass::None: return "none";
        case ErrorClass::Network: return "network";
        case ErrorClass::Tls: return "tls";
        case ErrorClass::Auth: return "auth";
        case ErrorClass::Maintenance: return "maintenance";
        case ErrorClass::Unsupported: return "unsupported";
        case ErrorClass::Server: return "server";
    }
    return "none";
}

}  // namespace pbnotes
