// US5 (spec 002 FR-025, FR-027..FR-030, research R-3, R-4): the push side of attachments with the fake Notes
// server: the file reaches the server before any reference to it, interruptions lose nothing, failures strip
// the token from the outgoing text only, conflict copies keep their attachments.
#include <sys/stat.h>

#include <fstream>
#include <string>
#include <vector>

#include "fake_notes_server.h"
#include "fake_transport.h"
#include "pbnotes/attachment_ref.h"
#include "pbnotes/attachment_service.h"
#include "pbnotes/note_store.h"
#include "pbnotes/title.h"
#include "pbnotes/sync_engine.h"
#include "test_support.h"
#include "tmp_dir.h"

using namespace pbnotes;

namespace {

struct Env {
    pbtest::TmpDir dir;
    std::string attachments;
    std::unique_ptr<NoteStore> store;
    pbtest::FakeNotesServer server;
    pbtest::FakeTransport transport;
    std::unique_ptr<AttachmentService> service;
    int64_t now = 1700001000;

    Env() {
        attachments = dir.file("attachments");
        ::mkdir(attachments.c_str(), 0755);
        store = NoteStore::open(dir.file("pbnotes.db"));
        Account a;
        a.server_url = "https://cloud.example.com";
        a.username = "alice";
        a.password = "app-password";
        a.api_versions = "1.0, 1.3, 1.4";   // known from an earlier sync: no capabilities request
        store->save_account(a);
        server.api_versions_header = "1.0, 1.3, 1.4";
        transport.set_handler([this](const Request& r) { return server.handle(r); });
        service.reset(new AttachmentService(*store, transport, attachments, [this] { return now; }));
    }
    SyncResult run() {
        SyncOptions options;
        options.attachments_dir = attachments;
        SyncEngine engine(*store, transport, [this] { return now; }, options);
        return engine.run();
    }
    // Adds a file through the service and returns its token.
    AddResult add(const std::string& name, const std::string& bytes) {
        dir.write(name, bytes);
        auto r = service->add(dir.file(name));
        PB_CHECK(r.ok);
        return r;
    }
    // A synced note pulled from the server.
    int64_t synced_note(const std::string& content, int64_t* remote = nullptr) {
        const int64_t id = server.add_on_server(content);
        PB_CHECK(run().outcome == SyncOutcome::Success);
        if (remote) *remote = id;
        return store->get_by_remote_id(id)->local_id;
    }
    int index_of(const std::string& prefix, size_t from = 0) const {
        for (size_t i = from; i < server.log.size(); ++i)
            if (server.log[i].compare(0, prefix.size(), prefix) == 0) return static_cast<int>(i);
        return -1;
    }
};

}  // namespace

PB_TEST(an_existing_note_uploads_the_file_before_the_put_and_ends_in_sync) {
    Env e;
    int64_t remote = 0;
    const int64_t note = e.synced_note("Title\nbody", &remote);
    e.server.log.clear();
    auto a = e.add("photo one.png", "PNGBYTES");
    PB_CHECK(e.store->save_local(note, pbnotes::derive_title("Title\nbody\n" + a.token + "\n"), "Title\nbody\n" + a.token + "\n", "", false, 500).error == EditError::None);

    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(r.counts.attachments_uploaded, 1);
    PB_CHECK_EQ(r.counts.attachments_failed, 0);
    const int post = e.index_of("POST /attachment/" + std::to_string(remote));
    const int put = e.index_of("PUT /notes/" + std::to_string(remote));
    PB_CHECK(post >= 0 && put > post);   // the file is on the server before any reference to it

    const std::string path = ".attachments." + std::to_string(remote) + "/photo one.png";
    PB_CHECK_EQ(e.server.notes.at(remote).content, "Title\nbody\n![photo one.png](" + encode_attachment_path(path) + ")\n");
    auto local = e.store->get(note);
    PB_CHECK_EQ(local->content, e.server.notes.at(remote).content);   // the local text now holds the server path
    PB_CHECK(local->status == NoteStatus::Void);
    auto row = e.store->get_attachment(a.id);
    PB_CHECK(row->state == AttachmentState::Downloaded);
    PB_CHECK_EQ(row->remote_path.value_or(""), path);
    PB_CHECK_EQ(e.server.uploads.size(), static_cast<size_t>(1));
    PB_CHECK_EQ(e.server.uploads[0].bytes, std::string("PNGBYTES"));
    PB_CHECK_EQ(e.server.uploads[0].note_id, remote);
}

PB_TEST(a_new_note_is_created_without_the_tokens_then_the_file_is_uploaded_then_the_text_is_put) {
    Env e;
    auto a = e.add("photo.png", "BYTES");
    auto created = e.store->create_local(pbnotes::derive_title("New note\n" + a.token + "\ntail"), "New note\n" + a.token + "\ntail", "Cat", false, 300);
    PB_CHECK(created.error == EditError::None);
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);

    PB_CHECK(e.server.log.size() >= 3);
    const int post_note = e.index_of("POST /notes");
    const int post_att = e.index_of("POST /attachment/");
    const int put = e.index_of("PUT /notes/");
    PB_CHECK(post_note >= 0 && post_att > post_note && put > post_att);
    // the first request carried no token
    const auto& reqs = e.transport.requests();
    for (const auto& req : reqs)
        if (req.method == "POST" && req.url.find("/api/v1/notes") != std::string::npos) PB_CHECK(req.body.find("pbatt") == std::string::npos);
    // the PUT used the ETag the creation returned
    bool put_has_if_match = false;
    for (const auto& req : reqs)
        if (req.method == "PUT") put_has_if_match = has_header(req.headers, "If-Match");
    PB_CHECK(put_has_if_match);

    PB_CHECK_EQ(e.server.notes.size(), static_cast<size_t>(1));
    const auto& server_note = e.server.notes.begin()->second;
    PB_CHECK(server_note.content.find("pbatt") == std::string::npos);
    PB_CHECK(server_note.content.find(".attachments." + std::to_string(server_note.id) + "/photo.png") != std::string::npos);
    PB_CHECK_EQ(server_note.category, std::string("Cat"));
    PB_CHECK(e.store->get(created.local_id)->status == NoteStatus::Void);
    PB_CHECK_EQ(e.store->get(created.local_id)->content, server_note.content);
}

PB_TEST(a_note_that_consists_of_a_token_alone_is_created_with_empty_text) {
    Env e;
    auto a = e.add("photo.png", "BYTES");
    auto created = e.store->create_local(pbnotes::derive_title(a.token), a.token, "", false, 300);
    PB_CHECK(created.error == EditError::None);
    PB_CHECK(e.run().outcome == SyncOutcome::Success);
    PB_CHECK_EQ(e.server.notes.size(), static_cast<size_t>(1));
    PB_CHECK(e.server.notes.begin()->second.content.find(".attachments.") != std::string::npos);
    const auto& reqs = e.transport.requests();
    PB_CHECK(reqs[0].method == "POST" && reqs[0].body.find("\"content\":\"\"") != std::string::npos);
}

PB_TEST(a_rejected_upload_strips_the_token_from_the_outgoing_text_only) {
    Env e;
    int64_t remote = 0;
    const int64_t note = e.synced_note("Title\nbody", &remote);
    auto a = e.add("big.png", "BYTES");
    PB_CHECK(e.store->save_local(note, pbnotes::derive_title("Title\nbody\n" + a.token + "\nmore"), "Title\nbody\n" + a.token + "\nmore", "", false, 500).error == EditError::None);
    e.server.upload_fail_status = 413;

    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Partial);
    PB_CHECK_EQ(r.message_key, std::string("attachments_failed"));
    PB_CHECK_EQ(r.counts.attachments_failed, 1);
    PB_CHECK_EQ(r.counts.attachments_uploaded, 0);
    PB_CHECK_EQ(e.server.notes.at(remote).content, std::string("Title\nbody\nmore"));   // no token on the server
    auto local = e.store->get(note);
    PB_CHECK(local->content.find(a.token) != std::string::npos);                          // the local text keeps it
    PB_CHECK(local->status == NoteStatus::Edited);                                          // still pending
    auto row = e.store->get_attachment(a.id);
    PB_CHECK(row->state == AttachmentState::Pending);
    PB_CHECK_EQ(row->error_key, std::string("too_large"));
    PB_CHECK_EQ(e.store->get_account()->last_sync_counts.attachments_failed, 1);

    // when the problem is gone the next run uploads it and the note ends in sync
    e.server.upload_fail_status = 0;
    auto r2 = e.run();
    PB_CHECK(r2.outcome == SyncOutcome::Success);
    PB_CHECK(e.store->get(note)->status == NoteStatus::Void);
    PB_CHECK(e.server.notes.at(remote).content.find(".attachments.") != std::string::npos);
    PB_CHECK_EQ(e.store->get_attachment(a.id)->error_key, std::string());
}

PB_TEST(a_transport_failure_during_the_upload_loses_nothing_and_never_creates_the_note_twice) {
    Env e;
    auto a = e.add("photo.png", "BYTES");
    auto created = e.store->create_local(pbnotes::derive_title("New note\n" + a.token), "New note\n" + a.token, "", false, 300);
    e.server.upload_network_failures = 1;

    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Offline);
    PB_CHECK_EQ(e.server.notes.size(), static_cast<size_t>(1));           // created without the token
    PB_CHECK(e.server.notes.begin()->second.content.find("pbatt") == std::string::npos);
    auto local = e.store->get(created.local_id);
    PB_CHECK(local->remote_id.has_value());
    PB_CHECK(local->status == NoteStatus::Edited);
    PB_CHECK(local->content.find(a.token) != std::string::npos);          // nothing lost
    PB_CHECK(e.store->get_attachment(a.id)->state == AttachmentState::Pending);

    auto r2 = e.run();
    PB_CHECK(r2.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(e.server.notes.size(), static_cast<size_t>(1));           // not created twice
    PB_CHECK(e.server.notes.begin()->second.content.find(".attachments.") != std::string::npos);
    PB_CHECK(e.store->get(created.local_id)->status == NoteStatus::Void);
}

PB_TEST(a_failure_between_the_upload_and_the_put_does_not_upload_again) {
    Env e;
    int64_t remote = 0;
    const int64_t note = e.synced_note("Title", &remote);
    auto a = e.add("photo.png", "BYTES");
    PB_CHECK(e.store->save_local(note, pbnotes::derive_title("Title\n" + a.token), "Title\n" + a.token, "", false, 500).error == EditError::None);
    const size_t first = e.transport.requests().size();
    e.transport.fail_request(first + 1, ErrorClass::Network);   // request 0: the upload, request 1: the PUT

    PB_CHECK(e.run().outcome == SyncOutcome::Offline);
    PB_CHECK_EQ(e.server.uploads.size(), static_cast<size_t>(1));
    PB_CHECK(e.store->get_attachment(a.id)->state == AttachmentState::Downloaded);           // already uploaded
    PB_CHECK(e.store->get(note)->content.find("pbatt") == std::string::npos);                // text holds the path
    PB_CHECK(e.store->get(note)->status == NoteStatus::Edited);

    PB_CHECK(e.run().outcome == SyncOutcome::Success);
    PB_CHECK_EQ(e.server.uploads.size(), static_cast<size_t>(1));          // no second upload
    PB_CHECK(e.server.notes.at(remote).content.find(".attachments.") != std::string::npos);
    PB_CHECK(e.store->get(note)->status == NoteStatus::Void);
}

PB_TEST(a_lost_upload_response_uploads_again_and_uses_the_name_the_server_returns) {
    Env e;
    int64_t remote = 0;
    const int64_t note = e.synced_note("Title", &remote);
    auto a = e.add("photo.png", "BYTES");
    PB_CHECK(e.store->save_local(note, pbnotes::derive_title("Title\n" + a.token), "Title\n" + a.token, "", false, 500).error == EditError::None);
    e.server.lose_next_upload_response = true;   // the server stores the file, the answer never arrives

    PB_CHECK(e.run().outcome == SyncOutcome::Offline);
    PB_CHECK_EQ(e.server.uploads.size(), static_cast<size_t>(1));
    PB_CHECK(e.store->get_attachment(a.id)->state == AttachmentState::Pending);

    PB_CHECK(e.run().outcome == SyncOutcome::Success);
    PB_CHECK_EQ(e.server.uploads.size(), static_cast<size_t>(2));
    PB_CHECK_EQ(e.server.uploads[1].filename, ".attachments." + std::to_string(remote) + "/photo (2).png");   // deduplicated
    PB_CHECK(e.server.notes.at(remote).content.find("photo%20%282%29.png") != std::string::npos);
    PB_CHECK(e.store->get(note)->status == NoteStatus::Void);
}

PB_TEST(a_conflict_after_the_upload_keeps_the_server_text_and_the_copy_uploads_under_its_own_note) {
    Env e;
    int64_t remote = 0;
    const int64_t note = e.synced_note("Title\nold", &remote);
    auto a = e.add("photo.png", "BYTES");
    PB_CHECK(e.store->save_local(note, pbnotes::derive_title("Title\nmy edit\n" + a.token + "\n"), "Title\nmy edit\n" + a.token + "\n", "", false, 500).error == EditError::None);
    e.server.edit_on_server(remote, "Title\nthe server's edit\n");   // changed meanwhile: the PUT will get 412

    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(r.counts.conflicts, 1);
    PB_CHECK_EQ(e.server.notes.size(), static_cast<size_t>(2));
    PB_CHECK_EQ(e.server.notes.at(remote).content, std::string("Title\nthe server's edit\n"));   // not overwritten
    int64_t copy_remote = 0;
    for (const auto& kv : e.server.notes)
        if (kv.first != remote) copy_remote = kv.first;
    const std::string copy_text = e.server.notes.at(copy_remote).content;
    PB_CHECK(copy_text.find("my edit") != std::string::npos);
    PB_CHECK_EQ(e.server.notes.at(copy_remote).title.find("Title (conflict "), static_cast<size_t>(0));   // the marker is in the title
    PB_CHECK(copy_text.find(".attachments." + std::to_string(copy_remote) + "/photo.png") != std::string::npos);
    PB_CHECK_EQ(e.server.uploads.size(), static_cast<size_t>(2));   // once for the original note, once for the copy
    PB_CHECK_EQ(e.server.uploads[1].note_id, copy_remote);
    PB_CHECK_EQ(e.server.uploads[1].bytes, std::string("BYTES"));
    for (const auto& s : e.store->list()) PB_CHECK(!s.pending);
}

PB_TEST(a_server_without_attachment_support_keeps_them_pending_and_pushes_the_text_without_tokens) {
    Env e;
    e.server.api_versions_header = "1.0, 1.3";
    e.store->set_collection_state(std::nullopt, 0, "1.0, 1.3");
    int64_t remote = 0;
    const int64_t note = e.synced_note("Title", &remote);
    auto a = e.add("photo.png", "BYTES");
    PB_CHECK(e.store->save_local(note, pbnotes::derive_title("Title\nline\n" + a.token + "\n"), "Title\nline\n" + a.token + "\n", "", false, 500).error == EditError::None);
    const int before = e.server.attachment_requests;

    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Partial);
    PB_CHECK_EQ(r.message_key, std::string("attachments_failed"));
    PB_CHECK_EQ(e.server.attachment_requests, before);                         // no attachment request at all
    PB_CHECK_EQ(e.server.notes.at(remote).content, std::string("Title\nline\n"));
    auto row = e.store->get_attachment(a.id);
    PB_CHECK(row->state == AttachmentState::Pending);
    PB_CHECK_EQ(row->error_key, std::string("unsupported_server"));
    PB_CHECK(e.store->get(note)->content.find(a.token) != std::string::npos);
}

PB_TEST(a_note_deleted_on_the_server_and_edited_here_is_kept_with_its_attachment) {
    Env e;
    int64_t remote = 0;
    const int64_t note = e.synced_note("Title\nbody", &remote);
    auto a = e.add("photo.png", "BYTES");
    PB_CHECK(e.store->save_local(note, pbnotes::derive_title("Title\nbody\n" + a.token), "Title\nbody\n" + a.token, "", false, 500).error == EditError::None);
    e.server.notes.erase(remote);   // deleted on the server

    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(r.counts.recreated, 1);
    PB_CHECK_EQ(e.server.notes.size(), static_cast<size_t>(1));
    const auto& server_note = e.server.notes.begin()->second;
    PB_CHECK(server_note.content.find(".attachments." + std::to_string(server_note.id) + "/photo.png") != std::string::npos);
    auto local = e.store->get(note);
    PB_CHECK(local.has_value() && local->status == NoteStatus::Void);
    PB_CHECK(e.store->get_attachment(a.id)->state == AttachmentState::Downloaded);
    PB_CHECK_EQ(e.server.uploads.back().note_id, server_note.id);
}

PB_TEST(a_sync_with_nothing_pending_makes_no_attachment_request_and_downloads_nothing) {
    Env e;
    e.synced_note("Title\n![x](.attachments.1/a.png)");
    PB_CHECK_EQ(e.server.attachment_requests, 0);
    PB_CHECK(e.run().outcome == SyncOutcome::Success);
    PB_CHECK_EQ(e.server.attachment_requests, 0);
}

PB_TEST(without_an_attachment_folder_pending_attachments_stay_pending_and_the_note_is_still_pushed) {
    Env e;
    int64_t remote = 0;
    const int64_t note = e.synced_note("Title", &remote);
    auto a = e.add("photo.png", "BYTES");
    PB_CHECK(e.store->save_local(note, pbnotes::derive_title("Title\n" + a.token), "Title\n" + a.token, "", false, 500).error == EditError::None);
    SyncEngine engine(*e.store, e.transport, [&] { return e.now; });   // default options: no folder
    auto r = engine.run();
    PB_CHECK(r.outcome == SyncOutcome::Partial);
    PB_CHECK_EQ(e.server.notes.at(remote).content, std::string("Title"));
    PB_CHECK(e.store->get_attachment(a.id)->state == AttachmentState::Pending);
}

PB_TEST_MAIN()
