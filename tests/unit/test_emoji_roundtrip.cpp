// US3 (spec 002 FR-014..FR-017, SC-003): emojis survive parsing, storing, searching and a sync round trip
// byte for byte.
#include <string>

#include "fake_notes_server.h"
#include "fake_transport.h"
#include "pbnotes/json.h"
#include "pbnotes/note_store.h"
#include "pbnotes/sync_engine.h"
#include "pbnotes/title.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::NoteStore;

namespace {

const std::string kGrin = "\U0001F600";
const std::string kFamily = "\U0001F468‍\U0001F469‍\U0001F467";
const std::string kFlagDE = "\U0001F1E9\U0001F1EA";
const std::string kThumbSkin = "\U0001F44D\U0001F3FD";
const std::string kKeycap = "1️⃣";
const std::string kHeart = "❤️";
const std::string kEmojiLine = "Emojis: " + kGrin + " " + kFamily + " " + kFlagDE + " " + kThumbSkin + " " + kKeycap + " " + kHeart;

}  // namespace

PB_TEST(json_escapes_and_raw_utf8_parse_to_the_same_bytes) {
    auto escaped = pbnotes::json::parse(R"({"c":"😀"})");
    auto raw = pbnotes::json::parse("{\"c\":\"" + kGrin + "\"}");
    PB_CHECK(escaped && raw);
    PB_CHECK_EQ(*escaped->get_string("c"), kGrin);
    PB_CHECK_EQ(*raw->get_string("c"), kGrin);
}

PB_TEST(a_lone_surrogate_still_fails) {
    PB_CHECK(!pbnotes::json::parse(R"({"c":"\uD83D"})").has_value());
    PB_CHECK(!pbnotes::json::parse(R"({"c":"\uDE00"})").has_value());
}

PB_TEST(json_quote_keeps_four_byte_characters_as_raw_utf8) {
    const std::string q = pbnotes::json::quote(kEmojiLine);
    PB_CHECK(q.find(kGrin) != std::string::npos);
    auto doc = pbnotes::json::parse("{\"c\":" + q + "}");
    PB_CHECK(doc.has_value());
    PB_CHECK_EQ(*doc->get_string("c"), kEmojiLine);
}

PB_TEST(local_edits_keep_every_emoji_byte) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    const std::string text = kGrin + " Title\n" + kEmojiLine + "\n- [ ] task " + kFamily;
    auto r = store->create_local(pbnotes::derive_title(text), text, "Cat " + kFlagDE, false, 1000);
    PB_CHECK(r.error == pbnotes::EditError::None);
    PB_CHECK_EQ(store->get(r.local_id)->content, text);
    PB_CHECK_EQ(store->get(r.local_id)->category, "Cat " + kFlagDE);
    PB_CHECK_EQ(store->get(r.local_id)->title, kGrin + " Title");

    PB_CHECK(store->save_local(r.local_id, pbnotes::derive_title(text + "\nmore " + kThumbSkin), text + "\nmore " + kThumbSkin, "Cat " + kFlagDE, true, 1001).error == pbnotes::EditError::None);
    PB_CHECK_EQ(store->get(r.local_id)->content, text + "\nmore " + kThumbSkin);

    PB_CHECK(store->toggle_task(r.local_id, 2, 1002).error == pbnotes::EditError::None);
    PB_CHECK_EQ(store->get(r.local_id)->content, kGrin + " Title\n" + kEmojiLine + "\n- [x] task " + kFamily + "\nmore " + kThumbSkin);

    PB_CHECK(store->set_favorite(r.local_id, false, 1003).error == pbnotes::EditError::None);
    PB_CHECK(store->set_category(r.local_id, kHeart, 1004).error == pbnotes::EditError::None);
    PB_CHECK_EQ(store->get(r.local_id)->category, kHeart);
    PB_CHECK(store->get(r.local_id)->content.find(kEmojiLine) != std::string::npos);
}

PB_TEST(searching_for_an_emoji_finds_the_note) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->create_local(pbnotes::derive_title("Smile " + kGrin + " here"), "Smile " + kGrin + " here", "", false, 1);
    store->create_local(pbnotes::derive_title("Flag " + kFlagDE), "Flag " + kFlagDE, "", false, 2);
    store->create_local(pbnotes::derive_title("Nothing special"), "Nothing special", "", false, 3);
    pbnotes::ListQuery q;
    q.search = kGrin;
    PB_CHECK_EQ(store->list(q).size(), static_cast<size_t>(1));
    q.search = kFlagDE;
    PB_CHECK_EQ(store->list(q).size(), static_cast<size_t>(1));
    q.search = "SMILE";
    PB_CHECK_EQ(store->list(q).size(), static_cast<size_t>(1));
}

PB_TEST(fold_case_leaves_emojis_unchanged) {
    PB_CHECK_EQ(pbnotes::fold_case(kEmojiLine), "emojis: " + kGrin + " " + kFamily + " " + kFlagDE + " " + kThumbSkin + " " + kKeycap + " " + kHeart);
}

namespace {
struct Env {
    pbtest::TmpDir dir;
    std::unique_ptr<NoteStore> store;
    pbtest::FakeNotesServer server;
    pbtest::FakeTransport transport;
    Env() {
        store = NoteStore::open(dir.file("pbnotes.db"));
        pbnotes::Account a;
        a.server_url = "https://cloud.example.com";
        a.username = "alice";
        a.password = "app-password";
        store->save_account(a);
        transport.set_handler([this](const pbnotes::Request& r) { return server.handle(r); });
    }
    pbnotes::SyncResult run() {
        pbnotes::SyncEngine engine(*store, transport, [] { return 1700001000; });
        return engine.run();
    }
};
}  // namespace

PB_TEST(push_then_pull_returns_the_content_byte_for_byte) {
    Env e;
    const std::string text = kGrin + " Title\n" + kEmojiLine + "\n";
    auto r = e.store->create_local(pbnotes::derive_title(text), text, "", false, 1000);
    PB_CHECK(e.run().outcome == pbnotes::SyncOutcome::Success);
    PB_CHECK_EQ(e.server.notes.begin()->second.content, text);   // what the server received
    e.server.edit_on_server(e.server.notes.begin()->first, text + "more\n");
    PB_CHECK(e.run().outcome == pbnotes::SyncOutcome::Success);
    PB_CHECK_EQ(e.store->get(r.local_id)->content, text + "more\n");
}

PB_TEST(a_server_that_escapes_non_ascii_as_unicode_escapes_is_read_correctly) {
    Env e;
    e.server.escape_non_ascii = true;
    const std::string text = kGrin + " Title\n" + kEmojiLine + "\n";
    e.server.add_on_server(text, kFlagDE);
    PB_CHECK(e.run().outcome == pbnotes::SyncOutcome::Success);
    auto notes = e.store->list();
    PB_CHECK_EQ(notes.size(), static_cast<size_t>(1));
    PB_CHECK_EQ(e.store->get(notes[0].local_id)->content, text);
    PB_CHECK_EQ(e.store->get(notes[0].local_id)->category, kFlagDE);
    PB_CHECK_EQ(notes[0].title, kGrin + " Title");
}

PB_TEST(editing_an_unrelated_word_and_syncing_keeps_the_emoji_line_identical) {
    Env e;
    const std::string line = kEmojiLine;
    e.server.escape_non_ascii = true;
    const int64_t sid = e.server.add_on_server("Title\n" + line + "\nword one\n");
    PB_CHECK(e.run().outcome == pbnotes::SyncOutcome::Success);
    const int64_t local = e.store->get_by_remote_id(sid)->local_id;
    PB_CHECK(e.store->save_local(local, pbnotes::derive_title("Title\n" + line + "\nword two\n"), "Title\n" + line + "\nword two\n", "", false, 1100).error == pbnotes::EditError::None);
    PB_CHECK(e.run().outcome == pbnotes::SyncOutcome::Success);
    PB_CHECK_EQ(e.server.notes.at(sid).content, "Title\n" + line + "\nword two\n");
}

PB_TEST_MAIN()
