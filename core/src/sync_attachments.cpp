#include "sync_attachments.h"

#include "pbnotes/attachment_ref.h"
#include "pbnotes/log.h"

namespace pbnotes {

namespace {

bool is_abort_class(ErrorClass c) {
    return c == ErrorClass::Network || c == ErrorClass::Tls || c == ErrorClass::Auth || c == ErrorClass::Maintenance ||
           c == ErrorClass::Unsupported;
}

enum class Create { Ok, Abort, Failed };

// Creates the note on the server with the text WITHOUT any token; the id and validator are stored, and the
// note stays pending because the stored text still has the tokens.
Create create_without_tokens(NoteStore& store, ApiClient& api, AttachmentPrep* prep) {
    NoteDraft draft;
    draft.title = prep->note.title;
    draft.content = strip_tokens(prep->note.content, find_tokens(prep->note.content));
    draft.category = prep->note.category;
    draft.favorite = prep->note.favorite;
    draft.modified = prep->note.modified;
    const WriteResult w = api.create_note(draft);
    if (is_abort_class(w.error)) {
        prep->abort = true;
        prep->abort_class = w.error;
        return Create::Abort;
    }
    if (w.error != ErrorClass::None || !w.note) return Create::Failed;
    if (!store.confirm_push(prep->note.local_id, *w.note, {draft.title, draft.content, draft.category, draft.favorite})) return Create::Failed;
    auto fresh = store.get(prep->note.local_id);
    if (!fresh) return Create::Failed;
    prep->note = *fresh;
    return Create::Ok;
}

// Marks every listed attachment as failed with the same reason.
void fail_all(NoteStore& store, const std::vector<Attachment>& rows, const char* key, AttachmentPrep* prep) {
    for (const Attachment& a : rows) {
        store.set_attachment_error(a.id, 0, "", key);
        ++prep->failed;
    }
}

}  // namespace

AttachmentPrep prepare_note_attachments(NoteStore& store, ApiClient& api, const std::string& attachments_dir,
                                        bool supported, const Note& note) {
    AttachmentPrep prep;
    prep.note = note;
    prep.outgoing = note.content;
    if (find_tokens(note.content).empty() && store.pending_attachments(note.local_id).empty()) return prep;  // nothing to do

    // a token no pending attachment answers to can never be uploaded: it goes from the text for good
    if (!store.drop_dead_tokens(note.local_id)) {
        prep.note_failed = true;
        return prep;
    }
    if (auto fresh = store.get(note.local_id)) prep.note = *fresh;
    std::vector<Attachment> pending = store.pending_attachments(note.local_id);

    if (!pending.empty() && !supported) {
        fail_all(store, pending, "unsupported_server", &prep);
    } else if (!pending.empty() && attachments_dir.empty()) {
        fail_all(store, pending, "unreadable", &prep);
    } else if (!pending.empty()) {
        // the upload needs the note's server id: a note the server does not know is created first
        if (!prep.note.remote_id) {
            const Create c = create_without_tokens(store, api, &prep);
            if (c == Create::Abort) return prep;
            if (c == Create::Failed) {
                prep.note_failed = true;
                return prep;
            }
        }
        for (int round = 0; round < 2; ++round) {  // a second round only if the server lost the note meanwhile
            bool note_vanished = false;
            for (const Attachment& a : pending) {
                const std::string file = attachments_dir + "/" + a.local_file;
                const AttachmentUploadResult up = api.upload_attachment(*prep.note.remote_id, file, a.display_name);
                if (is_abort_class(up.error)) {
                    prep.abort = true;
                    prep.abort_class = up.error;
                    return prep;
                }
                if (up.not_found) {
                    note_vanished = true;
                    break;
                }
                const char* failure = up.too_large ? "too_large"
                                      : up.unsupported ? "unsupported_server"
                                      : up.unreadable ? "unreadable"
                                      : up.rejected || up.error != ErrorClass::None || up.remote_path.empty() ? "rejected"
                                                                                                              : nullptr;
                if (failure) {
                    store.set_attachment_error(a.id, 0, "", failure);
                    ++prep.failed;
                    continue;
                }
                if (store.record_uploaded(a.id, up.remote_path, a.size.value_or(0))) {
                    ++prep.uploaded;
                } else {  // the file is on the server but the answer could not be stored: try again next time
                    store.set_attachment_error(a.id, 0, "", "rejected");
                    ++prep.failed;
                }
            }
            if (!note_vanished) break;
            // the note was deleted on the server: create it again (without tokens) and upload to the new id
            if (round == 1 || !store.clear_remote_identity(note.local_id)) {
                prep.note_failed = true;
                return prep;
            }
            prep.recreated = true;
            prep.failed = 0;
            prep.uploaded = 0;
            if (auto fresh = store.get(note.local_id)) prep.note = *fresh;
            const Create c = create_without_tokens(store, api, &prep);
            if (c == Create::Abort) return prep;
            if (c == Create::Failed) {
                prep.note_failed = true;
                return prep;
            }
            pending = store.pending_attachments(note.local_id);
        }
    }

    if (auto fresh = store.get(note.local_id)) prep.note = *fresh;
    // whatever token is still in the text was not resolved: it is only left out of what is sent
    prep.outgoing = strip_tokens(prep.note.content, find_tokens(prep.note.content));
    if (prep.uploaded > 0 || prep.failed > 0)
        log::event("attachments.push", {log::count("uploaded", prep.uploaded), log::count("failed", prep.failed)});
    return prep;
}

}  // namespace pbnotes
