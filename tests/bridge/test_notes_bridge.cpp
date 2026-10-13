// T032/T035/T040 (host): the Qt bridge for the setup flow, run against real local HTTPS servers.
// Covers the connection test messages, the CA handling (device/bundled + custom-ca.pem), saving and
// forgetting the account, and that the password is never exposed back. Needs the `openssl` CLI.
#include <atomic>
#include <chrono>
#include <ctime>
#include <functional>
#include <memory>
#include <thread>

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

// Spec 008: "Auto Sync" is a persisted on/off choice, on by default, usable without an account.
PB_TEST(the_auto_sync_choice_is_on_by_default_persisted_and_announced_once) {
    pbtest::TmpDir data;
    {
        NotesBridge bridge(config_for(data));
        int announced = 0;
        QObject::connect(&bridge, &NotesBridge::syncAfterSaveChanged, [&] { ++announced; });
        PB_CHECK(bridge.property("syncAfterSave").toBool());   // on on a fresh install, no account
        PB_CHECK(bridge.setProperty("syncAfterSave", false));
        PB_CHECK(!bridge.property("syncAfterSave").toBool());
        PB_CHECK_EQ(announced, 1);
        bridge.setProperty("syncAfterSave", false);   // same value again: nothing is announced
        PB_CHECK_EQ(announced, 1);
        bridge.setProperty("syncAfterSave", true);
        PB_CHECK(bridge.property("syncAfterSave").toBool());
        PB_CHECK_EQ(announced, 2);
        bridge.setProperty("syncAfterSave", false);
    }
    NotesBridge again(config_for(data));
    PB_CHECK(!again.property("syncAfterSave").toBool());
}

PB_TEST(forgetting_the_account_keeps_the_auto_sync_choice) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    PB_CHECK(bridge.saveAccount("https://localhost:1", "alice", "pw"));
    bridge.setProperty("syncAfterSave", false);
    PB_CHECK(bridge.forgetAccount());
    PB_CHECK(!bridge.property("syncAfterSave").toBool());
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

// Spec 010: the note list's layout is a remembered choice, a list until the reader switches it.
PB_TEST(the_list_layout_is_list_on_a_fresh_install_and_remembers_a_change) {
    pbtest::TmpDir data;
    int announced = 0;
    {
        NotesBridge bridge(config_for(data));
        QObject::connect(&bridge, &NotesBridge::listLayoutChanged, [&] { ++announced; });
        PB_CHECK_EQ(bridge.property("listLayout").toString().toStdString(), std::string("list"));
        PB_CHECK(bridge.setProperty("listLayout", QStringLiteral("tile")));
        PB_CHECK_EQ(bridge.property("listLayout").toString().toStdString(), std::string("tile"));
        PB_CHECK_EQ(announced, 1);
        bridge.setProperty("listLayout", QStringLiteral("tile"));   // the same value again: nothing is announced
        PB_CHECK_EQ(announced, 1);
        bridge.setProperty("listLayout", QStringLiteral("grid"));   // not a layout: ignored
        PB_CHECK_EQ(bridge.property("listLayout").toString().toStdString(), std::string("tile"));
        PB_CHECK_EQ(announced, 1);
    }
    NotesBridge again(config_for(data));   // an app restart reads the stored choice
    PB_CHECK_EQ(again.property("listLayout").toString().toStdString(), std::string("tile"));
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

// ---- Spec 008: an automatic sync after a save ---------------------------------------------------------------------
// A small stateful Notes server over TLS that counts what the app sends, plus a rig that wires a bridge to it with
// short timings. Nothing here calls syncNow() unless the test says so: the sync must come from the save.

#include <map>
#include <mutex>

namespace {

struct AutoServer {
    struct Stored {
        std::string etag, content, title;   // title: what the app sent; empty = the first line of the content
    };
    std::mutex mu;
    std::map<int64_t, Stored> notes;
    int64_t next_id = 100;
    int version = 0;
    std::atomic<int> puts{0}, posts{0}, deletes{0}, lists{0};
    std::atomic<int> hold_put_ms{0};       // delay the answer to a PUT (a sync that takes a while)
    std::atomic<int> list_status{200};     // != 200: the list answer fails (a server that is down)
    std::atomic<bool> conflict_on_put{false};   // answer a PUT with 412: the note changed on the server
    std::vector<std::string> bodies;       // request bodies of the writes, in order

    static std::string note_json(int64_t id, const Stored& n) {
        const std::string title = !n.title.empty() ? n.title : n.content.substr(0, n.content.find("\\n"));
        return "{\"id\":" + std::to_string(id) + ",\"etag\":\"" + n.etag + "\",\"readonly\":false,\"title\":\"" + title + "\",\"content\":\"" +
               n.content + "\",\"category\":\"\",\"favorite\":false,\"modified\":1700000900}";
    }
    static std::string body_of(const std::string& req) {
        const size_t p = req.find("\r\n\r\n");
        return p == std::string::npos ? std::string() : req.substr(p + 4);
    }
    // the raw (still JSON-escaped) value of "content" in a request body
    static std::string content_of(const std::string& body, const std::string& key = "content") {
        const std::string needle = "\"" + key + "\":\"";
        const size_t k = body.find(needle);
        if (k == std::string::npos) return std::string();
        std::string out;
        for (size_t i = k + needle.size(); i < body.size(); ++i) {
            if (body[i] == '\\' && i + 1 < body.size()) { out += body[i]; out += body[++i]; continue; }
            if (body[i] == '"') break;
            out += body[i];
        }
        return out;
    }
    std::string handle(const std::string& req) {
        using R = pbtest::TlsTestServer;
        const std::string line = req.substr(0, req.find('\r'));
        if (req.find("/capabilities") != std::string::npos) return R::http_response(200, kCapabilities);
        std::lock_guard<std::mutex> l(mu);
        const size_t at = line.find("/api/v1/notes/");   // one note; the list is ".../api/v1/notes" without a slash
        const int64_t id = at == std::string::npos ? 0 : std::atoll(line.c_str() + at + 14);
        if (line.rfind("PUT ", 0) == 0) {
            ++puts;
            const std::string body = body_of(req);
            bodies.push_back(body);
            if (conflict_on_put) return R::http_response(412, "{}");
            if (hold_put_ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(hold_put_ms.load()));
            Stored& n = notes[id];
            n.content = content_of(body);
            n.title = content_of(body, "title");
            n.etag = "e" + std::to_string(id) + "-" + std::to_string(++version);
            return R::http_response(200, note_json(id, n));
        }
        if (line.rfind("POST ", 0) == 0) {
            ++posts;
            const std::string body = body_of(req);
            bodies.push_back(body);
            const int64_t nid = next_id++;
            Stored& n = notes[nid];
            n.content = content_of(body);
            n.title = content_of(body, "title");
            n.etag = "e" + std::to_string(nid) + "-" + std::to_string(++version);
            return R::http_response(200, note_json(nid, n));
        }
        if (line.rfind("DELETE ", 0) == 0) {
            ++deletes;
            notes.erase(id);
            return R::http_response(200, "{}");
        }
        if (at != std::string::npos) {   // GET one note
            auto it = notes.find(id);
            if (it == notes.end()) return R::http_response(404, "{}");
            const size_t inm = req.find("If-None-Match:");
            if (inm != std::string::npos && req.substr(inm, req.find('\r', inm) - inm).find(it->second.etag) != std::string::npos)
                return R::http_response(304, "");   // unchanged since the etag the app holds
            return R::http_response(200, note_json(id, it->second));
        }
        ++lists;
        if (list_status != 200) return R::http_response(list_status, "{}");
        std::string out = "[";
        for (const auto& kv : notes) out += (out.size() > 1 ? "," : "") + note_json(kv.first, kv.second);
        return R::http_response(200, out + "]");
    }
    int writes() const { return puts + posts + deletes; }
};

// A bridge with a reader-side note 7 ("Seven", synced, etag "etag7") and an account on the local server.
struct AutoRig {
    pbtest::TmpDir pkidir, data;
    pbtest::TestPki pki{pkidir, "privateca"};
    AutoServer state;
    std::unique_ptr<pbtest::TlsTestServer> server;
    std::unique_ptr<NotesBridge> bridge;
    std::atomic<bool> online{true};
    int finished = 0;
    QStringList outcomes, messages;
    int errors = 0;
    qlonglong note7 = 0;

    explicit AutoRig(const std::string& content7 = "Seven\nbody", int quiet_ms = 50, int gap_ms = 0) {
        if (!pki.available()) return;
        auto leaf = pki.issue("server", "DNS:localhost,IP:127.0.0.1");
        std::string escaped;   // the server keeps note text JSON-escaped, as it is echoed back
        for (char c : content7) { if (c == '\n') escaped += "\\n"; else escaped += c; }
        state.notes[7] = {"etag7", escaped};
        server = std::make_unique<pbtest::TlsTestServer>(leaf.cert, leaf.key, [this](const std::string& req) { return state.handle(req); });
        data.write("custom-ca.pem", slurp_file(pki.ca_cert()));
        seed(data, {remote_note(7, content7, "", 1700000000)});
        NotesBridge::Config c = config_for(data);
        c.autoSyncQuietMs = quiet_ms;
        c.autoSyncGapMs = gap_ms;
        c.wifiConnected = [this] { return online.load(); };
        bridge = std::make_unique<NotesBridge>(c);
        PB_CHECK(bridge->saveAccount(QString::fromStdString(server->url("/")), "alice", "pw"));
        QObject::connect(bridge.get(), &NotesBridge::syncFinished, [this](const QString& o, const QString& m) { ++finished; outcomes << o; messages << m; });
        QObject::connect(bridge.get(), &NotesBridge::errorOccurred, [this](const QString&, const QString&) { ++errors; });
        note7 = bridge->listNotes().at(0).toMap().value("localId").toLongLong();
    }
    bool ok() const { return bridge != nullptr; }
    void switch_on() { bridge->setProperty("syncAfterSave", true); }
    size_t requests() const { return server->requests().size(); }
};

bool pump_until(const std::function<bool()>& done, int timeout_ms) {
    QEventLoop loop;
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, [&] { if (done()) loop.quit(); });
    poll.start(10);
    QTimer::singleShot(timeout_ms, &loop, &QEventLoop::quit);
    if (!done()) loop.exec();
    return done();
}
void settle(int ms) {
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

}  // namespace

PB_TEST(a_saved_edit_is_sent_by_an_automatic_sync) {
    AutoRig rig;
    if (!rig.ok()) PB_SKIP("openssl CLI not available");
    rig.switch_on();
    PB_CHECK_EQ(rig.requests(), static_cast<size_t>(0));
    PB_CHECK_EQ(rig.bridge->saveNote(rig.note7, "Seven", "Seven\nedited", "", false), rig.note7);
    PB_CHECK_EQ(rig.requests(), static_cast<size_t>(0));      // the save did not wait for the network (FR-012)
    PB_CHECK(pump_until([&] { return rig.finished >= 1; }, 5000));
    PB_CHECK_EQ(rig.state.puts.load(), 1);
    PB_CHECK(rig.state.bodies.at(0).find("edited") != std::string::npos);
    PB_CHECK_EQ(rig.outcomes.at(0).toStdString(), std::string("success"));
    PB_CHECK(!rig.bridge->property("syncRunning").toBool());
    PB_CHECK_EQ(rig.bridge->property("pendingCount").toInt(), 0);
    PB_CHECK(rig.bridge->property("syncStatus").toString().contains("Last sync"));
}

// The Sync now button reads "Syncing..." from the moment an automatic sync is triggered (the save), through the quiet
// period and a Wi-Fi connection, until it ends.
PB_TEST(sync_is_active_from_the_save_until_the_automatic_sync_ends) {
    AutoRig rig(  "Seven\nbody", 300);
    if (!rig.ok()) PB_SKIP("openssl CLI not available");
    int changes = 0;
    QObject::connect(rig.bridge.get(), &NotesBridge::syncStateChanged, [&] { ++changes; });
    PB_CHECK(!rig.bridge->property("syncActive").toBool());
    rig.bridge->saveNote(rig.note7, "Seven", "Seven\nedited", "", false);
    PB_CHECK(rig.bridge->property("syncActive").toBool());          // triggered: waiting out the quiet period
    PB_CHECK(!rig.bridge->property("syncRunning").toBool());
    PB_CHECK(changes >= 1);                                          // QML is told
    PB_CHECK(pump_until([&] { return rig.finished >= 1; }, 5000));
    PB_CHECK(!rig.bridge->property("syncActive").toBool());          // and back to idle afterwards
}

PB_TEST(sync_is_active_while_the_device_connects_and_idle_again_when_it_fails) {
    AutoRig rig;
    if (!rig.ok()) PB_SKIP("openssl CLI not available");
    int requests = 0;
    rig.bridge->setWifiRequest([&] { ++requests; });
    rig.online = false;
    rig.bridge->saveNote(rig.note7, "Seven", "Seven\nedited", "", false);
    PB_CHECK(pump_until([&] { return requests >= 1; }, 3000));
    PB_CHECK(rig.bridge->property("syncActive").toBool());          // connecting counts
    rig.bridge->networkAnswered(false);
    PB_CHECK(!rig.bridge->property("syncActive").toBool());         // dropped quietly: the button is usable again
}

PB_TEST(sync_is_not_active_when_a_save_starts_nothing) {
    AutoRig rig;
    if (!rig.ok()) PB_SKIP("openssl CLI not available");
    rig.bridge->setProperty("syncAfterSave", false);
    rig.bridge->saveNote(rig.note7, "Seven", "Seven\nedited", "", false);
    PB_CHECK(!rig.bridge->property("syncActive").toBool());
    rig.bridge->setProperty("syncAfterSave", true);
    rig.online = false;                                              // no way to connect: dropped after the quiet period
    rig.bridge->saveNote(rig.note7, "Seven", "Seven\nedited again", "", false);
    PB_CHECK(pump_until([&] { return !rig.bridge->property("syncActive").toBool(); }, 3000));
}

PB_TEST(a_saved_new_note_is_created_on_the_server_by_an_automatic_sync) {
    AutoRig rig;
    if (!rig.ok()) PB_SKIP("openssl CLI not available");
    rig.switch_on();
    PB_CHECK(rig.bridge->saveNote(-1, "Fresh", "Fresh\nnote", "", false) > 0);
    PB_CHECK(pump_until([&] { return rig.finished >= 1; }, 5000));
    PB_CHECK_EQ(rig.state.posts.load(), 1);
    PB_CHECK(rig.state.bodies.at(0).find("Fresh") != std::string::npos);
}

PB_TEST(a_confirmed_delete_is_sent_by_an_automatic_sync) {
    AutoRig rig;
    if (!rig.ok()) PB_SKIP("openssl CLI not available");
    rig.switch_on();
    PB_CHECK(rig.bridge->deleteNote(rig.note7));
    PB_CHECK(pump_until([&] { return rig.finished >= 1; }, 5000));
    PB_CHECK_EQ(rig.state.deletes.load(), 1);
}

PB_TEST(favorite_category_and_task_changes_each_start_an_automatic_sync) {
    AutoRig rig("Tasks\n- [ ] one\n- [ ] two");
    if (!rig.ok()) PB_SKIP("openssl CLI not available");
    rig.switch_on();
    PB_CHECK(rig.bridge->toggleFavorite(rig.note7));
    PB_CHECK(pump_until([&] { return rig.finished >= 1; }, 5000));
    PB_CHECK_EQ(rig.state.puts.load(), 1);
    PB_CHECK(rig.bridge->setCategory(rig.note7, "Work"));
    PB_CHECK(pump_until([&] { return rig.finished >= 2; }, 5000));
    PB_CHECK_EQ(rig.state.puts.load(), 2);
    PB_CHECK(rig.bridge->toggleTask(rig.note7, 1));
    PB_CHECK(pump_until([&] { return rig.finished >= 3; }, 5000));
    PB_CHECK_EQ(rig.state.puts.load(), 3);
    PB_CHECK(rig.state.bodies.at(2).find("[x] one") != std::string::npos);
}

PB_TEST(with_the_switch_off_a_save_sends_nothing) {
    AutoRig rig;
    if (!rig.ok()) PB_SKIP("openssl CLI not available");
    bool ever_running = false;
    QObject::connect(rig.bridge.get(), &NotesBridge::syncStateChanged, [&] { if (rig.bridge->syncRunning()) ever_running = true; });
    rig.bridge->setProperty("syncAfterSave", false);
    rig.bridge->saveNote(rig.note7, "Seven", "Seven\nedited", "", false);
    rig.bridge->saveNote(-1, "Fresh", "Fresh\nnote", "", false);
    rig.bridge->toggleFavorite(rig.note7);
    rig.bridge->deleteNote(rig.note7);
    settle(500);
    PB_CHECK_EQ(rig.requests(), static_cast<size_t>(0));       // zero network activity (SC-003)
    PB_CHECK(!ever_running);
    PB_CHECK_EQ(rig.finished, 0);
}

PB_TEST(saving_an_unchanged_note_starts_no_sync) {
    AutoRig rig;
    if (!rig.ok()) PB_SKIP("openssl CLI not available");
    rig.switch_on();
    rig.bridge->saveNote(rig.note7, "Seven", "Seven\nbody", "", false);   // the text it already has
    settle(500);
    PB_CHECK_EQ(rig.requests(), static_cast<size_t>(0));
    PB_CHECK_EQ(rig.finished, 0);
}

PB_TEST(a_manual_sync_right_after_a_save_leaves_no_second_sync_behind) {
    AutoRig rig("Seven\nbody", 300, 0);
    if (!rig.ok()) PB_SKIP("openssl CLI not available");
    rig.switch_on();
    rig.bridge->saveNote(rig.note7, "Seven", "Seven\nedited", "", false);
    rig.bridge->syncNow();                                    // inside the quiet period: it carries the change
    PB_CHECK(pump_until([&] { return rig.finished >= 1; }, 5000));
    settle(900);                                               // longer than the quiet period
    PB_CHECK_EQ(rig.finished, 1);
    PB_CHECK_EQ(rig.state.puts.load(), 1);
}

PB_TEST(an_automatic_sync_keeps_both_versions_when_the_server_changed_the_note) {
    AutoRig rig;
    if (!rig.ok()) PB_SKIP("openssl CLI not available");
    {
        std::lock_guard<std::mutex> l(rig.state.mu);
        rig.state.notes[7] = {"e7new", "Seven\\nserver text"};
        rig.state.conflict_on_put = true;
    }
    int conflicts = -1, restored = -1;
    QObject::connect(rig.bridge.get(), &NotesBridge::conflictsDetected, [&](int c, int r) { conflicts = c; restored = r; });
    rig.switch_on();
    rig.bridge->saveNote(rig.note7, "Seven", "Seven\nmy text", "", false);
    PB_CHECK(pump_until([&] { return rig.finished >= 1; }, 5000));
    PB_CHECK_EQ(conflicts, 1);                    // handled as a manual sync handles it (FR-017)
    PB_CHECK_EQ(restored, 0);
    PB_CHECK_EQ(rig.state.posts.load(), 1);        // the reader's text went up as a copy ...
    PB_CHECK(rig.state.bodies.back().find("my text") != std::string::npos);
    bool has_server = false, has_copy = false;     // ... and both versions are in the list
    for (const QVariant& v : rig.bridge->listNotes()) {
        const std::string title = v.toMap().value("title").toString().toStdString();
        if (title == "Seven") has_server = true;
        if (title.find("(conflict ") != std::string::npos) has_copy = true;
    }
    PB_CHECK(has_server);
    PB_CHECK(has_copy);
}

PB_TEST(markdown_of_feature_006_reaches_the_server_unchanged_by_an_automatic_sync) {
    AutoRig rig;
    if (!rig.ok()) PB_SKIP("openssl CLI not available");
    rig.switch_on();
    const QString text = "Formats\n```yaml\nkey:\n  nested: 1\n```\n> [!NOTE]\n> careful\n<details>\n<summary>more</summary>\nhidden\n</details>";
    PB_CHECK(rig.bridge->saveNote(-1, "Formats", text, "", false) > 0);
    PB_CHECK(pump_until([&] { return rig.finished >= 1; }, 5000));
    PB_CHECK_EQ(rig.state.posts.load(), 1);
    const std::string sent = AutoServer::content_of(rig.state.bodies.at(0));
    std::string expected;
    for (char c : text.toStdString()) { if (c == '\n') expected += "\\n"; else expected += c; }
    PB_CHECK_EQ(sent, expected);                   // byte for byte (FR-009)
}

// ---- Spec 008: a sync that ran while the editor was open must not be overwritten by the editor's older text ---------

namespace {
struct ConflictCatcher {
    int conflicts = -1, restored = -1, emitted = 0;
    explicit ConflictCatcher(NotesBridge& b) {
        QObject::connect(&b, &NotesBridge::conflictsDetected, [this](int c, int r) { conflicts = c; restored = r; ++emitted; });
    }
};
// What a pull does to an unchanged note: a newer server version with a new validator.
void pull_new_version(NotesBridge& b, int64_t remote_id, const std::string& content) {
    pbnotes::ApiNote n = remote_note(remote_id, content, "", 1700000500);
    n.etag = "etag" + std::to_string(remote_id) + "-newer";
    b.store()->apply_remote({n});
}
int count_conflict_copies(const NotesBridge& b) {
    int n = 0;
    for (const QVariant& v : b.listNotes())
        if (v.toMap().value("title").toString().contains("(conflict ")) ++n;
    return n;
}
}  // namespace

PB_TEST(saving_stale_editor_text_after_a_pull_keeps_both_versions) {
    pbtest::TmpDir data;
    seed(data, {remote_note(7, "Plan\nold text", "", 1700000000)});
    NotesBridge bridge(config_for(data));
    ConflictCatcher dialog(bridge);
    const qlonglong id = bridge.listNotes().at(0).toMap().value("localId").toLongLong();
    const QVariantMap loaded = bridge.loadForEdit(id);
    PB_CHECK_EQ(loaded.value("text").toString().toStdString(), std::string("Plan\nold text"));
    pull_new_version(bridge, 7, "Plan\nserver text");                                      // a sync, meanwhile
    const qlonglong saved = bridge.saveNote(id, "Plan", "Plan\nmy older text", "", false);
    PB_CHECK(saved > 0);
    PB_CHECK(saved != id);                                                                  // the text went to a new note
    PB_CHECK_EQ(bridge.loadForEdit(saved).value("text").toString().toStdString(), std::string("Plan\nmy older text"));
    PB_CHECK(bridge.loadForEdit(saved).value("title").toString().contains("(conflict "));
    PB_CHECK_EQ(bridge.loadForEdit(id).value("text").toString().toStdString(), std::string("Plan\nserver text"));   // untouched
    PB_CHECK_EQ(dialog.conflicts, 1);
    PB_CHECK_EQ(dialog.restored, 0);
    PB_CHECK_EQ(dialog.emitted, 1);
    PB_CHECK_EQ(count_conflict_copies(bridge), 1);
    PB_CHECK_EQ(bridge.property("pendingCount").toInt(), 1);                                // the copy waits for the next sync
}

PB_TEST(saving_editor_text_nobody_changed_underneath_is_an_ordinary_save) {
    pbtest::TmpDir data;
    seed(data, {remote_note(7, "Plan\nold text", "", 1700000000)});
    NotesBridge bridge(config_for(data));
    ConflictCatcher dialog(bridge);
    const qlonglong id = bridge.listNotes().at(0).toMap().value("localId").toLongLong();
    bridge.loadForEdit(id);
    PB_CHECK_EQ(bridge.saveNote(id, "Plan", "Plan\nnew text", "", false), id);
    PB_CHECK_EQ(dialog.emitted, 0);
    PB_CHECK_EQ(count_conflict_copies(bridge), 0);
    PB_CHECK_EQ(bridge.listNotes().size(), 1);
    // the same text with only a new validator (the reader's own push, confirmed meanwhile) is no conflict either
    bridge.loadForEdit(id);
    pull_new_version(bridge, 7, "Plan\nnew text");
    PB_CHECK_EQ(bridge.saveNote(id, "Plan", "Plan\nnewer text", "", false), id);
    PB_CHECK_EQ(dialog.emitted, 0);
}

PB_TEST(a_note_that_only_exists_here_and_a_new_note_are_saved_as_before) {
    pbtest::TmpDir data;
    NotesBridge bridge(config_for(data));
    ConflictCatcher dialog(bridge);
    const qlonglong fresh = bridge.saveNote(-1, "Mine", "Mine\none", "", false);
    bridge.loadForEdit(fresh);
    PB_CHECK_EQ(bridge.saveNote(fresh, "Mine", "Mine\ntwo", "", false), fresh);
    PB_CHECK_EQ(dialog.emitted, 0);
    PB_CHECK(bridge.saveNote(-1, "Other", "Other\ntext", "", false) > 0);
    PB_CHECK_EQ(dialog.emitted, 0);
}

PB_TEST(opening_another_note_does_not_disturb_the_remembered_version_of_the_first) {
    pbtest::TmpDir data;
    seed(data, {remote_note(7, "Plan\nold text", "", 1700000000), remote_note(8, "Other\nbody", "", 1700000100)});
    NotesBridge bridge(config_for(data));
    ConflictCatcher dialog(bridge);
    qlonglong plan = 0, other = 0;
    for (const QVariant& v : bridge.listNotes()) {
        const QVariantMap m = v.toMap();
        (m.value("title").toString() == "Plan" ? plan : other) = m.value("localId").toLongLong();
    }
    bridge.loadForEdit(plan);
    bridge.loadForEdit(other);
    pull_new_version(bridge, 7, "Plan\nserver text");
    PB_CHECK(bridge.saveNote(plan, "Plan", "Plan\nmine", "", false) != plan);               // still guarded
    PB_CHECK_EQ(dialog.emitted, 1);
}

PB_TEST(a_refused_save_and_a_save_without_loading_keep_working_and_a_saved_note_forgets_its_version) {
    pbtest::TmpDir data;
    seed(data, {remote_note(7, "Plan\nold text", "", 1700000000)});
    NotesBridge bridge(config_for(data));
    ConflictCatcher dialog(bridge);
    ErrorCatcher errors(bridge);
    const qlonglong id = bridge.listNotes().at(0).toMap().value("localId").toLongLong();
    // never loaded: no version to compare with, an ordinary save (the reader's checkbox and other callers)
    pull_new_version(bridge, 7, "Plan\nserver text");
    PB_CHECK_EQ(bridge.saveNote(id, "Plan", "Plan\nwithout loading", "", false), id);
    PB_CHECK_EQ(dialog.emitted, 0);
    // an invalid save reports its error and keeps the remembered version for the retry
    bridge.loadForEdit(id);
    PB_CHECK_EQ(bridge.saveNote(id, "Plan", "   ", "", false), qlonglong(-1));
    PB_CHECK(!errors.keys.isEmpty());
    PB_CHECK_EQ(bridge.saveNote(id, "Plan", "Plan\nretry", "", false), id);
    // after a successful save the version is forgotten: the editor has closed
    pull_new_version(bridge, 7, "Plan\nserver text again");
    PB_CHECK_EQ(bridge.saveNote(id, "Plan", "Plan\nno longer guarded", "", false), id);
    PB_CHECK_EQ(dialog.emitted, 0);
}

// ---- Spec 008, offline and busy situations ----------------------------------------------------------------------

PB_TEST(without_wifi_a_save_is_kept_quietly_and_goes_out_with_the_next_sync) {
    AutoRig rig;
    if (!rig.ok()) PB_SKIP("openssl CLI not available");
    rig.switch_on();
    rig.online = false;
    PB_CHECK(rig.bridge->saveNote(rig.note7, "Seven", "Seven\nedited offline", "", false) > 0);
    settle(500);
    PB_CHECK_EQ(rig.requests(), static_cast<size_t>(0));       // no attempt
    PB_CHECK_EQ(rig.errors, 0);                                // no error
    PB_CHECK_EQ(rig.finished, 0);                              // no failed sync either
    PB_CHECK_EQ(rig.bridge->property("pendingCount").toInt(), 1);   // still waiting
    settle(500);                                               // and no retry when time passes
    PB_CHECK_EQ(rig.requests(), static_cast<size_t>(0));

    rig.online = true;
    PB_CHECK(rig.bridge->saveNote(-1, "Later", "Later\nnote", "", false) > 0);
    PB_CHECK(pump_until([&] { return rig.finished >= 1; }, 5000));
    PB_CHECK_EQ(rig.state.puts.load(), 1);                     // the earlier waiting note went with it
    PB_CHECK_EQ(rig.state.posts.load(), 1);
    PB_CHECK_EQ(rig.bridge->property("pendingCount").toInt(), 0);
}

// The reader's Wi-Fi is normally off (first device test, 2026-10-12: the sync was dropped as offline). Auto Sync asks
// the device to connect, as "Sync now" does, and syncs once the connection is up.
PB_TEST(without_wifi_auto_sync_asks_the_device_to_connect_and_then_syncs) {
    AutoRig rig;
    if (!rig.ok()) PB_SKIP("openssl CLI not available");
    int requests = 0;
    rig.bridge->setWifiRequest([&] { ++requests; });
    rig.online = false;
    PB_CHECK(rig.bridge->saveNote(rig.note7, "Seven", "Seven\nedited offline", "", false) > 0);
    PB_CHECK(pump_until([&] { return requests >= 1; }, 3000));
    PB_CHECK_EQ(rig.requests(), static_cast<size_t>(0));        // nothing is sent before the connection is up
    PB_CHECK_EQ(rig.errors, 0);
    rig.online = true;
    rig.bridge->networkAnswered(true);
    PB_CHECK(pump_until([&] { return rig.finished >= 1; }, 5000));
    PB_CHECK_EQ(rig.state.puts.load(), 1);
    PB_CHECK_EQ(rig.bridge->property("pendingCount").toInt(), 0);
    PB_CHECK_EQ(requests, 1);
}

PB_TEST(a_connection_that_fails_ends_quietly_and_saves_made_while_connecting_share_it) {
    AutoRig rig;
    if (!rig.ok()) PB_SKIP("openssl CLI not available");
    int requests = 0;
    rig.bridge->setWifiRequest([&] { ++requests; });
    rig.online = false;
    rig.bridge->saveNote(rig.note7, "Seven", "Seven\nedited", "", false);
    PB_CHECK(pump_until([&] { return requests >= 1; }, 3000));
    rig.bridge->saveNote(-1, "Fresh", "Fresh\nnote", "", false);     // while the device is connecting
    settle(300);
    PB_CHECK_EQ(requests, 1);                                          // one connection request, not two
    rig.bridge->networkAnswered(false);                                // Wi-Fi did not come up
    settle(500);
    PB_CHECK_EQ(rig.requests(), static_cast<size_t>(0));               // no sync, no retry
    PB_CHECK_EQ(rig.errors, 0);
    PB_CHECK_EQ(rig.finished, 0);
    PB_CHECK_EQ(rig.bridge->property("pendingCount").toInt(), 2);      // both still waiting for the next sync
    rig.bridge->networkAnswered(true);                                 // a stray answer starts nothing
    settle(300);
    PB_CHECK_EQ(rig.requests(), static_cast<size_t>(0));
}

PB_TEST(a_failed_automatic_sync_is_reported_in_the_status_and_not_retried) {
    AutoRig rig;
    if (!rig.ok()) PB_SKIP("openssl CLI not available");
    rig.state.list_status = 503;
    int dialogs = 0;
    QObject::connect(rig.bridge.get(), &NotesBridge::conflictsDetected, [&](int, int) { ++dialogs; });
    rig.switch_on();
    rig.bridge->saveNote(rig.note7, "Seven", "Seven\nedited", "", false);
    PB_CHECK(pump_until([&] { return rig.finished >= 1; }, 5000));
    PB_CHECK(rig.outcomes.at(0) != QString("success"));
    PB_CHECK(!rig.bridge->property("syncStatus").toString().isEmpty());
    settle(1000);                                              // quiet period and gap are long over
    PB_CHECK_EQ(rig.finished, 1);                              // no retry loop (FR-016)
    PB_CHECK_EQ(rig.state.lists.load(), 1);
    PB_CHECK_EQ(dialogs, 0);
    PB_CHECK_EQ(rig.errors, 0);
}

PB_TEST(a_save_while_a_sync_runs_is_sent_by_exactly_one_more_sync) {
    AutoRig rig;
    if (!rig.ok()) PB_SKIP("openssl CLI not available");
    rig.state.hold_put_ms = 400;
    rig.switch_on();
    rig.bridge->saveNote(rig.note7, "Seven", "Seven\nfirst", "", false);
    // wait until the first PUT is on the server: the sync has read its list, a later save is not part of it
    PB_CHECK(pump_until([&] { return rig.state.puts.load() >= 1 && rig.bridge->syncRunning(); }, 3000));
    rig.bridge->saveNote(rig.note7, "Seven", "Seven\nsecond", "", false);   // while that sync is running
    PB_CHECK(pump_until([&] { return rig.finished >= 2; }, 8000));
    settle(700);
    PB_CHECK_EQ(rig.finished, 2);                              // never overlapping, exactly one follow-up
    PB_CHECK_EQ(rig.state.puts.load(), 2);
    PB_CHECK(rig.state.bodies.back().find("second") != std::string::npos);
    PB_CHECK_EQ(rig.bridge->property("pendingCount").toInt(), 0);
}

PB_TEST(saves_close_together_share_one_sync) {
    AutoRig rig("Seven\nbody", 250, 0);
    if (!rig.ok()) PB_SKIP("openssl CLI not available");
    rig.switch_on();
    for (int i = 0; i < 5; ++i) {
        PB_CHECK(rig.bridge->saveNote(-1, "N", QString("N%1\ntext").arg(i), "", false) > 0);
        settle(20);
    }
    PB_CHECK(pump_until([&] { return rig.finished >= 1; }, 5000));
    settle(700);
    PB_CHECK_EQ(rig.finished, 1);
    PB_CHECK_EQ(rig.state.posts.load(), 5);                    // all five went in that one sync
}

PB_TEST(switching_off_during_a_sync_lets_it_finish_and_starts_no_more) {
    AutoRig rig;
    if (!rig.ok()) PB_SKIP("openssl CLI not available");
    rig.state.hold_put_ms = 400;
    rig.switch_on();
    rig.bridge->saveNote(rig.note7, "Seven", "Seven\nfirst", "", false);
    PB_CHECK(pump_until([&] { return rig.state.puts.load() >= 1 && rig.bridge->syncRunning(); }, 3000));
    rig.bridge->saveNote(rig.note7, "Seven", "Seven\nsecond", "", false);   // would need a follow-up ...
    rig.bridge->setProperty("syncAfterSave", false);                        // ... but the reader switches it off
    PB_CHECK(pump_until([&] { return rig.finished >= 1; }, 5000));
    settle(900);
    PB_CHECK_EQ(rig.finished, 1);                              // the running sync finished normally, nothing followed
    PB_CHECK_EQ(rig.outcomes.at(0).toStdString(), std::string("success"));
}

PB_TEST(without_an_account_a_save_starts_no_sync_and_reports_no_error) {
    pbtest::TmpDir data;
    NotesBridge::Config c = config_for(data);
    c.autoSyncQuietMs = 50;
    c.autoSyncGapMs = 0;
    c.wifiConnected = [] { return true; };
    NotesBridge bridge(c);
    int errors = 0, finished = 0;
    QObject::connect(&bridge, &NotesBridge::errorOccurred, [&](const QString&, const QString&) { ++errors; });
    QObject::connect(&bridge, &NotesBridge::syncFinished, [&](const QString&, const QString&) { ++finished; });
    bridge.setProperty("syncAfterSave", true);                 // allowed without an account
    PB_CHECK(bridge.property("syncAfterSave").toBool());
    PB_CHECK(bridge.saveNote(-1, "Local", "Local\nonly", "", false) > 0);
    settle(500);
    PB_CHECK_EQ(errors, 0);
    PB_CHECK_EQ(finished, 0);
    PB_CHECK(!bridge.property("syncRunning").toBool());
}

PB_TEST(no_automatic_sync_starts_while_an_editor_is_open) {
    AutoRig rig;
    if (!rig.ok()) PB_SKIP("openssl CLI not available");
    rig.switch_on();
    rig.bridge->setEditorOpen(true);
    rig.bridge->saveNote(rig.note7, "Seven", "Seven\nedited", "", false);
    settle(500);
    PB_CHECK_EQ(rig.requests(), static_cast<size_t>(0));       // deferred (a pull could replace the note being edited)
    rig.bridge->setEditorOpen(false);
    PB_CHECK(pump_until([&] { return rig.finished >= 1; }, 5000));
    PB_CHECK_EQ(rig.state.puts.load(), 1);
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    return pbtest::run_all();
}
