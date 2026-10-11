// US4 (spec 002 FR-019..FR-024, data-model.md "Attachment (entity)"): the store operations on the download
// side. Constraints are quoted from data-model.md.
#include <algorithm>
#include <string>
#include <vector>

#include "pbnotes/note_store.h"
#include "pbnotes/title.h"
#include "pbnotes/types.h"
#include "test_support.h"
#include "tmp_dir.h"

using namespace pbnotes;

namespace {

ApiNote server_note(int64_t id, const std::string& content) {
    ApiNote n;
    n.id = id;
    n.etag = "e" + std::to_string(id);
    n.content = content;
    n.modified = 100;
    return n;
}

struct Env {
    pbtest::TmpDir dir;
    std::unique_ptr<NoteStore> store;
    int64_t note = 0;
    Env(const std::string& content = "Note\n![x](.attachments.1/a.png)") {
        store = NoteStore::open(dir.file("db"));
        store->apply_remote({server_note(1, content)});
        note = store->get_by_remote_id(1)->local_id;
    }
};

}  // namespace

PB_TEST(record_downloaded_creates_a_row_keyed_by_note_and_path) {
    Env e;
    PB_CHECK(e.store->record_downloaded(e.note, ".attachments.1/a.png", 1234, "1.png"));
    auto a = e.store->attachment_for(e.note, ".attachments.1/a.png");
    PB_CHECK(a.has_value());
    PB_CHECK(a->state == AttachmentState::Downloaded);
    PB_CHECK_EQ(a->size.value_or(-1), static_cast<int64_t>(1234));
    PB_CHECK_EQ(a->remote_path.value_or(""), std::string(".attachments.1/a.png"));
    PB_CHECK_EQ(a->note_local_id.value_or(0), e.note);
    PB_CHECK_EQ(a->local_file, std::string("1.png"));
    PB_CHECK_EQ(a->error_key, std::string());
    // the same key again updates the same row
    PB_CHECK(e.store->record_downloaded(e.note, ".attachments.1/a.png", 99, "1.png"));
    PB_CHECK_EQ(e.store->attachment_for(e.note, ".attachments.1/a.png")->id, a->id);
    PB_CHECK_EQ(e.store->attachment_for(e.note, ".attachments.1/a.png")->size.value_or(-1), static_cast<int64_t>(99));
}

PB_TEST(a_path_is_unique_per_note_but_not_across_notes) {
    Env e;
    e.store->apply_remote({server_note(1, "Note\n![x](.attachments.1/a.png)"), server_note(2, "Other\n![x](.attachments.1/a.png)")});
    const int64_t other = e.store->get_by_remote_id(2)->local_id;
    PB_CHECK(e.store->record_downloaded(e.note, ".attachments.1/a.png", 5, "1.png"));
    PB_CHECK(e.store->record_downloaded(other, ".attachments.1/a.png", 5, "2.png"));
    PB_CHECK(e.store->attachment_for(e.note, ".attachments.1/a.png")->id != e.store->attachment_for(other, ".attachments.1/a.png")->id);
}

PB_TEST(a_size_over_ten_mebibytes_is_refused_for_a_downloaded_row) {
    Env e;
    PB_CHECK_EQ(kMaxAttachmentBytes, static_cast<int64_t>(10485760));
    PB_CHECK(e.store->record_downloaded(e.note, "a.png", kMaxAttachmentBytes, "1.png"));
    PB_CHECK(!e.store->record_downloaded(e.note, "b.png", kMaxAttachmentBytes + 1, "2.png"));
    PB_CHECK(!e.store->attachment_for(e.note, "b.png").has_value());
    PB_CHECK(!e.store->record_downloaded(e.note, "c.png", -5, "3.png"));
}

PB_TEST(a_remote_row_may_report_a_size_over_the_limit_so_the_reader_can_say_so) {
    Env e;
    PB_CHECK(e.store->record_size(e.note, "big.pdf", kMaxAttachmentBytes * 3));
    auto a = e.store->attachment_for(e.note, "big.pdf");
    PB_CHECK(a.has_value());
    PB_CHECK(a->state == AttachmentState::Remote);
    PB_CHECK_EQ(a->size.value_or(-1), kMaxAttachmentBytes * 3);
    PB_CHECK_EQ(a->local_file, std::string());
}

PB_TEST(record_size_creates_a_remote_row_and_updates_an_existing_one) {
    Env e;
    PB_CHECK(e.store->record_size(e.note, ".attachments.1/a.png", 500));
    auto a = e.store->attachment_for(e.note, ".attachments.1/a.png");
    PB_CHECK(a && a->state == AttachmentState::Remote && a->size.value_or(0) == 500);
    PB_CHECK(e.store->record_size(e.note, ".attachments.1/a.png", 600));
    PB_CHECK_EQ(e.store->attachment_for(e.note, ".attachments.1/a.png")->size.value_or(0), static_cast<int64_t>(600));
    PB_CHECK_EQ(e.store->attachment_for(e.note, ".attachments.1/a.png")->id, a->id);
    PB_CHECK(!e.store->record_size(e.note, "x.png", -1));
    PB_CHECK(!e.store->record_size(99999, "x.png", 5));   // unknown note
}

PB_TEST(ensure_attachment_creates_one_remote_row_with_unknown_size) {
    Env e;
    auto a = e.store->ensure_attachment(e.note, ".attachments.1/photo one.png");
    PB_CHECK(a.has_value());
    PB_CHECK(a->state == AttachmentState::Remote);
    PB_CHECK(!a->size.has_value());
    auto b = e.store->ensure_attachment(e.note, ".attachments.1/photo one.png");
    PB_CHECK_EQ(a->id, b->id);
    PB_CHECK(!e.store->ensure_attachment(99999, "x.png").has_value());
}

PB_TEST(display_name_and_kind_are_derived_from_the_path) {
    Env e;
    auto img = e.store->ensure_attachment(e.note, "dir/My Photo.PNG");
    PB_CHECK_EQ(img->display_name, std::string("My Photo.PNG"));
    PB_CHECK(img->kind == AttachmentKind::Image);
    for (const char* n : {"a.jpg", "a.jpeg", "a.png", "a.gif", "a.bmp", "a.webp", "a.JPG"})
        PB_CHECK(e.store->ensure_attachment(e.note, n)->kind == AttachmentKind::Image);
    for (const char* n : {"a.pdf", "a.svg", "noext", "a.png.zip"})
        PB_CHECK(e.store->ensure_attachment(e.note, n)->kind == AttachmentKind::File);
    // a path with control characters is not a valid reference at all
    PB_CHECK(!e.store->ensure_attachment(e.note, std::string("d/a\x01") + "b.png").has_value());
    // the name is at most 255 characters
    auto longer = e.store->ensure_attachment(e.note, "d/" + std::string(300, 'x') + ".png");
    size_t chars = 0;
    for (unsigned char c : longer->display_name) chars += (c & 0xC0) != 0x80;
    PB_CHECK_EQ(chars, static_cast<size_t>(255));
}

PB_TEST(local_file_is_an_id_with_a_safe_extension_or_nothing) {
    Env e;
    PB_CHECK(e.store->record_downloaded(e.note, "a.png", 5, "1.png"));
    PB_CHECK(e.store->record_downloaded(e.note, "b", 5, "2"));
    PB_CHECK(e.store->record_downloaded(e.note, "c.tar.gz", 5, "3.gz"));
    PB_CHECK(!e.store->record_downloaded(e.note, "d.png", 5, "../etc/passwd"));
    PB_CHECK(!e.store->record_downloaded(e.note, "e.png", 5, "sub/1.png"));
    PB_CHECK(!e.store->record_downloaded(e.note, "f.png", 5, "1.PNG"));            // lower case only
    PB_CHECK(!e.store->record_downloaded(e.note, "g.png", 5, "1.abcdefghi"));      // nine characters
    PB_CHECK(!e.store->record_downloaded(e.note, "h.png", 5, "name.png"));         // not an id
    PB_CHECK(!e.store->record_downloaded(e.note, "i.png", 5, ""));                  // a downloaded row has a file
    PB_CHECK(!e.store->record_downloaded(e.note, "j.png", 5, ".png"));
}

PB_TEST(attachment_errors_are_a_closed_set) {
    Env e;
    auto a = e.store->ensure_attachment(e.note, "a.png");
    for (const char* key : {"too_large", "not_found", "rejected", "unsupported_server", "storage_full", "unreadable",
                            "format_unsupported", "network", ""}) {
        PB_CHECK(e.store->set_attachment_error(a->id, 0, "", key));
        PB_CHECK_EQ(e.store->get_attachment(a->id)->error_key, std::string(key));
    }
    PB_CHECK(!e.store->set_attachment_error(a->id, 0, "", "something else"));
    PB_CHECK(!e.store->set_attachment_error(a->id, 0, "", "not_found/with/path"));
    PB_CHECK_EQ(e.store->get_attachment(a->id)->error_key, std::string());   // unchanged after the refusals
}

PB_TEST(an_error_can_be_recorded_by_note_and_path_and_creates_the_row) {
    Env e;
    PB_CHECK(e.store->set_attachment_error(0, e.note, ".attachments.1/new.pdf", "not_found"));
    auto a = e.store->attachment_for(e.note, ".attachments.1/new.pdf");
    PB_CHECK(a.has_value());
    PB_CHECK(a->state == AttachmentState::Remote);
    PB_CHECK_EQ(a->error_key, std::string("not_found"));
    PB_CHECK(!e.store->set_attachment_error(0, 99999, "x", "network"));
}

PB_TEST(a_successful_download_clears_an_earlier_error) {
    Env e;
    PB_CHECK(e.store->set_attachment_error(0, e.note, "a.png", "network"));
    PB_CHECK(e.store->record_downloaded(e.note, "a.png", 5, "1.png"));
    PB_CHECK_EQ(e.store->attachment_for(e.note, "a.png")->error_key, std::string());
}

PB_TEST(get_and_lookup_return_nothing_for_unknown_rows) {
    Env e;
    PB_CHECK(!e.store->get_attachment(12345).has_value());
    PB_CHECK(!e.store->attachment_for(e.note, "nope.png").has_value());
    PB_CHECK(!e.store->attachment_for(99999, "nope.png").has_value());
}

PB_TEST(referenced_local_files_lists_every_row_that_has_a_file) {
    Env e;
    PB_CHECK(e.store->record_downloaded(e.note, "a.png", 5, "1.png"));
    PB_CHECK(e.store->record_downloaded(e.note, "b.pdf", 5, "2.pdf"));
    PB_CHECK(e.store->record_size(e.note, "c.png", 7));   // a remote row has no file
    auto files = e.store->referenced_local_files();
    std::sort(files.begin(), files.end());
    PB_CHECK((files == std::vector<std::string>{"1.png", "2.pdf"}));
}

PB_TEST(purge_unreferenced_downloads_keeps_what_the_text_still_names) {
    Env e("Note\n![x](.attachments.1/keep%20me.png)\n[f](.attachments.1/doc.pdf)\n| h |\n|---|\n| ![t](.attachments.1/cell.png) |");
    PB_CHECK(e.store->record_downloaded(e.note, ".attachments.1/keep me.png", 5, "1.png"));
    PB_CHECK(e.store->record_downloaded(e.note, ".attachments.1/doc.pdf", 5, "2.pdf"));
    PB_CHECK(e.store->record_downloaded(e.note, ".attachments.1/cell.png", 5, "3.png"));
    PB_CHECK(e.store->record_downloaded(e.note, ".attachments.1/gone.png", 5, "4.png"));
    PB_CHECK(e.store->record_size(e.note, ".attachments.1/remote.png", 5));     // a remote row is kept
    e.store->purge_unreferenced_downloads(e.note);
    PB_CHECK(e.store->attachment_for(e.note, ".attachments.1/keep me.png").has_value());
    PB_CHECK(e.store->attachment_for(e.note, ".attachments.1/doc.pdf").has_value());
    PB_CHECK(e.store->attachment_for(e.note, ".attachments.1/cell.png").has_value());
    PB_CHECK(!e.store->attachment_for(e.note, ".attachments.1/gone.png").has_value());
    PB_CHECK(e.store->attachment_for(e.note, ".attachments.1/remote.png").has_value());
}

PB_TEST(a_pull_that_removes_the_reference_deletes_the_downloaded_row) {
    Env e;
    PB_CHECK(e.store->record_downloaded(e.note, ".attachments.1/a.png", 5, "1.png"));
    // the server text no longer names the file
    ApiNote changed = server_note(1, "Note\nno attachment any more");
    changed.etag = "e1b";
    changed.modified = 200;
    e.store->apply_remote({changed});
    PB_CHECK(!e.store->attachment_for(e.note, ".attachments.1/a.png").has_value());
    PB_CHECK(e.store->referenced_local_files().empty());
}

PB_TEST(a_pull_that_keeps_the_reference_keeps_the_row) {
    Env e;
    PB_CHECK(e.store->record_downloaded(e.note, ".attachments.1/a.png", 5, "1.png"));
    ApiNote changed = server_note(1, "Note changed\n![x](.attachments.1/a.png)");
    changed.etag = "e1b";
    changed.modified = 200;
    e.store->apply_remote({changed});
    PB_CHECK(e.store->attachment_for(e.note, ".attachments.1/a.png").has_value());
}

PB_TEST(a_local_edit_that_drops_the_reference_keeps_the_row_until_the_next_pull) {
    Env e;
    PB_CHECK(e.store->record_downloaded(e.note, ".attachments.1/a.png", 5, "1.png"));
    PB_CHECK(e.store->save_local(e.note, pbnotes::derive_title("Note\nreference removed locally"), "Note\nreference removed locally", "", false, 1000).error == EditError::None);
    // the user may undo the edit: nothing is deleted by an edit
    PB_CHECK(e.store->attachment_for(e.note, ".attachments.1/a.png").has_value());
}

PB_TEST_MAIN()
