#include "pbnotes/sync_engine.h"

#include <ctime>

#include "pbnotes/api_client.h"
#include "pbnotes/attachment_ref.h"
#include "pbnotes/log.h"
#include "sync_attachments.h"

namespace pbnotes {

namespace {

SyncResult failure(SyncOutcome outcome, ErrorClass cls, const char* key) {
    SyncResult r;
    r.outcome = outcome;
    r.error_class = cls;
    r.message_key = key;
    return r;
}

// Maps a failed request to the sync outcome the user sees.
SyncResult from_error(ErrorClass cls) {
    switch (cls) {
        case ErrorClass::Network: return failure(SyncOutcome::Offline, cls, "offline");
        case ErrorClass::Maintenance: return failure(SyncOutcome::Maintenance, cls, "maintenance");
        case ErrorClass::Auth: return failure(SyncOutcome::Failed, cls, "auth_rejected");
        case ErrorClass::Tls: return failure(SyncOutcome::Failed, cls, "certificate_unverifiable");
        case ErrorClass::Unsupported: return failure(SyncOutcome::Failed, cls, "server_unsupported");
        case ErrorClass::Server:
        case ErrorClass::None: break;
    }
    return failure(SyncOutcome::Failed, ErrorClass::Server, "server_error");
}

std::string join_versions(const std::vector<std::string>& v) {
    std::string s;
    for (const auto& x : v) s += (s.empty() ? "" : ", ") + x;
    return s;
}

bool versions_include_v1(const std::string& header) {
    // "0.2, 1.0, 1.3" -> any token "1" or "1.x"
    size_t pos = 0;
    while (pos < header.size()) {
        size_t end = header.find(',', pos);
        if (end == std::string::npos) end = header.size();
        std::string tok = header.substr(pos, end - pos);
        const size_t b = tok.find_first_not_of(" \t");
        tok = b == std::string::npos ? std::string() : tok.substr(b);
        if (tok == "1" || tok.compare(0, 2, "1.") == 0) return true;
        pos = end + 1;
    }
    return false;
}

}  // namespace

SyncEngine::SyncEngine(NoteStore& store, HttpTransport& transport, Clock clock, Options options)
    : store_(store), transport_(transport), clock_(std::move(clock)), options_(options) {
    if (!clock_) clock_ = [] { return static_cast<int64_t>(std::time(nullptr)); };
}

SyncResult SyncEngine::run() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        SyncResult busy;
        busy.outcome = SyncOutcome::Running;
        busy.message_key = "already_running";
        return busy;
    }
    SyncResult result = run_locked();
    if (store_.get_account()) store_.record_sync_result(result, clock_());
    log::event("sync.finished", {log::text("outcome", to_string(result.outcome)),
                                 log::text("error_class", to_string(result.error_class)),
                                 log::count("pulled", result.counts.pulled), log::count("deleted", result.counts.deleted),
                                 log::count("att_uploaded", result.counts.attachments_uploaded),
                                 log::count("att_failed", result.counts.attachments_failed)});
    running_.store(false);
    return result;
}

namespace {

// Outcome of pushing one pending note.
enum class PushOutcome { Done, NoteFailed, Abort };

NoteDraft draft_of(const Note& n) {
    NoteDraft d;
    d.title = n.title;
    d.content = n.content;
    d.category = n.category;
    d.favorite = n.favorite;
    d.modified = n.modified;
    return d;
}

NoteStore::PushSnapshot snapshot_of(const Note& n) { return {n.title, n.content, n.category, n.favorite}; }

// Transport-level problems end the whole run (there is no point in trying the next note).
bool is_abort_class(ErrorClass c) {
    return c == ErrorClass::Network || c == ErrorClass::Tls || c == ErrorClass::Auth || c == ErrorClass::Maintenance ||
           c == ErrorClass::Unsupported;
}

}  // namespace

SyncResult SyncEngine::run_locked() {
    auto account = store_.get_account();
    if (!account) return failure(SyncOutcome::Failed, ErrorClass::None, "no_account");

    ApiClient api(transport_, {account->server_url, account->username, account->password});
    api.set_delete_uses_if_match(options_.delete_uses_if_match);

    // API version gate: ask the capabilities endpoint once (when no versions are stored); later
    // runs rely on the X-Notes-API-Versions header of the listing.
    std::string api_versions = account->api_versions;
    if (api_versions.empty()) {
        const CapabilitiesResult caps = api.fetch_capabilities();
        if (caps.error != ErrorClass::None) return from_error(caps.error);
        api_versions = join_versions(caps.capabilities.notes_api_versions);
    }

    SyncCounts counts;
    int failed_notes = 0;
    auto abort_with = [&](ErrorClass cls) {
        SyncResult r = from_error(cls);
        r.counts = counts;
        return r;
    };

    // ---- push: local changes first ---------------------------------------------------------
    // A queue, because resolving a conflict adds the conflict copy to it (it is uploaded right away).
    std::vector<Note> queue = store_.pending_notes();
    for (size_t qi = 0; qi < queue.size(); ++qi) {
        const Note note = queue[qi];
        if (note.status == NoteStatus::Edited) {
            // a title that breaks the rules is never sent (the editor cannot produce one; this is a safety net)
            if (!validate_title(note.title).empty()) { ++failed_notes; continue; }
            // attachments first: the files must be on the server before any text refers to them (spec 002)
            Note cur = note;
            std::string outgoing = note.content;
            bool recreated = false;
            const AttachmentPrep prep = prepare_note_attachments(store_, api, options_.attachments_dir,
                                                                 supports_attachments(api_versions), note);
            counts.attachments_uploaded += prep.uploaded;
            counts.attachments_failed += prep.failed;
            if (prep.abort) return abort_with(prep.abort_class);
            if (prep.note_failed) { ++failed_notes; continue; }
            cur = prep.note;
            outgoing = prep.outgoing;
            recreated = prep.recreated;

            NoteDraft draft = draft_of(cur);
            draft.content = outgoing;  // the local text may still hold tokens that never go to the server
            const NoteStore::PushSnapshot sent{cur.title, outgoing, cur.category, cur.favorite};
            WriteResult w;
            if (!cur.remote_id) {
                w = api.create_note(draft);
            } else {
                w = api.update_note(*cur.remote_id, draft, cur.etag.value_or(""));
                if (w.not_found) {  // deleted on the server: create it again from the local text
                    if (!store_.clear_remote_identity(cur.local_id)) { ++failed_notes; continue; }
                    recreated = true;
                    w = api.create_note(draft);
                } else if (w.precondition_failed) {
                    // Changed on the server since we last saw it (HTTP 412): never overwritten. Fetch the
                    // server's version, keep BOTH (server version = original, ours = uploaded copy).
                    const NoteResult server = api.get_note(*cur.remote_id);
                    if (is_abort_class(server.error)) return abort_with(server.error);
                    if (server.error != ErrorClass::None) { ++failed_notes; continue; }
                    if (server.not_found) {  // vanished meanwhile: same as a 404 on the PUT
                        if (!store_.clear_remote_identity(cur.local_id)) { ++failed_notes; continue; }
                        recreated = true;
                        w = api.create_note(draft);
                    } else if (server.note) {
                        const ConflictResolution res = store_.resolve_conflict(cur.local_id, *server.note, clock_());
                        if (!res.ok) { ++failed_notes; continue; }
                        if (res.created_copy) {
                            ++counts.conflicts;
                            if (auto copy = store_.get(res.copy_local_id)) queue.push_back(*copy);  // upload it now
                        }
                        continue;
                    } else {
                        ++failed_notes;
                        continue;
                    }
                }
            }
            if (is_abort_class(w.error)) return abort_with(w.error);
            if (w.error != ErrorClass::None || !w.note) { ++failed_notes; continue; }
            if (!store_.confirm_push(cur.local_id, *w.note, sent)) { ++failed_notes; continue; }
            ++counts.pushed;
            if (recreated) ++counts.recreated;
        } else if (note.status == NoteStatus::Deleted) {
            if (!note.remote_id) {  // never reached the server: nothing to delete there
                store_.remove_note(note.local_id);
                continue;
            }
            const DeleteResult d = api.delete_note(*note.remote_id, note.etag.value_or(""));
            if (is_abort_class(d.error)) return abort_with(d.error);
            if (d.outcome == DeleteOutcome::Deleted || d.outcome == DeleteOutcome::AlreadyGone) {
                store_.remove_note(note.local_id);
                ++counts.pushed;
            } else if (d.outcome == DeleteOutcome::ChangedOnServer) {
                // Changed on the server: the deletion is NOT applied; the server's version comes back.
                std::optional<ApiNote> server = d.server_note;
                if (!server) {  // the If-Match variant did not fetch it
                    const NoteResult fetched = api.get_note(*note.remote_id);
                    if (is_abort_class(fetched.error)) return abort_with(fetched.error);
                    if (fetched.not_found) { store_.remove_note(note.local_id); continue; }
                    server = fetched.note;
                }
                if (server && store_.restore_from_server(note.local_id, *server)) ++counts.restored;
                else ++failed_notes;
            } else {
                ++failed_notes;
            }
        }
    }

    // ---- pull ------------------------------------------------------------------------------
    account = store_.get_account();  // collection bookkeeping is unchanged by the push, but re-read it
    SyncResult result;
    // Notes stored with a title that is not the server's (older versions, or no text and no title) need
    // the full list once; the usual validator/pruneBefore would skip them as "unchanged".
    const bool full_pull = store_.titles_need_refresh();
    const ListResult list = full_pull ? api.list_notes(std::nullopt, 0)
                                      : api.list_notes(account->collection_etag, account->collection_last_modified);
    if (list.error != ErrorClass::None) return abort_with(list.error);
    result.counts = counts;
    // A pending attachment that could not be uploaded is not an error of the run, but it is not finished either.
    const bool attachments_pending = counts.attachments_failed > 0;
    if (list.not_modified) {
        result.outcome = failed_notes || attachments_pending ? SyncOutcome::Partial : SyncOutcome::Success;
        result.message_key = failed_notes ? "partial" : attachments_pending ? "attachments_failed" : "up_to_date";
        if (account->api_versions != api_versions)
            store_.set_collection_state(account->collection_etag, account->collection_last_modified, api_versions);
        return result;
    }

    if (!list.api_versions.empty()) {
        api_versions = list.api_versions;
        if (!versions_include_v1(api_versions)) {
            SyncResult r = failure(SyncOutcome::Failed, ErrorClass::Unsupported, "server_unsupported");
            r.counts = counts;
            return r;
        }
    }

    PullApplyResult applied = store_.apply_remote(list.notes);
    int fetched = 0;
    std::vector<ApiNote> extra;
    for (int64_t id : applied.need_full_fetch) {
        const NoteResult one = api.get_note(id);
        if (one.error != ErrorClass::None) return abort_with(one.error);
        if (one.note && one.note->id != id) {
            SyncResult r = failure(SyncOutcome::Failed, ErrorClass::Server, "server_error");
            r.counts = counts;
            return r;
        }
        if (one.note) extra.push_back(*one.note);
    }
    if (!extra.empty()) {
        // Insert the individually fetched notes by re-applying the listing with the pruned
        // placeholders replaced by the full notes (everything else in it is a no-op the second time).
        std::vector<ApiNote> again = list.notes;
        for (auto& n : again)
            for (const auto& e : extra)
                if (e.id == n.id) n = e;
        fetched = store_.apply_remote(again).inserted;
    }

    store_.set_collection_state(list.etag, list.last_modified, api_versions);
    // A list requested without validator was complete, so every title in it is the server's.
    if (full_pull || !account->collection_etag || account->collection_etag->empty()) store_.mark_titles_refreshed();
    result.outcome = failed_notes || attachments_pending ? SyncOutcome::Partial : SyncOutcome::Success;
    result.message_key = failed_notes ? "partial" : attachments_pending ? "attachments_failed" : "ok";
    result.counts.pulled = applied.inserted + applied.updated + fetched;
    result.counts.deleted = applied.removed;
    return result;
}

}  // namespace pbnotes
