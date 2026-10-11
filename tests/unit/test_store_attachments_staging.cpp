// US5 (spec 002 FR-025..FR-030, data-model.md state machine, research R-3 and R-4): staging, binding to a
// note inside the save transaction, the upload-side store operations and conflicts with attachments.
#include <string>
#include <vector>

#include "pbnotes/attachment_ref.h"
#include "pbnotes/note_store.h"
#include "pbnotes/title.h"
#include "pbnotes/types.h"
#include "test_support.h"
#include "tmp_dir.h"

using namespace pbnotes;

namespace {

constexpr int64_t kDay = 1791633600;  // 2026-10-10

struct Env {
    pbtest::TmpDir dir;
    std::unique_ptr<NoteStore> store;
    Env() { store = NoteStore::open(dir.file("db")); }

    // A staged attachment whose file "exists" (the store only records its name).
    Attachment stage(const std::string& name = "photo.png", int64_t size = 1000) {
        auto a = store->stage_attachment(name, 100);
        PB_CHECK(a.has_value());
        PB_CHECK(store->set_staged_file(a->id, size, std::to_string(a->id) + "." + safe_extension(name)));
        return *store->get_attachment(a->id);
    }
    // A note whose text carries the token of `a`.
    int64_t note_with(const Attachment& a, const std::string& before = "Title\n") {
        auto r = store->create_local("Explicit title", before + make_token(a.id, a.display_name) + "\nafter", "", false, 200);
        PB_CHECK(r.error == EditError::None);
        return r.local_id;
    }
};

ApiNote server_note(int64_t id, const std::string& content) {
    ApiNote n;
    n.id = id;
    n.etag = "e" + std::to_string(id);
    n.content = content;
    n.modified = 100;
    return n;
}

}  // namespace

// ---- staging ------------------------------------------------------------------------------------------

PB_TEST(a_staged_attachment_has_no_note_and_derives_its_kind_and_name) {
    Env e;
    auto a = e.store->stage_attachment("My Photo.JPG", 100);
    PB_CHECK(a.has_value());
    PB_CHECK(a->state == AttachmentState::Staged);
    PB_CHECK(!a->note_local_id.has_value());
    PB_CHECK(!a->remote_path.has_value());
    PB_CHECK_EQ(a->display_name, std::string("My Photo.JPG"));
    PB_CHECK(a->kind == AttachmentKind::Image);
    PB_CHECK_EQ(a->local_file, std::string());
    PB_CHECK_EQ(e.store->stage_attachment("a\x01" "b.pdf", 100)->display_name, std::string("ab.pdf"));
    PB_CHECK(e.store->stage_attachment("a.pdf", 100)->kind == AttachmentKind::File);
}

PB_TEST(the_staged_file_name_is_checked_and_the_size_limited) {
    Env e;
    auto a = e.store->stage_attachment("a.png", 100);
    PB_CHECK(!e.store->set_staged_file(a->id, 10, "../x.png"));
    PB_CHECK(!e.store->set_staged_file(a->id, 10, "99.png"));                    // another row's name
    PB_CHECK(!e.store->set_staged_file(a->id, kMaxAttachmentBytes + 1, std::to_string(a->id) + ".png"));
    PB_CHECK(!e.store->set_staged_file(a->id, -1, std::to_string(a->id) + ".png"));
    PB_CHECK(e.store->set_staged_file(a->id, kMaxAttachmentBytes, std::to_string(a->id) + ".png"));
    auto b = e.store->get_attachment(a->id);
    PB_CHECK_EQ(b->size.value_or(-1), kMaxAttachmentBytes);
    PB_CHECK_EQ(b->local_file, std::to_string(a->id) + ".png");
    PB_CHECK(!e.store->set_staged_file(424242, 10, "424242.png"));                // unknown row
}

PB_TEST(a_staged_row_can_be_dropped_but_nothing_else) {
    Env e;
    auto a = e.stage();
    PB_CHECK(e.store->drop_staged(a.id));
    PB_CHECK(!e.store->get_attachment(a.id).has_value());
    PB_CHECK(!e.store->drop_staged(a.id));
    auto b = e.stage();
    const int64_t note = e.note_with(b);
    PB_CHECK(!e.store->drop_staged(b.id));    // now pending: bound to a note
    PB_CHECK(e.store->get_attachment(b.id).has_value());
    (void)note;
}

// ---- binding inside the save ---------------------------------------------------------------------------

PB_TEST(creating_a_note_binds_its_staged_attachments_and_makes_them_pending) {
    Env e;
    auto a = e.stage("a.png");
    auto b = e.stage("b.pdf");
    auto unused = e.stage("c.png");   // no token in the text
    auto r = e.store->create_local(pbnotes::derive_title("Title\n" + make_token(a.id, "a.png") + "\n" + make_token(b.id, "b.pdf")), "Title\n" + make_token(a.id, "a.png") + "\n" + make_token(b.id, "b.pdf"), "", false, 100);
    PB_CHECK(r.error == EditError::None);
    for (int64_t id : {a.id, b.id}) {
        auto row = e.store->get_attachment(id);
        PB_CHECK(row->state == AttachmentState::Pending);
        PB_CHECK_EQ(row->note_local_id.value_or(0), r.local_id);
    }
    PB_CHECK(e.store->get_attachment(unused.id)->state == AttachmentState::Staged);
    PB_CHECK_EQ(e.store->pending_attachments(r.local_id).size(), static_cast<size_t>(2));
}

PB_TEST(saving_a_note_binds_the_staged_attachments_named_by_its_tokens) {
    Env e;
    e.store->apply_remote({server_note(1, "Synced note")});
    const int64_t note = e.store->get_by_remote_id(1)->local_id;
    auto a = e.stage();
    PB_CHECK(e.store->save_local(note, pbnotes::derive_title("Synced note\n" + make_token(a.id, "photo.png")), "Synced note\n" + make_token(a.id, "photo.png"), "", false, 500).error == EditError::None);
    auto row = e.store->get_attachment(a.id);
    PB_CHECK(row->state == AttachmentState::Pending);
    PB_CHECK_EQ(row->note_local_id.value_or(0), note);
    PB_CHECK(e.store->get(note)->status == NoteStatus::Edited);
}

PB_TEST(a_save_that_fails_leaves_the_note_and_the_staged_rows_unchanged) {
    for (int n = 1; n <= 3; ++n) {
        Env e;
        e.store->apply_remote({server_note(1, "Original")});
        const int64_t note = e.store->get_by_remote_id(1)->local_id;
        auto a = e.stage();
        e.store->test_fail_nth_write(n);
        auto r = e.store->save_local(note, pbnotes::derive_title("Original\n" + make_token(a.id, "photo.png")), "Original\n" + make_token(a.id, "photo.png"), "", false, 500);
        e.store->test_fail_nth_write(0);
        if (r.error == EditError::None) continue;   // this injection point is past the save's last write
        PB_CHECK_EQ(e.store->get(note)->content, std::string("Original"));
        PB_CHECK(e.store->get(note)->status == NoteStatus::Void);
        auto row = e.store->get_attachment(a.id);
        PB_CHECK(row->state == AttachmentState::Staged);
        PB_CHECK(!row->note_local_id.has_value());
    }
}

PB_TEST(a_create_that_fails_leaves_no_note_and_the_row_staged) {
    for (int n = 1; n <= 3; ++n) {
        Env e;
        auto a = e.stage();
        e.store->test_fail_nth_write(n);
        auto r = e.store->create_local(pbnotes::derive_title("T\n" + make_token(a.id, "photo.png")), "T\n" + make_token(a.id, "photo.png"), "", false, 500);
        e.store->test_fail_nth_write(0);
        if (r.error == EditError::None) continue;
        PB_CHECK(e.store->list().empty());
        PB_CHECK(e.store->get_attachment(a.id)->state == AttachmentState::Staged);
    }
}

PB_TEST(unknown_tokens_and_tokens_of_other_notes_are_ignored) {
    Env e;
    auto a = e.stage();
    const int64_t first = e.note_with(a);
    // the same token pasted into another note: it stays literal text and does not steal the row
    auto r = e.store->create_local(pbnotes::derive_title("Other\n" + make_token(a.id, "photo.png") + "\n" + make_token(9999, "ghost.png")), "Other\n" + make_token(a.id, "photo.png") + "\n" + make_token(9999, "ghost.png"), "", false, 300);
    PB_CHECK(r.error == EditError::None);
    PB_CHECK_EQ(e.store->get_attachment(a.id)->note_local_id.value_or(0), first);
    PB_CHECK(!e.store->get_attachment(9999).has_value());
}

PB_TEST(re_saving_a_note_with_its_pending_attachment_keeps_it_pending) {
    Env e;
    auto a = e.stage();
    const int64_t note = e.note_with(a);
    PB_CHECK(e.store->save_local(note, pbnotes::derive_title("Title\n" + make_token(a.id, "photo.png") + "\nchanged text"), "Title\n" + make_token(a.id, "photo.png") + "\nchanged text", "", false, 400).error == EditError::None);
    PB_CHECK(e.store->get_attachment(a.id)->state == AttachmentState::Pending);
    PB_CHECK_EQ(e.store->get_attachment(a.id)->note_local_id.value_or(0), note);
}

// ---- discarding -----------------------------------------------------------------------------------------

PB_TEST(discard_staged_drops_unbound_rows_and_keeps_bound_ones) {
    Env e;
    auto kept = e.stage("kept.png");
    const int64_t note = e.note_with(kept);
    auto loose1 = e.stage("loose1.png");
    auto loose2 = e.stage("loose2.pdf");
    PB_CHECK(e.store->discard_staged());
    PB_CHECK(!e.store->get_attachment(loose1.id).has_value());
    PB_CHECK(!e.store->get_attachment(loose2.id).has_value());
    PB_CHECK(e.store->get_attachment(kept.id).has_value());
    PB_CHECK_EQ(e.store->pending_attachments(note).size(), static_cast<size_t>(1));
}

PB_TEST(the_start_up_cleanup_binds_a_staged_row_whose_token_is_in_a_saved_note) {
    Env e;
    auto a = e.stage();
    const int64_t note = e.note_with(a);
    // simulate a crash between "note saved" and "row bound"
    PB_CHECK(e.store->debug_exec("UPDATE attachments SET state='staged', note_local_id=NULL WHERE id=" + std::to_string(a.id)));
    PB_CHECK(e.store->discard_staged());
    auto row = e.store->get_attachment(a.id);
    PB_CHECK(row.has_value());
    PB_CHECK(row->state == AttachmentState::Pending);
    PB_CHECK_EQ(row->note_local_id.value_or(0), note);
}

PB_TEST(pending_attachments_lists_only_the_pending_rows_of_that_note) {
    Env e;
    auto a = e.stage("a.png");
    auto b = e.stage("b.png");
    const int64_t n1 = e.note_with(a);
    const int64_t n2 = e.note_with(b);
    auto p1 = e.store->pending_attachments(n1);
    PB_CHECK_EQ(p1.size(), static_cast<size_t>(1));
    PB_CHECK_EQ(p1[0].id, a.id);
    PB_CHECK_EQ(e.store->pending_attachments(n2).size(), static_cast<size_t>(1));
    PB_CHECK(e.store->pending_attachments(99999).empty());
}

// ---- upload -----------------------------------------------------------------------------------------------

PB_TEST(record_uploaded_stores_the_path_rewrites_the_token_and_changes_nothing_else) {
    Env e;
    auto a = e.stage("photo one.png", 4321);
    const int64_t note = e.note_with(a);
    const Note before = *e.store->get(note);
    PB_CHECK(e.store->record_uploaded(a.id, ".attachments.12/photo one.png", 4321));

    auto row = e.store->get_attachment(a.id);
    PB_CHECK(row->state == AttachmentState::Downloaded);
    PB_CHECK_EQ(row->remote_path.value_or(""), std::string(".attachments.12/photo one.png"));
    PB_CHECK_EQ(row->size.value_or(-1), static_cast<int64_t>(4321));
    PB_CHECK_EQ(row->local_file, std::to_string(a.id) + ".png");   // the local copy is kept
    PB_CHECK_EQ(row->error_key, std::string());

    const Note after = *e.store->get(note);
    PB_CHECK_EQ(after.content, "Title\n![photo one.png](.attachments.12/photo%20one.png)\nafter");
    PB_CHECK_EQ(after.title, std::string("Explicit title"));   // the upload rewrites the text, never the title (spec 004)
    PB_CHECK_EQ(after.title, before.title);
    PB_CHECK_EQ(after.excerpt, make_excerpt(after.content, after.title));
    PB_CHECK_EQ(after.modified, before.modified);
    PB_CHECK(after.status == before.status);
    PB_CHECK_EQ(after.etag.value_or("-"), before.etag.value_or("-"));
    PB_CHECK_EQ(after.category, before.category);
    PB_CHECK(e.store->pending_attachments(note).empty());
}

PB_TEST(record_uploaded_keeps_the_title_even_when_the_token_is_the_first_line) {
    Env e;
    auto a = e.stage("photo.png", 10);
    const int64_t note = e.note_with(a, "");   // the text starts with the token line
    PB_CHECK(e.store->record_uploaded(a.id, ".attachments.12/photo.png", 10));
    const Note after = *e.store->get(note);
    PB_CHECK_EQ(after.title, std::string("Explicit title"));
    PB_CHECK(after.content.find("pbatt:") == std::string::npos);
    PB_CHECK_EQ(after.excerpt, make_excerpt(after.content, "Explicit title"));
    PB_CHECK(after.excerpt.find("photo.png") != std::string::npos);   // the first line is part of the preview now
}

PB_TEST(drop_dead_tokens_keeps_the_title) {
    Env e;
    auto a = e.stage();
    const int64_t note = e.note_with(a);
    PB_CHECK(e.store->drop_staged(a.id) == false);   // bound to the note, cannot be dropped
    PB_CHECK(e.store->get(note)->title == "Explicit title");
}

PB_TEST(record_uploaded_is_idempotent) {
    Env e;
    auto a = e.stage();
    const int64_t note = e.note_with(a);
    PB_CHECK(e.store->record_uploaded(a.id, ".attachments.1/photo.png", 1000));
    const std::string content = e.store->get(note)->content;
    PB_CHECK(e.store->record_uploaded(a.id, ".attachments.1/photo.png", 1000));
    PB_CHECK_EQ(e.store->get(note)->content, content);
    PB_CHECK(e.store->get_attachment(a.id)->state == AttachmentState::Downloaded);
}

PB_TEST(record_uploaded_refuses_bad_input_and_unknown_or_unbound_rows) {
    Env e;
    auto a = e.stage();
    PB_CHECK(!e.store->record_uploaded(a.id, ".attachments.1/photo.png", 1000));   // still staged: no note
    const int64_t note = e.note_with(a);
    PB_CHECK(!e.store->record_uploaded(a.id, "/abs/photo.png", 1000));
    PB_CHECK(!e.store->record_uploaded(a.id, "dir/", 1000));
    PB_CHECK(!e.store->record_uploaded(a.id, ".attachments.1/photo.png", kMaxAttachmentBytes + 1));
    PB_CHECK(!e.store->record_uploaded(424242, ".attachments.1/photo.png", 1000));
    PB_CHECK(e.store->get_attachment(a.id)->state == AttachmentState::Pending);
    PB_CHECK(e.store->get(note)->content.find("pbatt:") != std::string::npos);
}

PB_TEST(a_failed_record_uploaded_rolls_everything_back) {
    for (int n = 1; n <= 3; ++n) {
        Env e;
        auto a = e.stage();
        const int64_t note = e.note_with(a);
        const Note before = *e.store->get(note);
        e.store->test_fail_nth_write(n);
        const bool ok = e.store->record_uploaded(a.id, ".attachments.1/photo.png", 1000);
        e.store->test_fail_nth_write(0);
        if (ok) continue;
        PB_CHECK_EQ(e.store->get(note)->content, before.content);
        PB_CHECK(e.store->get_attachment(a.id)->state == AttachmentState::Pending);
        PB_CHECK(!e.store->get_attachment(a.id)->remote_path.has_value());
    }
}

// ---- conflicts ----------------------------------------------------------------------------------------------

namespace {

// A synced note (server id 7) edited locally: it carries a pending attachment, a downloaded one referenced by
// its server path, and a reference to a file that is only on the server.
struct Conflict {
    Env e;
    int64_t note = 0, pending_id = 0, downloaded_id = 0;
    std::string local_text;
    Conflict() {
        e.store->apply_remote({server_note(7, "Plan\nold text\n![d](.attachments.7/d.png)\n")});
        note = e.store->get_by_remote_id(7)->local_id;
        downloaded_id = e.store->ensure_attachment(note, ".attachments.7/d.png")->id;
        PB_CHECK(e.store->record_downloaded(note, ".attachments.7/d.png", 50, std::to_string(downloaded_id) + ".png"));
        auto p = e.stage("new.png");
        pending_id = p.id;
        local_text = "Plan\nmy edit\n![d](.attachments.7/d.png)\n" + make_token(p.id, "new.png") + "\n![r](.attachments.7/remote.pdf)\n";
        PB_CHECK(e.store->save_local(note, pbnotes::derive_title(local_text), local_text, "", false, 500).error == EditError::None);
    }
    ApiNote server() const { ApiNote s = server_note(7, "Plan\nthe server's edit\n"); s.etag = "v2"; s.modified = 900; return s; }
};

}  // namespace

PB_TEST(a_conflict_copy_takes_the_pending_attachments_with_it) {
    Conflict c;
    auto res = c.e.store->resolve_conflict(c.note, c.server(), kDay);
    PB_CHECK(res.ok && res.created_copy);
    auto row = c.e.store->get_attachment(c.pending_id);
    PB_CHECK(row->state == AttachmentState::Pending);
    PB_CHECK_EQ(row->note_local_id.value_or(0), res.copy_local_id);
    PB_CHECK(c.e.store->get(res.copy_local_id)->content.find("pbatt:" + std::to_string(c.pending_id)) != std::string::npos);
    PB_CHECK(c.e.store->pending_attachments(c.note).empty());
}

PB_TEST(a_downloaded_attachment_is_duplicated_for_the_copy_as_a_pending_one) {
    Conflict c;
    const std::string file = c.e.store->get_attachment(c.downloaded_id)->local_file;
    auto res = c.e.store->resolve_conflict(c.note, c.server(), kDay);
    PB_CHECK(res.ok);
    auto pending = c.e.store->pending_attachments(res.copy_local_id);
    PB_CHECK_EQ(pending.size(), static_cast<size_t>(2));   // the moved one and the duplicate
    const Attachment* dup = nullptr;
    for (const auto& p : pending)
        if (p.id != c.pending_id) dup = &p;
    PB_CHECK(dup != nullptr);
    PB_CHECK_EQ(dup->display_name, std::string("d.png"));
    PB_CHECK(dup->kind == AttachmentKind::Image);
    PB_CHECK_EQ(dup->local_file, file);               // the local file is shared, not copied
    PB_CHECK(!dup->remote_path.has_value());
    const std::string text = c.e.store->get(res.copy_local_id)->content;
    PB_CHECK(text.find(".attachments.7/d.png") == std::string::npos);                       // the path was replaced
    PB_CHECK(text.find("![d](pbatt:" + std::to_string(dup->id) + ")") != std::string::npos); // by the new token
}

PB_TEST(remote_only_references_stay_paths_in_the_copy) {
    Conflict c;
    auto res = c.e.store->resolve_conflict(c.note, c.server(), kDay);
    PB_CHECK(c.e.store->get(res.copy_local_id)->content.find("![r](.attachments.7/remote.pdf)") != std::string::npos);
}

PB_TEST(the_original_adopts_the_server_text_and_loses_attachments_it_no_longer_names) {
    Conflict c;
    const std::string file = c.e.store->get_attachment(c.downloaded_id)->local_file;
    auto res = c.e.store->resolve_conflict(c.note, c.server(), kDay);
    PB_CHECK(res.ok);
    PB_CHECK_EQ(c.e.store->get(c.note)->content, std::string("Plan\nthe server's edit\n"));
    PB_CHECK(!c.e.store->get_attachment(c.downloaded_id).has_value());   // the server text does not reference it
    const auto files = c.e.store->referenced_local_files();
    int count = 0;
    for (const auto& f : files) count += f == file;
    PB_CHECK_EQ(count, 1);   // still named by the copy's duplicate: the file survives the janitor
}

PB_TEST(a_conflict_that_fails_rolls_back_with_the_copy_and_the_attachments) {
    int failures = 0;
    for (int n = 1; n <= 40; ++n) {
        Conflict c;
        c.e.store->test_fail_nth_write(n);
        auto res = c.e.store->resolve_conflict(c.note, c.server(), kDay);
        c.e.store->test_fail_nth_write(0);
        if (res.ok) break;   // past the last write of the operation
        ++failures;
        PB_CHECK_EQ(c.e.store->get(c.note)->content, c.local_text);
        PB_CHECK(c.e.store->get(c.note)->status == NoteStatus::Edited);
        PB_CHECK_EQ(c.e.store->list().size(), static_cast<size_t>(1));          // no copy
        auto p = c.e.store->get_attachment(c.pending_id);
        PB_CHECK(p->state == AttachmentState::Pending);
        PB_CHECK_EQ(p->note_local_id.value_or(0), c.note);
        PB_CHECK(c.e.store->get_attachment(c.downloaded_id).has_value());
        PB_CHECK_EQ(c.e.store->pending_attachments(c.note).size(), static_cast<size_t>(1));
    }
    PB_CHECK(failures >= 4);   // the injection really hit several points
}

PB_TEST(a_conflict_without_attachments_still_works_as_before) {
    Env e;
    e.store->apply_remote({server_note(7, "Plan\nold\n")});
    const int64_t note = e.store->get_by_remote_id(7)->local_id;
    PB_CHECK(e.store->save_local(note, pbnotes::derive_title("Plan\nmine\n"), "Plan\nmine\n", "", false, 500).error == EditError::None);
    ApiNote s = server_note(7, "Plan\ntheirs\n");
    s.etag = "v2";
    auto res = e.store->resolve_conflict(note, s, kDay);
    PB_CHECK(res.ok && res.created_copy);
    PB_CHECK(e.store->pending_attachments(res.copy_local_id).empty());
}

PB_TEST_MAIN()
