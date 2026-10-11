// T032/T035/T040 (host): the Qt bridge for the setup flow, run against real local HTTPS servers.
// Covers the connection test messages, the CA handling (device/bundled + custom-ca.pem), saving and
// forgetting the account, and that the password is never exposed back. Needs the `openssl` CLI.
#include <atomic>
#include <ctime>

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QMetaProperty>
#include <QTimer>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include "notes_bridge.h"
#include "pbnotes/note_store.h"
#include "pbnotes/title.h"
#include "test_support.h"
#include "tls_test_server.h"

namespace {

struct Result {
    bool done = false;
    bool ok = false;
    QString key, message, url;
};

Result run_test(NotesBridge& b, const QString& url, const QString& user, const QString& pw) {
    Result r;
    QEventLoop loop;
    QObject::connect(&b, &NotesBridge::connectionTested, &loop,
                     [&](bool ok, const QString& key, const QString& msg, const QString& norm) {
                         r.done = true; r.ok = ok; r.key = key; r.message = msg; r.url = norm;
                         loop.quit();
                     });
    QTimer::singleShot(20000, &loop, &QEventLoop::quit);
    b.testConnection(url, user, pw);
    if (!r.done) loop.exec();
    return r;
}

const char* kCapabilities = R"({"ocs":{"data":{"capabilities":{"notes":{"api_version":["0.2","1.0"]}}}}})";

pbtest::TlsTestServer::Handler notes_server(const std::string& body = kCapabilities, int status = 200) {
    return [body, status](const std::string&) { return pbtest::TlsTestServer::http_response(status, body); };
}

NotesBridge::Config config_for(const pbtest::TmpDir& d) {
    NotesBridge::Config c;
    c.dataDir = QString::fromStdString(d.path());
    c.bundledCaPath = QString::fromStdString(std::string(PBNOTES_SOURCE_DIR) + "/third_party/cacert/cacert.pem");
    return c;  // no device CA candidates: the bundled file is the base
}

std::string slurp_file(const std::string& path) {
    QFile f(QString::fromStdString(path));
    if (!f.open(QIODevice::ReadOnly)) return std::string();
    return f.readAll().toStdString();
}

}  // namespace

PB_TEST(connection_works_with_a_private_ca_in_custom_ca_pem) {
    pbtest::TmpDir d;
    pbtest::TestPki pki(d, "privateca");
    if (!pki.available()) PB_SKIP("openssl CLI not available");
    auto leaf = pki.issue("server", "DNS:localhost,IP:127.0.0.1");
    PB_CHECK(leaf.ok);
    pbtest::TlsTestServer server(leaf.cert, leaf.key, notes_server());
    pbtest::TmpDir data;
    data.write("custom-ca.pem", slurp_file(pki.ca_cert()));
    NotesBridge bridge(config_for(data));
    PB_CHECK(bridge.property("customCaStatus").toString() == "loaded");
    PB_CHECK_EQ(bridge.property("customCaCertificates").toInt(), 1);
    auto r = run_test(bridge, QString::fromStdString(server.url("/")), "alice", "app-password");
    PB_CHECK(r.done);
    PB_CHECK(r.ok);
    PB_CHECK_EQ(r.key.toStdString(), std::string("ok"));
    PB_CHECK_EQ(r.url.toStdString(), "https://localhost:" + std::to_string(server.port()));
    PB_CHECK(server.saw_authorization());
}

PB_TEST(without_the_custom_ca_the_certificate_is_refused_and_nothing_is_sent) {
    pbtest::TmpDir d;
    pbtest::TestPki pki(d, "privateca");
    if (!pki.available()) PB_SKIP("openssl CLI not available");
    auto leaf = pki.issue("server", "DNS:localhost,IP:127.0.0.1");
    pbtest::TlsTestServer server(leaf.cert, leaf.key, notes_server());
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    PB_CHECK(bridge.property("customCaStatus").toString() == "none");
    auto r = run_test(bridge, QString::fromStdString(server.url("/")), "alice", "app-password");
    PB_CHECK(r.done && !r.ok);
    PB_CHECK_EQ(r.key.toStdString(), std::string("certificate_unverifiable"));
    PB_CHECK(!r.message.isEmpty());
    PB_CHECK(!server.saw_authorization());
}

PB_TEST(an_invalid_custom_ca_file_is_reported_and_ignored) {
    pbtest::TmpDir data;
    data.write("custom-ca.pem", "this is not a certificate");
    NotesBridge bridge(config_for(data));
    PB_CHECK(bridge.property("customCaStatus").toString() == "invalid");
    PB_CHECK_EQ(bridge.property("customCaCertificates").toInt(), 0);
    PB_CHECK(bridge.property("caSource").toString() == "bundled");
}

PB_TEST(rejected_credentials_and_unsupported_servers_get_their_own_messages) {
    pbtest::TmpDir d;
    pbtest::TestPki pki(d, "privateca");
    if (!pki.available()) PB_SKIP("openssl CLI not available");
    auto leaf = pki.issue("server", "DNS:localhost,IP:127.0.0.1");
    pbtest::TmpDir data;
    data.write("custom-ca.pem", slurp_file(pki.ca_cert()));
    NotesBridge bridge(config_for(data));
    {
        pbtest::TlsTestServer server(leaf.cert, leaf.key, notes_server("{}", 401));
        auto r = run_test(bridge, QString::fromStdString(server.url("/")), "alice", "wrong");
        PB_CHECK_EQ(r.key.toStdString(), std::string("auth_rejected"));
    }
    {
        pbtest::TlsTestServer server(leaf.cert, leaf.key,
                                     notes_server(R"({"ocs":{"data":{"capabilities":{"notes":{"api_version":["0.2"]}}}}})"));
        auto r = run_test(bridge, QString::fromStdString(server.url("/")), "alice", "pw");
        PB_CHECK_EQ(r.key.toStdString(), std::string("server_unsupported"));
    }
}

PB_TEST(input_errors_are_reported_without_any_network_traffic) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    PB_CHECK_EQ(run_test(bridge, "http://cloud.example.com", "alice", "pw").key.toStdString(), std::string("url_not_https"));
    PB_CHECK_EQ(run_test(bridge, "", "alice", "pw").key.toStdString(), std::string("url_empty"));
    PB_CHECK_EQ(run_test(bridge, "https://cloud.example.com", "", "pw").key.toStdString(), std::string("credentials_missing"));
    PB_CHECK_EQ(run_test(bridge, "https://cloud.example.com", "alice", "").key.toStdString(), std::string("credentials_missing"));
    PB_CHECK(!bridge.message("url_not_https").isEmpty());
}

PB_TEST(a_connection_to_nowhere_is_reported_as_unreachable) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    auto r = run_test(bridge, "https://localhost:1", "alice", "pw");  // nothing listens on port 1
    PB_CHECK_EQ(r.key.toStdString(), std::string("server_unreachable"));
}

PB_TEST(saving_the_account_persists_it_and_never_exposes_the_password) {
    pbtest::TmpDir data;
    {
        NotesBridge bridge(config_for(data));
        PB_CHECK(!bridge.property("configured").toBool());
        PB_CHECK(bridge.saveAccount("https://Cloud.Example.com/", "alice", "secret-app-password"));
        PB_CHECK(bridge.property("configured").toBool());
        PB_CHECK_EQ(bridge.property("serverUrl").toString().toStdString(), std::string("https://cloud.example.com"));
        PB_CHECK_EQ(bridge.property("username").toString().toStdString(), std::string("alice"));
        // no property returns the password
        const QMetaObject* mo = bridge.metaObject();
        for (int i = 0; i < mo->propertyCount(); ++i)
            PB_CHECK(!bridge.property(mo->property(i).name()).toString().contains("secret-app-password"));
    }
    NotesBridge again(config_for(data));
    PB_CHECK(again.property("configured").toBool());  // survives a restart
    PB_CHECK_EQ(again.property("username").toString().toStdString(), std::string("alice"));
}

PB_TEST(saving_rejects_bad_input_and_a_second_account) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    PB_CHECK(!bridge.saveAccount("http://cloud.example.com", "alice", "pw"));
    PB_CHECK(!bridge.saveAccount("https://cloud.example.com", "", "pw"));
    PB_CHECK(!bridge.property("configured").toBool());
    PB_CHECK(bridge.saveAccount("https://cloud.example.com", "alice", "pw"));
    PB_CHECK(!bridge.saveAccount("https://other.example.com", "bob", "pw"));  // one account: forget first
    PB_CHECK_EQ(bridge.property("username").toString().toStdString(), std::string("alice"));
    PB_CHECK(bridge.saveAccount("https://cloud.example.com", "alice", "new-password"));  // same account: password change
}

PB_TEST(a_failed_connection_test_keeps_the_entries_for_the_next_start) {
    pbtest::TmpDir data;
    {
        NotesBridge bridge(config_for(data));
        PB_CHECK(bridge.draft().isEmpty());
        bridge.testConnection("http://not-https.example.com/long/path", "alice", "long-app-password");  // fails early
        QCoreApplication::processEvents();
    }
    NotesBridge again(config_for(data));
    const QVariantMap d = again.draft();
    PB_CHECK_EQ(d.value("url").toString().toStdString(), std::string("http://not-https.example.com/long/path"));
    PB_CHECK_EQ(d.value("username").toString().toStdString(), std::string("alice"));
    PB_CHECK_EQ(d.value("password").toString().toStdString(), std::string("long-app-password"));
    PB_CHECK(again.saveAccount("https://cloud.example.com", "alice", "long-app-password"));
    PB_CHECK(again.draft().isEmpty());  // a saved account replaces the draft
}

PB_TEST(draft_is_stored_when_asked_and_cleared_by_forgetting) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    bridge.saveDraft("https://cloud.example.com", "alice", "pw-1");
    PB_CHECK_EQ(bridge.draft().value("password").toString().toStdString(), std::string("pw-1"));
    bridge.saveDraft("", "", "");
    PB_CHECK(bridge.draft().isEmpty());
}

PB_TEST(forgetting_the_account_is_refused_while_changes_are_pending) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    bridge.saveAccount("https://cloud.example.com", "alice", "pw");
    {
        auto store = pbnotes::NoteStore::open(data.file("pbnotes.db"));
        store->create_local(pbnotes::derive_title("Unsynced note\nbody"), "Unsynced note\nbody", "", false, 1000);
    }
    PB_CHECK(!bridge.forgetAccount());
    PB_CHECK(bridge.property("configured").toBool());
    {
        auto store = pbnotes::NoteStore::open(data.file("pbnotes.db"));
        for (const auto& n : store->pending_notes()) store->mark_deleted(n.local_id);
    }
    PB_CHECK(bridge.forgetAccount());
    PB_CHECK(!bridge.property("configured").toBool());
}

PB_TEST(the_rotation_preference_is_validated_and_persisted) {
    pbtest::TmpDir data;
    {
        NotesBridge bridge(config_for(data));
        PB_CHECK_EQ(bridge.property("rotationPreference").toString().toStdString(), std::string("auto"));
        bridge.setProperty("rotationPreference", "landscape");
        PB_CHECK_EQ(bridge.property("rotationPreference").toString().toStdString(), std::string("landscape"));
        bridge.setProperty("rotationPreference", "diagonal");  // ignored
        PB_CHECK_EQ(bridge.property("rotationPreference").toString().toStdString(), std::string("landscape"));
    }
    NotesBridge again(config_for(data));
    PB_CHECK_EQ(again.property("rotationPreference").toString().toStdString(), std::string("landscape"));
}

// ---- User Story 2: note list, reader, font size, sync ----------------------------------------------

namespace {
pbnotes::ApiNote remote_note(int64_t id, const std::string& content, const std::string& category, int64_t modified,
                             bool favorite = false) {
    pbnotes::ApiNote n;
    n.id = id;
    n.etag = "etag" + std::to_string(id);
    n.content = content;
    n.category = category;
    n.modified = modified;
    n.favorite = favorite;
    return n;
}

void seed(const pbtest::TmpDir& data, const std::vector<pbnotes::ApiNote>& notes) {
    auto store = pbnotes::NoteStore::open(data.file("pbnotes.db"));
    store->apply_remote(notes);
}
}  // namespace

PB_TEST(the_note_list_comes_from_the_store_newest_first) {
    pbtest::TmpDir data;
    seed(data, {remote_note(1, "Old note\nbody", "Home", 100), remote_note(2, "New note\nmore", "", 300, true)});
    NotesBridge bridge(config_for(data));
    const QVariantList rows = bridge.listNotes();
    PB_CHECK_EQ(rows.size(), 2);
    const QVariantMap first = rows.at(0).toMap();
    PB_CHECK_EQ(first.value("title").toString().toStdString(), std::string("New note"));
    PB_CHECK(first.value("favorite").toBool());
    PB_CHECK_EQ(first.value("modified").toLongLong(), static_cast<qlonglong>(300));
    PB_CHECK_EQ(rows.at(1).toMap().value("category").toString().toStdString(), std::string("Home"));
    PB_CHECK(first.contains("localId") && first.contains("excerpt") && first.contains("pending"));
}

PB_TEST(opening_a_note_returns_styled_escaped_blocks) {
    pbtest::TmpDir data;
    seed(data, {remote_note(1, "# Plan <b>\nSome **bold** and *soft* & text\n- [ ] milk\n- [x] bread\n1. first\n", "Home", 100)});
    NotesBridge bridge(config_for(data));
    const qlonglong id = bridge.listNotes().at(0).toMap().value("localId").toLongLong();
    const QVariantMap note = bridge.openNote(id);
    PB_CHECK_EQ(note.value("title").toString().toStdString(), std::string("Plan <b>"));  // the heading marker is not shown
    const QVariantList blocks = note.value("blocks").toList();
    PB_CHECK(blocks.size() >= 5);
    const QVariantMap heading = blocks.at(0).toMap();
    PB_CHECK_EQ(heading.value("kind").toString().toStdString(), std::string("heading"));
    PB_CHECK_EQ(heading.value("level").toInt(), 1);
    PB_CHECK_EQ(heading.value("html").toString().toStdString(), std::string("Plan &lt;b&gt;"));  // escaped, no injected tag
    PB_CHECK_EQ(blocks.at(1).toMap().value("html").toString().toStdString(),
                std::string("Some <b>bold</b> and <i>soft</i> &amp; text"));
    const QVariantMap open = blocks.at(2).toMap(), done = blocks.at(3).toMap();
    PB_CHECK(open.value("task").toBool() && !open.value("checked").toBool());
    PB_CHECK(done.value("task").toBool() && done.value("checked").toBool());
    PB_CHECK_EQ(open.value("line").toInt(), 2);
    PB_CHECK(blocks.at(4).toMap().value("ordered").toBool());
}

PB_TEST(a_note_without_text_still_gets_a_visible_title) {
    pbtest::TmpDir data;
    seed(data, {remote_note(1, "", "", 100), remote_note(2, "  \n\n", "", 90), remote_note(3, "#\nbody", "", 80)});
    NotesBridge bridge(config_for(data));
    const QVariantList rows = bridge.listNotes();
    PB_CHECK_EQ(rows.size(), 3);
    for (const QVariant& r : rows) PB_CHECK(!r.toMap().value("title").toString().trimmed().isEmpty());
}

PB_TEST(opening_an_unknown_note_returns_an_empty_map) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    PB_CHECK(bridge.openNote(12345).isEmpty());
}

PB_TEST(font_size_is_validated_persisted_and_mapped_to_pixels) {
    pbtest::TmpDir data;
    int small_px = 0, large_px = 0;
    {
        NotesBridge bridge(config_for(data));
        PB_CHECK_EQ(bridge.property("fontSize").toString().toStdString(), std::string("medium"));
        PB_CHECK(bridge.setProperty("fontSize", "small"));
        small_px = bridge.property("fontPx").toInt();
        bridge.setProperty("fontSize", "huge");  // rejected, stays
        PB_CHECK_EQ(bridge.property("fontSize").toString().toStdString(), std::string("small"));
        bridge.setProperty("fontSize", "large");
        large_px = bridge.property("fontPx").toInt();
    }
    PB_CHECK(small_px >= 24 && large_px > small_px);
    NotesBridge again(config_for(data));
    PB_CHECK_EQ(again.property("fontSize").toString().toStdString(), std::string("large"));
}

PB_TEST(times_are_shown_in_the_c_library_local_time) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    const std::time_t t = 1760000000;
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", &tm);
    PB_CHECK_EQ(bridge.formatTime(1760000000).toStdString(), std::string(buf));
}

PB_TEST(sync_status_text_is_never_empty) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    PB_CHECK(!bridge.property("syncStatus").toString().isEmpty());
    PB_CHECK_EQ(bridge.property("syncOutcome").toString().toStdString(), std::string("never"));
    PB_CHECK(!bridge.property("syncRunning").toBool());
}

PB_TEST(syncing_pulls_the_notes_and_reports_the_outcome) {
    pbtest::TmpDir d;
    pbtest::TestPki pki(d, "privateca");
    if (!pki.available()) PB_SKIP("openssl CLI not available");
    auto leaf = pki.issue("server", "DNS:localhost,IP:127.0.0.1");
    const std::string list = R"([{"id":7,"etag":"e7","readonly":false,"title":"Pulled note","content":"Pulled note\nbody","category":"Work","favorite":false,"modified":1700000000}])";
    pbtest::TlsTestServer server(leaf.cert, leaf.key, [&](const std::string& req) {
        if (req.find("/capabilities") != std::string::npos) return pbtest::TlsTestServer::http_response(200, kCapabilities);
        return pbtest::TlsTestServer::http_response(200, list);
    });
    pbtest::TmpDir data;
    data.write("custom-ca.pem", slurp_file(pki.ca_cert()));
    NotesBridge bridge(config_for(data));
    PB_CHECK(bridge.saveAccount(QString::fromStdString(server.url("/")), "alice", "pw"));

    QString outcome;
    QEventLoop loop;
    QObject::connect(&bridge, &NotesBridge::syncFinished, &loop, [&](const QString& o, const QString&) {
        outcome = o;
        loop.quit();
    });
    QTimer::singleShot(20000, &loop, &QEventLoop::quit);
    int changed = 0;
    QObject::connect(&bridge, &NotesBridge::notesChanged, [&] { ++changed; });
    bridge.syncNow();
    PB_CHECK(bridge.property("syncRunning").toBool());
    bridge.syncNow();  // a second request while running is ignored
    if (outcome.isEmpty()) loop.exec();
    PB_CHECK_EQ(outcome.toStdString(), std::string("success"));
    PB_CHECK(!bridge.property("syncRunning").toBool());
    PB_CHECK(changed >= 1);
    const QVariantList rows = bridge.listNotes();
    PB_CHECK_EQ(rows.size(), 1);
    PB_CHECK_EQ(rows.at(0).toMap().value("title").toString().toStdString(), std::string("Pulled note"));
    PB_CHECK_EQ(bridge.property("syncOutcome").toString().toStdString(), std::string("success"));
    PB_CHECK(bridge.property("syncStatus").toString().contains("Last sync"));
}

PB_TEST(a_failed_sync_is_reported_and_keeps_the_local_notes) {
    pbtest::TmpDir data;
    seed(data, {remote_note(1, "Local copy\nstays", "", 100)});
    NotesBridge bridge(config_for(data));
    PB_CHECK(bridge.saveAccount("https://localhost:1", "alice", "pw"));  // nothing listens there
    QString outcome;
    QEventLoop loop;
    QObject::connect(&bridge, &NotesBridge::syncFinished, &loop, [&](const QString& o, const QString&) {
        outcome = o;
        loop.quit();
    });
    QTimer::singleShot(20000, &loop, &QEventLoop::quit);
    bridge.syncNow();
    if (outcome.isEmpty()) loop.exec();
    PB_CHECK_EQ(outcome.toStdString(), std::string("offline"));
    PB_CHECK_EQ(bridge.listNotes().size(), 1);
    PB_CHECK(!bridge.property("syncStatus").toString().isEmpty());
}

// ---- User Story 3: create, edit, delete, category, favorite ----------------------------------------

namespace {
struct ErrorCatcher {
    QStringList keys;
    explicit ErrorCatcher(NotesBridge& b) {
        QObject::connect(&b, &NotesBridge::errorOccurred, [this](const QString& key, const QString&) { keys << key; });
    }
};
}  // namespace

PB_TEST(a_new_note_is_saved_pending_and_survives_a_restart) {
    pbtest::TmpDir data;
    qlonglong id = 0;
    {
        NotesBridge bridge(config_for(data));
        PB_CHECK_EQ(bridge.property("pendingCount").toInt(), 0);
        id = bridge.saveNote(-1, "Fresh idea", "Fresh idea\nwith a body", "Ideas", true);
        PB_CHECK(id > 0);
        PB_CHECK_EQ(bridge.property("pendingCount").toInt(), 1);
    }
    NotesBridge again(config_for(data));
    const QVariantList rows = again.listNotes();
    PB_CHECK_EQ(rows.size(), 1);
    const QVariantMap row = rows.at(0).toMap();
    PB_CHECK_EQ(row.value("title").toString().toStdString(), std::string("Fresh idea"));
    PB_CHECK(row.value("pending").toBool() && row.value("favorite").toBool());
    PB_CHECK_EQ(row.value("category").toString().toStdString(), std::string("Ideas"));
    const QVariantMap edit = again.loadForEdit(id);
    PB_CHECK_EQ(edit.value("text").toString().toStdString(), std::string("Fresh idea\nwith a body"));
    PB_CHECK_EQ(edit.value("category").toString().toStdString(), std::string("Ideas"));
    PB_CHECK(edit.value("favorite").toBool());
}

PB_TEST(an_empty_new_note_is_rejected_and_nothing_is_stored) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    ErrorCatcher errors(bridge);
    PB_CHECK_EQ(bridge.saveNote(-1, "Whatever", "  \n \t", "", false), static_cast<qlonglong>(-1));
    PB_CHECK_EQ(errors.keys.size(), 1);
    PB_CHECK_EQ(errors.keys.at(0).toStdString(), std::string("content_empty"));
    PB_CHECK(bridge.listNotes().isEmpty());
}

PB_TEST(editing_a_synced_note_marks_it_pending_and_updates_the_title) {
    pbtest::TmpDir data;
    seed(data, {remote_note(1, "Old first line\nbody", "Home", 100)});
    NotesBridge bridge(config_for(data));
    const qlonglong id = bridge.listNotes().at(0).toMap().value("localId").toLongLong();
    int changed = 0;
    QObject::connect(&bridge, &NotesBridge::notesChanged, [&] { ++changed; });
    PB_CHECK_EQ(bridge.saveNote(id, "New first line", "New first line\nbody", "Home", false), id);
    PB_CHECK(changed >= 1);
    const QVariantMap row = bridge.listNotes().at(0).toMap();
    PB_CHECK_EQ(row.value("title").toString().toStdString(), std::string("New first line"));
    PB_CHECK(row.value("pending").toBool());
    PB_CHECK_EQ(bridge.property("pendingCount").toInt(), 1);
}

PB_TEST(an_invalid_category_is_refused_and_the_old_one_stays) {
    pbtest::TmpDir data;
    seed(data, {remote_note(1, "Note\nbody", "Home", 100)});
    NotesBridge bridge(config_for(data));
    ErrorCatcher errors(bridge);
    const qlonglong id = bridge.listNotes().at(0).toMap().value("localId").toLongLong();
    PB_CHECK(!bridge.setCategory(id, " leading space"));
    PB_CHECK_EQ(errors.keys.at(0).toStdString(), std::string("category_spaces"));
    PB_CHECK_EQ(bridge.listNotes().at(0).toMap().value("category").toString().toStdString(), std::string("Home"));
    PB_CHECK(bridge.setCategory(id, "Work/Projects"));
    PB_CHECK_EQ(bridge.listNotes().at(0).toMap().value("category").toString().toStdString(), std::string("Work/Projects"));
    PB_CHECK_EQ(bridge.saveNote(id, "Note", "Note\nbody", "x\x01y", false), static_cast<qlonglong>(-1));  // control character
    PB_CHECK(!bridge.message("category_too_long").startsWith("category_"));  // has a real text
}

// ---- spec 004: the explicit title -------------------------------------------------------------------------------

PB_TEST(the_editor_gets_the_stored_title_as_it_is) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    const qlonglong id = bridge.saveNote(-1, "## Plan", "Something else entirely\nbody", "", false);
    PB_CHECK(id > 0);
    const QVariantMap edit = bridge.loadForEdit(id);
    PB_CHECK_EQ(edit.value("title").toString().toStdString(), std::string("## Plan"));   // not display-processed
    PB_CHECK_EQ(edit.value("text").toString().toStdString(), std::string("Something else entirely\nbody"));
}

PB_TEST(list_and_reader_show_the_title_exactly_as_entered) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    const qlonglong id = bridge.saveNote(-1, "  # Not a heading  ", "text", "", false);
    PB_CHECK(id > 0);
    PB_CHECK_EQ(bridge.listNotes().at(0).toMap().value("title").toString().toStdString(), std::string("# Not a heading"));
    PB_CHECK_EQ(bridge.openNote(id).value("title").toString().toStdString(), std::string("# Not a heading"));
}

PB_TEST(editing_the_text_does_not_change_the_title_and_editing_the_title_does_not_change_the_text) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    const qlonglong id = bridge.saveNote(-1, "Groceries", "First line\nmilk", "", false);
    PB_CHECK_EQ(bridge.saveNote(id, "Groceries", "Another first line\nmilk", "", false), id);
    PB_CHECK_EQ(bridge.listNotes().at(0).toMap().value("title").toString().toStdString(), std::string("Groceries"));
    PB_CHECK_EQ(bridge.saveNote(id, "Shopping", "Another first line\nmilk", "", false), id);
    PB_CHECK_EQ(bridge.listNotes().at(0).toMap().value("title").toString().toStdString(), std::string("Shopping"));
    PB_CHECK_EQ(bridge.loadForEdit(id).value("text").toString().toStdString(), std::string("Another first line\nmilk"));
}

PB_TEST(a_refused_title_reports_a_real_text_and_stores_nothing) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    ErrorCatcher errors(bridge);
    PB_CHECK_EQ(bridge.saveNote(-1, "", "Some text", "", false), static_cast<qlonglong>(-1));
    PB_CHECK_EQ(bridge.saveNote(-1, "   ", "Some text", "", false), static_cast<qlonglong>(-1));
    PB_CHECK_EQ(bridge.saveNote(-1, "two\nlines", "Some text", "", false), static_cast<qlonglong>(-1));
    PB_CHECK_EQ(bridge.saveNote(-1, QString(256, QLatin1Char('a')), "Some text", "", false), static_cast<qlonglong>(-1));
    PB_CHECK_EQ(errors.keys.size(), 4);
    PB_CHECK_EQ(errors.keys.at(0).toStdString(), std::string("title_empty"));
    PB_CHECK_EQ(errors.keys.at(1).toStdString(), std::string("title_empty"));
    PB_CHECK_EQ(errors.keys.at(2).toStdString(), std::string("title_control_chars"));
    PB_CHECK_EQ(errors.keys.at(3).toStdString(), std::string("title_too_long"));
    PB_CHECK(bridge.listNotes().isEmpty());
    for (const char* key : {"title_empty", "title_too_long", "title_control_chars", "title_invalid"}) {
        const std::string text = bridge.message(key).toStdString();
        PB_CHECK(!text.empty() && text.rfind("title_", 0) != 0);   // a sentence, not the key
    }
    PB_CHECK_EQ(bridge.message("title_empty").toStdString(), std::string("The note needs a title."));
    PB_CHECK_EQ(bridge.message("title_too_long").toStdString(), std::string("The title is too long (at most 255 characters)."));
}

PB_TEST(a_note_that_was_saved_keeps_its_text_when_the_title_is_refused) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    const qlonglong id = bridge.saveNote(-1, "Keep", "Original text", "", false);
    PB_CHECK_EQ(bridge.saveNote(id, "", "Changed text", "", false), static_cast<qlonglong>(-1));
    PB_CHECK_EQ(bridge.loadForEdit(id).value("text").toString().toStdString(), std::string("Original text"));
    PB_CHECK_EQ(bridge.loadForEdit(id).value("title").toString().toStdString(), std::string("Keep"));
}

PB_TEST(a_note_with_no_title_at_all_is_shown_as_untitled) {
    pbtest::TmpDir data;
    seed(data, {remote_note(1, "", "", 100)});   // no text and no server title
    NotesBridge bridge(config_for(data));
    PB_CHECK_EQ(bridge.listNotes().at(0).toMap().value("title").toString().toStdString(), std::string("(untitled)"));
}

// ---- spec 004: the category picker ------------------------------------------------------------------------------------

namespace {
QStringList choice_values(const QVariantList& choices) {
    QStringList out;
    for (const QVariant& c : choices) out << c.toMap().value("value").toString();
    return out;
}
QStringList choice_labels(const QVariantList& choices) {
    QStringList out;
    for (const QVariant& c : choices) out << c.toMap().value("label").toString();
    return out;
}
}  // namespace

PB_TEST(the_picker_lists_no_category_new_category_and_every_category_in_use) {
    pbtest::TmpDir data;
    seed(data, {remote_note(1, "A\nbody", "Work", 100), remote_note(2, "B\nbody", "Home", 90), remote_note(3, "C\nbody", "Work", 80)});
    NotesBridge bridge(config_for(data));
    const QVariantList choices = bridge.categoryChoices(QString());
    PB_CHECK_EQ(choice_values(choices).join("|").toStdString(), std::string("none|new|c:Home|c:Work"));
    PB_CHECK_EQ(choice_labels(choices).at(0).toStdString(), std::string("No category"));
    PB_CHECK_EQ(choice_labels(choices).at(2).toStdString(), std::string("Home"));
    PB_CHECK(choice_labels(choices).at(1).startsWith("New category"));
}

PB_TEST(with_no_category_in_use_the_picker_offers_only_the_two_actions) {
    pbtest::TmpDir data;
    seed(data, {remote_note(1, "A\nbody", "", 100)});
    NotesBridge bridge(config_for(data));
    PB_CHECK_EQ(choice_values(bridge.categoryChoices(QString())).join("|").toStdString(), std::string("none|new"));
}

PB_TEST(categories_of_notes_not_yet_synced_are_offered_and_those_of_deleted_notes_are_not) {
    pbtest::TmpDir data;
    seed(data, {remote_note(1, "A\nbody", "Gone", 100)});
    NotesBridge bridge(config_for(data));
    const qlonglong synced = bridge.listNotes().at(0).toMap().value("localId").toLongLong();
    PB_CHECK(bridge.saveNote(-1, "Fresh", "Fresh\ntext", "Offline only", false) > 0);
    PB_CHECK(bridge.deleteNote(synced));
    PB_CHECK_EQ(choice_values(bridge.categoryChoices(QString())).join("|").toStdString(), std::string("none|new|c:Offline only"));
}

PB_TEST(a_current_choice_nobody_carries_yet_is_listed_once_in_its_sorted_place) {
    pbtest::TmpDir data;
    seed(data, {remote_note(1, "A\nbody", "Home", 100), remote_note(2, "B\nbody", "Work", 90)});
    NotesBridge bridge(config_for(data));
    PB_CHECK_EQ(choice_values(bridge.categoryChoices("Travel")).join("|").toStdString(), std::string("none|new|c:Home|c:Travel|c:Work"));
    PB_CHECK_EQ(choice_values(bridge.categoryChoices("Home")).join("|").toStdString(), std::string("none|new|c:Home|c:Work"));   // not twice
}

PB_TEST(a_category_called_none_or_new_cannot_be_mistaken_for_an_action) {
    pbtest::TmpDir data;
    seed(data, {remote_note(1, "A\nbody", "none", 100), remote_note(2, "B\nbody", "new", 90)});
    NotesBridge bridge(config_for(data));
    PB_CHECK_EQ(choice_values(bridge.categoryChoices(QString())).join("|").toStdString(), std::string("none|new|c:new|c:none"));
}

PB_TEST(with_many_categories_the_two_actions_stay_first) {
    pbtest::TmpDir data;
    std::vector<pbnotes::ApiNote> notes;
    for (int i = 0; i < 60; ++i) notes.push_back(remote_note(i + 1, "N" + std::to_string(i) + "\nbody", "Cat " + std::to_string(100 + i), 100 + i));
    seed(data, notes);
    NotesBridge bridge(config_for(data));
    const QStringList values = choice_values(bridge.categoryChoices(QString()));
    PB_CHECK_EQ(values.size(), 62);
    PB_CHECK_EQ(values.at(0).toStdString(), std::string("none"));
    PB_CHECK_EQ(values.at(1).toStdString(), std::string("new"));
}

PB_TEST(a_category_chosen_but_not_saved_is_not_offered_to_other_notes) {
    pbtest::TmpDir data;
    seed(data, {remote_note(1, "A\nbody", "Home", 100)});
    NotesBridge bridge(config_for(data));
    // the editor of note 1 has "Draft" as its current choice; it is listed there only
    PB_CHECK(choice_values(bridge.categoryChoices("Draft")).contains("c:Draft"));
    PB_CHECK(!choice_values(bridge.categoryChoices(QString())).contains("c:Draft"));
    // once a note carrying it is saved it is offered everywhere
    PB_CHECK(bridge.saveNote(-1, "Trip", "Trip\ntext", "Draft", false) > 0);
    PB_CHECK(choice_values(bridge.categoryChoices(QString())).contains("c:Draft"));
}

// ---- spec 004: defining a new category from the picker ---------------------------------------------------------------

PB_TEST(a_typed_category_name_is_trimmed_and_accepted) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    const QVariantMap r = bridge.checkCategory("  Travel  ");
    PB_CHECK(r.value("ok").toBool());
    PB_CHECK_EQ(r.value("name").toString().toStdString(), std::string("Travel"));
    PB_CHECK(r.value("errorText").toString().isEmpty());
}

PB_TEST(an_empty_category_name_is_refused_with_a_sentence) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    for (const char* name : {"", "   ", "\t"}) {
        const QVariantMap r = bridge.checkCategory(name);
        PB_CHECK(!r.value("ok").toBool());
        PB_CHECK_EQ(r.value("errorText").toString().toStdString(), std::string("Enter a category name."));
    }
}

PB_TEST(the_length_limit_is_255_characters) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    PB_CHECK(bridge.checkCategory(QString(255, QLatin1Char('a'))).value("ok").toBool());
    const QVariantMap r = bridge.checkCategory(QString(256, QLatin1Char('a')));
    PB_CHECK(!r.value("ok").toBool());
    PB_CHECK_EQ(r.value("errorText").toString().toStdString(), std::string("The category is too long (at most 255 characters)."));
    PB_CHECK(bridge.checkCategory(QString(255, QChar(0x00E4))).value("ok").toBool());   // characters, not bytes
}

PB_TEST(control_characters_in_a_category_name_are_refused) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    const QVariantMap r = bridge.checkCategory(QString("a") + QChar(0x01) + QString("b"));
    PB_CHECK(!r.value("ok").toBool());
    PB_CHECK_EQ(r.value("errorText").toString().toStdString(), std::string("The category contains characters that are not allowed."));
    PB_CHECK(!bridge.checkCategory(QString("a\nb")).value("ok").toBool());
}

PB_TEST(a_slash_stays_one_flat_name_and_case_is_significant) {
    pbtest::TmpDir data;
    seed(data, {remote_note(1, "A\nbody", "Work", 100)});
    NotesBridge bridge(config_for(data));
    const QVariantMap slash = bridge.checkCategory("Work/Projects");
    PB_CHECK(slash.value("ok").toBool());
    PB_CHECK_EQ(slash.value("name").toString().toStdString(), std::string("Work/Projects"));
    const QVariantMap lower = bridge.checkCategory("work");
    PB_CHECK(lower.value("ok").toBool());
    PB_CHECK_EQ(lower.value("name").toString().toStdString(), std::string("work"));   // not merged with "Work"
}

PB_TEST(a_name_equal_to_an_existing_category_comes_back_as_that_exact_name) {
    pbtest::TmpDir data;
    seed(data, {remote_note(1, "A\nbody", "Work", 100)});
    NotesBridge bridge(config_for(data));
    const QVariantMap r = bridge.checkCategory(" Work ");
    PB_CHECK(r.value("ok").toBool());
    PB_CHECK_EQ(r.value("name").toString().toStdString(), std::string("Work"));
    // assigning it creates no second spelling
    PB_CHECK(bridge.saveNote(-1, "Another", "text", r.value("name").toString(), false) > 0);
    PB_CHECK_EQ(bridge.categories().size(), 1);
}

PB_TEST(checking_a_name_stores_nothing) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    bridge.checkCategory("Travel");
    PB_CHECK(bridge.categories().isEmpty());
    PB_CHECK(!choice_values(bridge.categoryChoices(QString())).contains("c:Travel"));
}

PB_TEST(favorite_can_be_toggled_and_toggles_back) {
    pbtest::TmpDir data;
    seed(data, {remote_note(1, "Note\nbody", "", 100)});
    NotesBridge bridge(config_for(data));
    const qlonglong id = bridge.listNotes().at(0).toMap().value("localId").toLongLong();
    PB_CHECK(bridge.toggleFavorite(id));
    PB_CHECK(bridge.listNotes().at(0).toMap().value("favorite").toBool());
    PB_CHECK(bridge.toggleFavorite(id));
    PB_CHECK(!bridge.listNotes().at(0).toMap().value("favorite").toBool());
}

PB_TEST(deleting_removes_an_unsynced_note_and_hides_a_synced_one_until_the_server_confirms) {
    pbtest::TmpDir data;
    seed(data, {remote_note(1, "Synced note\nbody", "", 100)});
    NotesBridge bridge(config_for(data));
    const qlonglong synced = bridge.listNotes().at(0).toMap().value("localId").toLongLong();
    const qlonglong fresh = bridge.saveNote(-1, "Only on the reader", "Only on the reader\nbody", "", false);
    PB_CHECK_EQ(bridge.listNotes().size(), 2);
    PB_CHECK(bridge.deleteNote(fresh));
    PB_CHECK_EQ(bridge.listNotes().size(), 1);
    PB_CHECK_EQ(bridge.property("pendingCount").toInt(), 0);       // never reached the server: gone
    PB_CHECK(bridge.deleteNote(synced));
    PB_CHECK(bridge.listNotes().isEmpty());                         // hidden ...
    PB_CHECK_EQ(bridge.property("pendingCount").toInt(), 1);       // ... but still to be deleted on the server
    PB_CHECK(bridge.openNote(synced).isEmpty());
    PB_CHECK(!bridge.deleteNote(99999));
}

// ---- User Story 5: conflicts are reported to the UI ---------------------------------------------------

PB_TEST(a_sync_with_a_conflict_signals_it_and_keeps_both_versions) {
    pbtest::TmpDir d;
    pbtest::TestPki pki(d, "privateca");
    if (!pki.available()) PB_SKIP("openssl CLI not available");
    auto leaf = pki.issue("server", "DNS:localhost,IP:127.0.0.1");
    const std::string server_note =
        R"({"id":7,"etag":"e7new","readonly":false,"title":"Plan","content":"Plan\nserver text","category":"","favorite":false,"modified":1700000500})";
    const std::string copy_note =
        R"j({"id":8,"etag":"e8","readonly":false,"title":"Plan (conflict 2026-10-10)","content":"Plan (conflict 2026-10-10)\nlocal text","category":"","favorite":false,"modified":1700000600})j";
    std::atomic<bool> copy_posted{false};
    pbtest::TlsTestServer server(leaf.cert, leaf.key, [&](const std::string& req) {
        const std::string line = req.substr(0, req.find('\r'));
        if (req.find("/capabilities") != std::string::npos) return pbtest::TlsTestServer::http_response(200, kCapabilities);
        if (line.rfind("PUT ", 0) == 0) return pbtest::TlsTestServer::http_response(412, "{}");
        if (line.rfind("POST ", 0) == 0) {
            copy_posted = true;
            return pbtest::TlsTestServer::http_response(200, copy_note);
        }
        if (line.find("/notes/7") != std::string::npos) return pbtest::TlsTestServer::http_response(200, server_note);
        return pbtest::TlsTestServer::http_response(
            200, "[" + server_note + (copy_posted ? "," + copy_note : std::string()) + "]");
    });
    pbtest::TmpDir data;
    data.write("custom-ca.pem", slurp_file(pki.ca_cert()));
    seed(data, {remote_note(7, "Plan\nold text", "", 1700000000)});
    {
        auto store = pbnotes::NoteStore::open(data.file("pbnotes.db"));
        const auto note = store->get_by_remote_id(7);
        PB_CHECK(note.has_value());
        PB_CHECK(store->save_local(note->local_id, pbnotes::derive_title("Plan\nlocal text"), "Plan\nlocal text", "", false, 1700000100).error == pbnotes::EditError::None);
    }
    NotesBridge bridge(config_for(data));
    PB_CHECK(bridge.saveAccount(QString::fromStdString(server.url("/")), "alice", "pw"));

    int conflicts = -1, restored = -1;
    QEventLoop loop;
    QObject::connect(&bridge, &NotesBridge::conflictsDetected, &loop, [&](int c, int r) { conflicts = c; restored = r; });
    QObject::connect(&bridge, &NotesBridge::syncFinished, &loop, [&](const QString&, const QString&) { loop.quit(); });
    QTimer::singleShot(20000, &loop, &QEventLoop::quit);
    bridge.syncNow();
    loop.exec();
    PB_CHECK_EQ(conflicts, 1);
    PB_CHECK_EQ(restored, 0);
    PB_CHECK_EQ(bridge.property("conflictCount").toInt(), 1);
    PB_CHECK(bridge.property("syncStatus").toString().contains("1 conflict"));

    bool has_server = false, has_copy = false;
    for (const QVariant& v : bridge.listNotes()) {
        const std::string title = v.toMap().value("title").toString().toStdString();
        if (title == "Plan") has_server = true;
        if (title.find("(conflict ") != std::string::npos) has_copy = true;
    }
    PB_CHECK(has_server);
    PB_CHECK(has_copy);
}

// ---- User Story 6: views, sort, search, task boxes ---------------------------------------------------

namespace {
QStringList titles_of(const QVariantList& rows) {
    QStringList out;
    for (const QVariant& v : rows) out << v.toMap().value("title").toString();
    return out;
}
}  // namespace

PB_TEST(views_sort_and_search_shape_the_list_and_are_remembered) {
    pbtest::TmpDir data;
    seed(data, {remote_note(1, "Banana\nyellow fruit", "Food", 100), remote_note(2, "apple\nred fruit", "Food", 200, true),
                remote_note(3, "Zebra\nstripes", "", 300)});
    {
        NotesBridge bridge(config_for(data));
        PB_CHECK_EQ(bridge.property("view").toString().toStdString(), std::string("all"));
        PB_CHECK(titles_of(bridge.listNotes()).join(",") == QString("Zebra,apple,Banana"));

        bridge.setSortOrder("title");
        PB_CHECK(titles_of(bridge.listNotes()).join(",") == QString("apple,Banana,Zebra"));
        bridge.setSortOrder("bogus");  // ignored
        PB_CHECK_EQ(bridge.property("sortOrder").toString().toStdString(), std::string("title"));

        bridge.setView("favorites");
        PB_CHECK(titles_of(bridge.listNotes()).join(",") == QString("apple"));
        bridge.setView("uncategorized");
        PB_CHECK(titles_of(bridge.listNotes()).join(",") == QString("Zebra"));
        bridge.setView("category:Food");
        PB_CHECK(titles_of(bridge.listNotes()).join(",") == QString("apple,Banana"));

        const QVariantList cats = bridge.property("categories").toList();
        PB_CHECK_EQ(cats.size(), 1);
        PB_CHECK_EQ(cats.at(0).toMap().value("name").toString().toStdString(), std::string("Food"));
        PB_CHECK_EQ(cats.at(0).toMap().value("count").toInt(), 2);

        bridge.setSearch("RED");  // case-insensitive, in the body; combined with the view
        PB_CHECK(titles_of(bridge.listNotes()).join(",") == QString("apple"));
        PB_CHECK_EQ(bridge.property("searchText").toString().toStdString(), std::string("RED"));
        bridge.setSearch("");
        PB_CHECK_EQ(int(titles_of(bridge.listNotes()).size()), 2);
    }
    NotesBridge again(config_for(data));  // view and sort order persist, the search text does not
    PB_CHECK_EQ(again.property("view").toString().toStdString(), std::string("category:Food"));
    PB_CHECK_EQ(again.property("sortOrder").toString().toStdString(), std::string("title"));
    PB_CHECK(again.property("searchText").toString().isEmpty());
}

PB_TEST(a_view_of_a_category_that_no_longer_exists_falls_back_to_all) {
    pbtest::TmpDir data;
    seed(data, {remote_note(1, "One\nbody", "Gone", 100)});
    NotesBridge bridge(config_for(data));
    bridge.setView("category:Gone");
    const qlonglong id = bridge.listNotes().at(0).toMap().value("localId").toLongLong();
    int changed = 0;
    QObject::connect(&bridge, &NotesBridge::notesChanged, [&] { ++changed; });  // what the View button listens to
    PB_CHECK(bridge.setCategory(id, ""));
    PB_CHECK(changed >= 1);
    // the property the View button binds to must announce the change through notesChanged
    PB_CHECK_EQ(std::string(bridge.metaObject()->property(bridge.metaObject()->indexOfProperty("view")).notifySignal().name().constData()),
                std::string("notesChanged"));
    PB_CHECK_EQ(bridge.property("view").toString().toStdString(), std::string("all"));
    PB_CHECK_EQ(bridge.listNotes().size(), 1);
}

PB_TEST(toggling_a_task_box_saves_the_note_as_a_pending_edit) {
    pbtest::TmpDir data;
    seed(data, {remote_note(1, "Shopping\n- [ ] milk\n- [x] bread\n", "", 100)});
    NotesBridge bridge(config_for(data));
    const qlonglong id = bridge.listNotes().at(0).toMap().value("localId").toLongLong();
    int changed = 0;
    QObject::connect(&bridge, &NotesBridge::noteChanged, [&](qlonglong) { ++changed; });
    PB_CHECK(bridge.toggleTask(id, 1));
    PB_CHECK(changed >= 1);
    PB_CHECK_EQ(bridge.property("pendingCount").toInt(), 1);
    PB_CHECK_EQ(bridge.loadForEdit(id).value("text").toString().toStdString(), std::string("Shopping\n- [x] milk\n- [x] bread\n"));
    PB_CHECK(bridge.toggleTask(id, 2));
    PB_CHECK_EQ(bridge.loadForEdit(id).value("text").toString().toStdString(), std::string("Shopping\n- [x] milk\n- [ ] bread\n"));
    PB_CHECK(!bridge.toggleTask(id, 0));      // not a task line: nothing changes
    PB_CHECK(!bridge.toggleTask(99999, 1));   // unknown note
}

// ---- US3: emojis ------------------------------------------------------------------------------------

PB_TEST(emojis_survive_the_reader_and_the_list) {
    pbtest::TmpDir data;
    const std::string grin = "\U0001F600", family = "\U0001F468\u200D\U0001F469\u200D\U0001F467", flag = "\U0001F1E9\U0001F1EA";
    seed(data, {remote_note(1, grin + " Title <x>\nLine " + family + " & " + flag + "\n", "", 100),
                remote_note(2, grin, "", 90)});
    NotesBridge bridge(config_for(data));
    const QVariantList rows = bridge.listNotes();
    PB_CHECK_EQ(rows.size(), 2);
    PB_CHECK_EQ(rows.at(0).toMap().value("title").toString().toStdString(), grin + " Title <x>");  // a leading emoji stays
    PB_CHECK_EQ(rows.at(1).toMap().value("title").toString().toStdString(), grin);                  // an emoji-only note is listed
    PB_CHECK(rows.at(0).toMap().value("excerpt").toString().toStdString().find(family) != std::string::npos);
    const qlonglong id = rows.at(0).toMap().value("localId").toLongLong();
    const QVariantList blocks = bridge.openNote(id).value("blocks").toList();
    PB_CHECK_EQ(blocks.at(0).toMap().value("html").toString().toStdString(), grin + " Title &lt;x&gt;");
    PB_CHECK_EQ(blocks.at(1).toMap().value("html").toString().toStdString(), "Line " + family + " &amp; " + flag);
}

// ---- spec 005: the title on the reading page --------------------------------------------------------------------------

PB_TEST(opening_a_note_reports_whether_it_has_a_title_to_show) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    const qlonglong id = bridge.saveNote(-1, "Groceries", "Not the title\nmilk", "", false);
    PB_CHECK(id > 0);
    const QVariantMap note = bridge.openNote(id);
    PB_CHECK(note.value("hasTitle").toBool());
    PB_CHECK_EQ(note.value("titleText").toString().toStdString(), std::string("Groceries"));
    PB_CHECK_EQ(note.value("title").toString().toStdString(), std::string("Groceries"));
}

PB_TEST(a_title_of_255_characters_is_returned_whole) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    const QString title(255, QLatin1Char('t'));
    const qlonglong id = bridge.saveNote(-1, title, "text", "", false);
    PB_CHECK(id > 0);
    PB_CHECK_EQ(bridge.openNote(id).value("titleText").toString().size(), 255);
}

PB_TEST(a_note_without_any_title_has_none_to_show_but_the_list_still_says_untitled) {
    pbtest::TmpDir data;
    seed(data, {remote_note(1, "", "", 100), remote_note(2, "  \n\n", "", 90)});
    NotesBridge bridge(config_for(data));
    const QVariantList rows = bridge.listNotes();
    PB_CHECK_EQ(rows.size(), 2);
    for (const QVariant& r : rows) {
        const QVariantMap note = bridge.openNote(r.toMap().value("localId").toLongLong());
        PB_CHECK(!note.value("hasTitle").toBool());                         // the reader shows no title block
        PB_CHECK_EQ(note.value("titleText").toString().trimmed().size(), 0);
        PB_CHECK_EQ(note.value("title").toString().toStdString(), std::string("(untitled)"));
        PB_CHECK_EQ(r.toMap().value("title").toString().toStdString(), std::string("(untitled)"));
    }
}

PB_TEST(the_reader_gets_the_title_as_stored_not_stripped) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    const qlonglong id = bridge.saveNote(-1, "# Plan", "text", "", false);
    PB_CHECK_EQ(bridge.openNote(id).value("titleText").toString().toStdString(), std::string("# Plan"));
}

PB_TEST(the_text_is_not_de_duplicated_against_the_title) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    const qlonglong id = bridge.saveNote(-1, "Groceries", "# Groceries\nmilk", "", false);
    const QVariantMap note = bridge.openNote(id);
    const QVariantList blocks = note.value("blocks").toList();
    PB_CHECK_EQ(blocks.size(), 2);
    PB_CHECK_EQ(blocks.at(0).toMap().value("kind").toString().toStdString(), std::string("heading"));
    PB_CHECK_EQ(blocks.at(0).toMap().value("html").toString().toStdString(), std::string("Groceries"));
    PB_CHECK(note.value("hasTitle").toBool());
}

// ---- spec 005: copying from the reading page ------------------------------------------------------------------------

namespace {
QString copied(NotesBridge& bridge, qlonglong id, int fu, int fo, int tu, int to) {
    return bridge.readerCopy(id, fu, fo, tu, to);
}
}  // namespace

PB_TEST(the_reader_copy_returns_the_title_and_source_text) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    const qlonglong id = bridge.saveNote(-1, "Plan", "# Head\nsome **bold** words", "", false);
    // units: 0 = title, 1 = "Head", 2 = the paragraph
    PB_CHECK_EQ(copied(bridge, id, 0, 0, 0, 4).toStdString(), std::string("Plan"));
    PB_CHECK_EQ(copied(bridge, id, 0, 0, 1, 4).toStdString(), std::string("Plan\n# Head"));
    PB_CHECK_EQ(copied(bridge, id, 1, 1, 1, 3).toStdString(), std::string("ea"));              // inside the heading: characters only
    PB_CHECK_EQ(copied(bridge, id, 2, 5, 2, 9).toStdString(), std::string("**bold**"));         // a completely selected element
    PB_CHECK_EQ(copied(bridge, id, 2, 6, 2, 8).toStdString(), std::string("ol"));               // partly: no marks
    PB_CHECK_EQ(copied(bridge, id, 0, 2, 2, 4).toStdString(), std::string("an\n# Head\nsome")); // from the title on
}

PB_TEST(the_reader_copy_counts_utf16_units) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    const qlonglong id = bridge.saveNote(-1, "T", "\U0001F600 smile and more", "", false);
    // unit 1 is the paragraph: the emoji is two UTF-16 units, so " smile" is units 2..8
    PB_CHECK_EQ(copied(bridge, id, 1, 2, 1, 8).toStdString(), std::string(" smile"));
    PB_CHECK_EQ(copied(bridge, id, 1, 0, 1, 2).toStdString(), std::string("\xF0\x9F\x98\x80"));
}

PB_TEST(a_note_without_a_title_or_text_copies_nothing) {
    pbtest::TmpDir data;
    seed(data, {remote_note(1, "", "", 100)});
    NotesBridge bridge(config_for(data));
    const qlonglong id = bridge.listNotes().at(0).toMap().value("localId").toLongLong();
    PB_CHECK_EQ(copied(bridge, id, 0, 0, 0, 0).toStdString(), std::string());
    PB_CHECK_EQ(copied(bridge, id, 0, 0, 3, 3).toStdString(), std::string());
}

PB_TEST(table_rows_and_attachments_are_whole_units) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    const qlonglong id = bridge.saveNote(-1, "T", "| a | b |\n|---|---|\n| 1 | 2 |\n![n](.attachments.4/pic.png)", "", false);
    // units: 0 title, 1 header row, 2 data row, 3 attachment
    PB_CHECK_EQ(copied(bridge, id, 2, 0, 2, 0).toStdString(), std::string("| 1 | 2 |"));
    PB_CHECK_EQ(copied(bridge, id, 3, 0, 3, 0).toStdString(), std::string("![n](.attachments.4/pic.png)"));
    PB_CHECK_EQ(copied(bridge, id, 1, 0, 2, 0).toStdString(), std::string("| a | b |\n|---|---|\n| 1 | 2 |"));
}

PB_TEST(a_reversed_or_out_of_range_selection_copies_nothing) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    const qlonglong id = bridge.saveNote(-1, "T", "alpha\nbeta", "", false);
    // normalized: from and to may come in either order
    PB_CHECK_EQ(copied(bridge, id, 2, 2, 1, 1).toStdString(), copied(bridge, id, 1, 1, 2, 2).toStdString());
    PB_CHECK_EQ(copied(bridge, id, 0, 0, 99, 0).toStdString(), std::string());
    PB_CHECK_EQ(copied(bridge, 12345, 0, 0, 1, 1).toStdString(), std::string());   // unknown note
}

PB_TEST(copying_changes_nothing) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    const qlonglong id = bridge.saveNote(-1, "Plan", "# Head\nsome **bold** words", "Work", true);
    const auto before = bridge.openNote(id);
    const auto list = bridge.listNotes();
    const int pending = bridge.property("pendingCount").toInt();
    (void)copied(bridge, id, 0, 0, 2, 9);
    (void)copied(bridge, id, 1, 0, 1, 0);
    const auto after = bridge.openNote(id);
    PB_CHECK(before == after);                                   // title, blocks, modified, pending, category, favorite
    PB_CHECK(list == bridge.listNotes());
    PB_CHECK_EQ(bridge.property("pendingCount").toInt(), pending);
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    return pbtest::run_all();
}
