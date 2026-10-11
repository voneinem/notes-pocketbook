// US4 (spec 002 FR-019..FR-024, FR-026): the download side end to end with the fake Notes server: sync
// never touches attachments, a download is explicit, limits and failures leave nothing half-written.
#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

#include "fake_notes_server.h"
#include "fake_transport.h"
#include "pbnotes/attachment_ref.h"
#include "pbnotes/attachment_service.h"
#include "pbnotes/note_store.h"
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
        store->save_account(a);
        server.api_versions_header = "1.0, 1.3, 1.4";
        transport.set_handler([this](const Request& r) { return server.handle(r); });
        service.reset(new AttachmentService(*store, transport, attachments, [this] { return now; }));
    }
    SyncResult sync() {
        SyncEngine engine(*store, transport, [this] { return now; });
        return engine.run();
    }
    // A server note referencing one attachment, pulled by a first sync. Returns the local id.
    int64_t pulled_note(const std::string& path, const std::string& bytes, int64_t* remote = nullptr) {
        const int64_t id = server.add_on_server("Note\n![pic](" + encode_attachment_path(path) + ")");
        server.add_attachment(id, path, bytes);
        if (remote) *remote = id;
        PB_CHECK(sync().outcome == SyncOutcome::Success);
        return store->get_by_remote_id(id)->local_id;
    }
    std::vector<std::string> files() const {
        std::vector<std::string> out;
        if (DIR* d = opendir(attachments.c_str())) {
            while (dirent* e = readdir(d)) {
                const std::string n = e->d_name;
                if (n != "." && n != "..") out.push_back(n);
            }
            closedir(d);
        }
        std::sort(out.begin(), out.end());
        return out;
    }
    std::string read(const std::string& name) const {
        std::ifstream f(attachments + "/" + name, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }
    void write(const std::string& name, const std::string& data) const {
        std::ofstream f(attachments + "/" + name, std::ios::binary);
        f << data;
    }
};

}  // namespace

PB_TEST(a_sync_of_notes_that_reference_attachments_makes_no_attachment_request) {
    Env e;
    for (int i = 0; i < 5; ++i) {
        const int64_t id = e.server.add_on_server("Note " + std::to_string(i) + "\n![p](.attachments." + std::to_string(i + 1) + "/a.png)\n[f](.attachments." + std::to_string(i + 1) + "/d.pdf)");
        e.server.add_attachment(id, ".attachments." + std::to_string(i + 1) + "/a.png", "PNG");
    }
    PB_CHECK(e.sync().outcome == SyncOutcome::Success);
    PB_CHECK_EQ(e.server.attachment_requests, 0);
    PB_CHECK(e.files().empty());
    // and a second, incremental sync as well
    e.server.edit_on_server(1, "Note 0 edited\n![p](.attachments.1/a.png)");
    PB_CHECK(e.sync().outcome == SyncOutcome::Success);
    PB_CHECK_EQ(e.server.attachment_requests, 0);
}

PB_TEST(a_download_stores_the_file_under_its_id_and_marks_the_row_downloaded) {
    Env e;
    const int64_t note = e.pulled_note(".attachments.1/photo one.png", "PNGBYTES");
    auto r = e.service->download(note, ".attachments.1/photo one.png");
    PB_CHECK(r.ok);
    PB_CHECK_EQ(r.error_key, std::string());
    auto a = e.store->attachment_for(note, ".attachments.1/photo one.png");
    PB_CHECK(a.has_value());
    PB_CHECK(a->state == AttachmentState::Downloaded);
    PB_CHECK_EQ(a->local_file, std::to_string(a->id) + ".png");
    PB_CHECK_EQ(a->size.value_or(-1), static_cast<int64_t>(8));
    PB_CHECK(a->kind == AttachmentKind::Image);
    PB_CHECK_EQ(e.read(a->local_file), std::string("PNGBYTES"));
    PB_CHECK((e.files() == std::vector<std::string>{a->local_file}));   // no .part left
    PB_CHECK_EQ(e.server.attachment_requests, 1);
}

PB_TEST(the_remote_path_never_becomes_a_local_path) {
    Env e;
    const int64_t note = e.pulled_note("../../etc/pass wd.pdf", "DATA");
    PB_CHECK(e.service->download(note, "../../etc/pass wd.pdf").ok);
    auto a = e.store->attachment_for(note, "../../etc/pass wd.pdf");
    PB_CHECK_EQ(a->local_file, std::to_string(a->id) + ".pdf");
    PB_CHECK_EQ(e.files().size(), static_cast<size_t>(1));
    PB_CHECK_EQ(e.files()[0], a->local_file);
}

PB_TEST(a_missing_file_on_the_server_is_not_found_and_the_row_keeps_the_reason) {
    Env e;
    int64_t remote = 0;
    const int64_t note = e.pulled_note(".attachments.1/a.png", "X", &remote);
    e.server.attachments.clear();
    auto r = e.service->download(note, ".attachments.1/a.png");
    PB_CHECK(!r.ok);
    PB_CHECK_EQ(r.error_key, std::string("not_found"));
    auto a = e.store->attachment_for(note, ".attachments.1/a.png");
    PB_CHECK(a.has_value());
    PB_CHECK(a->state == AttachmentState::Remote);
    PB_CHECK_EQ(a->error_key, std::string("not_found"));
    PB_CHECK(e.files().empty());
}

PB_TEST(a_file_over_the_limit_is_refused_and_nothing_is_stored) {
    Env e;
    const int64_t note = e.pulled_note(".attachments.1/big.pdf", std::string(static_cast<size_t>(kMaxAttachmentBytes) + 1, 'x'));
    auto r = e.service->download(note, ".attachments.1/big.pdf");
    PB_CHECK(!r.ok);
    PB_CHECK_EQ(r.error_key, std::string("too_large"));
    PB_CHECK_EQ(e.store->attachment_for(note, ".attachments.1/big.pdf")->error_key, std::string("too_large"));
    PB_CHECK(e.files().empty());
}

PB_TEST(a_full_storage_stops_cleanly_and_keeps_earlier_files) {
    Env e;
    const int64_t note = e.pulled_note(".attachments.1/a.png", "FIRST");
    PB_CHECK(e.service->download(note, ".attachments.1/a.png").ok);
    const auto before = e.files();
    e.server.add_attachment(e.store->get(note)->remote_id.value(), ".attachments.1/b.png", "SECOND");
    e.transport.simulate_storage_full = true;
    auto r = e.service->download(note, ".attachments.1/b.png");
    PB_CHECK(!r.ok);
    PB_CHECK_EQ(r.error_key, std::string("storage_full"));
    PB_CHECK((e.files() == before));   // the .part file is gone, the first file is intact
    PB_CHECK_EQ(e.read(before[0]), std::string("FIRST"));
    PB_CHECK_EQ(e.store->attachment_for(note, ".attachments.1/b.png")->error_key, std::string("storage_full"));
}

PB_TEST(a_network_failure_during_a_download_is_reported_and_leaves_no_part_file) {
    Env e;
    const int64_t note = e.pulled_note(".attachments.1/a.png", "X");
    e.transport.fail_request(e.transport.requests().size(), ErrorClass::Network);
    auto r = e.service->download(note, ".attachments.1/a.png");
    PB_CHECK(!r.ok);
    PB_CHECK_EQ(r.error_key, std::string("network"));
    PB_CHECK(r.transport_error == ErrorClass::Network);
    PB_CHECK(e.files().empty());
}

PB_TEST(a_server_without_attachment_support_makes_no_request) {
    Env e;
    e.server.api_versions_header = "1.0, 1.3";
    const int64_t note = e.pulled_note(".attachments.1/a.png", "X");
    e.store->set_collection_state(std::nullopt, 0, "1.0, 1.3");
    auto r = e.service->download(note, ".attachments.1/a.png");
    PB_CHECK(!r.ok);
    PB_CHECK_EQ(r.error_key, std::string("unsupported_server"));
    PB_CHECK_EQ(e.server.attachment_requests, 0);
}

PB_TEST(the_size_is_probed_once_and_cached) {
    Env e;
    const int64_t note = e.pulled_note(".attachments.1/a.png", "ABCDEFGHIJ");
    auto size = e.service->probe_size(note, ".attachments.1/a.png");
    PB_CHECK_EQ(size.value_or(-1), static_cast<int64_t>(10));
    PB_CHECK_EQ(e.store->attachment_for(note, ".attachments.1/a.png")->size.value_or(-1), static_cast<int64_t>(10));
    PB_CHECK_EQ(e.server.attachment_requests, 1);
    auto again = e.service->probe_size(note, ".attachments.1/a.png");
    PB_CHECK_EQ(again.value_or(-1), static_cast<int64_t>(10));
    PB_CHECK_EQ(e.server.attachment_requests, 1);   // the cached size needs no request
}

PB_TEST(a_probe_without_a_length_returns_nothing_and_stores_nothing) {
    Env e;
    e.server.head_has_length = false;
    const int64_t note = e.pulled_note(".attachments.1/a.png", "ABC");
    PB_CHECK(!e.service->probe_size(note, ".attachments.1/a.png").has_value());
    auto a = e.store->attachment_for(note, ".attachments.1/a.png");
    PB_CHECK(!a.has_value() || !a->size.has_value());
}

PB_TEST(the_janitor_removes_files_no_row_names_and_every_part_file_but_only_its_own_names) {
    Env e;
    const int64_t note = e.pulled_note(".attachments.1/a.png", "KEEP");
    PB_CHECK(e.service->download(note, ".attachments.1/a.png").ok);
    const std::string kept = e.store->attachment_for(note, ".attachments.1/a.png")->local_file;
    e.write("999.png", "stray");
    e.write("7.part", "half");
    e.write("notes-from-the-user.txt", "not ours");   // a name the service never makes: left alone
    e.service->purge_files();
    PB_CHECK((e.files() == std::vector<std::string>{kept, "notes-from-the-user.txt"} ||
              e.files() == std::vector<std::string>{"notes-from-the-user.txt", kept}));
    PB_CHECK_EQ(e.read(kept), std::string("KEEP"));
}

PB_TEST(a_pull_that_removes_the_reference_deletes_the_row_and_after_the_janitor_the_file) {
    Env e;
    int64_t remote = 0;
    const int64_t note = e.pulled_note(".attachments.1/a.png", "DATA", &remote);
    PB_CHECK(e.service->download(note, ".attachments.1/a.png").ok);
    e.server.edit_on_server(remote, "Note\nthe picture is gone");
    PB_CHECK(e.sync().outcome == SyncOutcome::Success);
    PB_CHECK(!e.store->attachment_for(note, ".attachments.1/a.png").has_value());
    e.service->purge_files();
    PB_CHECK(e.files().empty());
}

PB_TEST(a_note_deleted_on_the_server_takes_its_attachments_with_it) {
    Env e;
    int64_t remote = 0;
    const int64_t note = e.pulled_note(".attachments.1/a.png", "DATA", &remote);
    PB_CHECK(e.service->download(note, ".attachments.1/a.png").ok);
    e.server.notes.erase(remote);
    e.server.add_on_server("Another note");   // the collection changes, so the pull runs
    PB_CHECK(e.sync().outcome == SyncOutcome::Success);
    PB_CHECK(!e.store->get(note).has_value());
    e.service->purge_files();
    PB_CHECK(e.files().empty());
}

PB_TEST_MAIN()
