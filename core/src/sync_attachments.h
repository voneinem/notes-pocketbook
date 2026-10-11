#pragma once
// The attachment part of pushing one note (spec 002, research R-3). Private to the sync engine.

#include <string>

#include "pbnotes/api_client.h"
#include "pbnotes/note_store.h"
#include "pbnotes/types.h"

namespace pbnotes {

struct AttachmentPrep {
    bool abort = false;                   // a connection or account problem: the whole run ends
    ErrorClass abort_class = ErrorClass::None;
    bool note_failed = false;             // a store failure: count the note as failed and go on with the next
    bool recreated = false;               // the server no longer had the note: it was created again
    Note note;                            // the note as it is now (it may have a server id and rewritten text)
    std::string outgoing;                 // the text to send: tokens that could not be resolved are removed
    int uploaded = 0;                     // files the server accepted
    int failed = 0;                       // pending attachments that stay pending
};

// Before a note's text is sent: a note the server does not know is created first, WITHOUT its tokens (the
// upload needs its id); every pending attachment is uploaded and its token in the stored text becomes the
// server path (NoteStore::record_uploaded); a token that could not be resolved stays in the local text and is
// only left out of what is sent. Without attachments this does nothing and `outgoing` is the note's text.
// Never downloads. `supported`: the server announces API version 1.4.
AttachmentPrep prepare_note_attachments(NoteStore& store, ApiClient& api, const std::string& attachments_dir,
                                        bool supported, const Note& note);

}  // namespace pbnotes
