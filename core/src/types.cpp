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

const char* to_string(AttachmentKind k) { return k == AttachmentKind::Image ? "image" : "file"; }

std::optional<AttachmentKind> attachment_kind_from_string(const std::string& s) {
    if (s == "image") return AttachmentKind::Image;
    if (s == "file") return AttachmentKind::File;
    return std::nullopt;
}

const char* to_string(AttachmentState s) {
    switch (s) {
        case AttachmentState::Staged: return "staged";
        case AttachmentState::Pending: return "pending";
        case AttachmentState::Remote: return "remote";
        case AttachmentState::Downloaded: return "downloaded";
    }
    return "remote";
}

std::optional<AttachmentState> attachment_state_from_string(const std::string& s) {
    if (s == "staged") return AttachmentState::Staged;
    if (s == "pending") return AttachmentState::Pending;
    if (s == "remote") return AttachmentState::Remote;
    if (s == "downloaded") return AttachmentState::Downloaded;
    return std::nullopt;
}

bool is_attachment_error_key(const std::string& key) {
    static const char* const kKeys[] = {"",          "too_large",  "not_found",          "rejected",           "unsupported_server",
                                        "storage_full", "unreadable", "format_unsupported", "network"};
    for (const char* k : kKeys)
        if (key == k) return true;
    return false;
}

}  // namespace pbnotes
