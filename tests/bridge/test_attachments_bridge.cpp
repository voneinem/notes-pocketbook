// US5 (contracts/qml-bridge.md "AttachmentsBridge"): adding a file from the editor, the file browser listing,
// discarding unsaved attachments, and how pending attachments show in the reader and the editor.
#include <sys/stat.h>
#include <unistd.h>

#include <fstream>
#include <string>

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include "attachments_bridge.h"
#include "notes_bridge.h"
#include "pbnotes/note_store.h"
#include "test_support.h"
#include "tmp_dir.h"

namespace {

struct Env {
    pbtest::TmpDir data;
    pbtest::TmpDir files;
    std::unique_ptr<NotesBridge> notes;
    std::unique_ptr<AttachmentsBridge> attachments;

    explicit Env(const std::string& api_versions = "1.0, 1.3, 1.4") {
        {   // an account known to the store before the bridge opens it
            auto store = pbnotes::NoteStore::open(data.file("pbnotes.db"));
            pbnotes::Account a;
            a.server_url = "https://cloud.example.com";
            a.username = "alice";
            a.password = "pw";
            a.api_versions = api_versions;
            store->save_account(a);
        }
        NotesBridge::Config c;
        c.dataDir = QString::fromStdString(data.path());
        c.bundledCaPath = QString::fromStdString(std::string(PBNOTES_SOURCE_DIR) + "/third_party/cacert/cacert.pem");
        notes.reset(new NotesBridge(c));
        AttachmentsBridge::Config ac;
        ac.rootDirectory = QString::fromStdString(files.path());
        attachments.reset(new AttachmentsBridge(notes.get(), ac));
    }
    std::string make(const std::string& name, const std::string& content) {
        files.write(name, content);
        return files.file(name);
    }
    // Starts staging; returns the result map once stageFinished arrives. `*finished_during_call` (if given) tells
    // whether the signal came before stageAttachment returned (it must not: the work is on a worker thread).
    QVariantMap stage(const std::string& path, bool* finished_during_call = nullptr) {
        int count = 0;
        QVariantMap result;
        QEventLoop loop;
        auto conn = QObject::connect(attachments.get(), &AttachmentsBridge::stageFinished, &loop, [&](const QVariantMap& m) {
            ++count;
            result = m;
            loop.quit();
        });
        attachments->stageAttachment(QString::fromStdString(path));
        if (finished_during_call) *finished_during_call = count > 0;
        if (count == 0) {
            QTimer::singleShot(10000, &loop, &QEventLoop::quit);
            loop.exec();
        }
        QObject::disconnect(conn);
        PB_CHECK_EQ(count, 1);
        return result;
    }
};

QString s(const QVariantMap& m, const char* key) { return m.value(QString::fromLatin1(key)).toString(); }

}  // namespace

PB_TEST(supported_follows_the_stored_api_versions) {
    Env with("1.0, 1.3, 1.4");
    PB_CHECK(with.attachments->supported());
    Env without("1.0, 1.3");
    PB_CHECK(!without.attachments->supported());
    PB_CHECK_EQ(with.attachments->maxBytes(), static_cast<qlonglong>(10 * 1024 * 1024));
    PB_CHECK_EQ(with.attachments->rootDirectory().toStdString(), with.files.path());
}

PB_TEST(staging_a_file_reports_the_token_name_and_size_after_the_call_returned) {
    Env e;
    const std::string path = e.make("photo one.png", "PNGDATA");
    bool during = true;
    const QVariantMap m = e.stage(path, &during);
    PB_CHECK(!during);   // the copy runs on a worker thread: the call itself returns at once
    PB_CHECK(m.value("ok").toBool());
    PB_CHECK_EQ(s(m, "name").toStdString(), std::string("photo one.png"));
    PB_CHECK_EQ(m.value("size").toLongLong(), static_cast<qlonglong>(7));
    const qlonglong id = m.value("id").toLongLong();
    PB_CHECK(id > 0);
    PB_CHECK_EQ(s(m, "token").toStdString(), "![photo one.png](pbatt:" + std::to_string(id) + ")");
    PB_CHECK(QFile::exists(QString::fromStdString(path)));   // the original is still there
    auto row = e.notes->store()->get_attachment(id);
    PB_CHECK(row.has_value() && row->state == pbnotes::AttachmentState::Staged);
    PB_CHECK(QFile::exists(e.notes->attachmentsDir() + QLatin1Char('/') + QString::fromStdString(row->local_file)));
}

PB_TEST(a_file_that_cannot_be_added_reports_a_reason_in_plain_words) {
    Env e;
    const QVariantMap missing = e.stage(e.files.file("nope.png"));
    PB_CHECK(!missing.value("ok").toBool());
    PB_CHECK_EQ(s(missing, "errorKey").toStdString(), std::string("unreadable"));
    PB_CHECK(!s(missing, "errorText").isEmpty());
    PB_CHECK(!s(missing, "errorText").contains(QStringLiteral("nope")));
    const std::string big = e.files.file("big.bin");
    {
        std::ofstream f(big, std::ios::binary);
        f.seekp(pbnotes::kMaxAttachmentBytes);
        f.put('x');
    }
    const QVariantMap too_big = e.stage(big);
    PB_CHECK_EQ(s(too_big, "errorKey").toStdString(), std::string("too_large"));
}

PB_TEST(saving_a_note_with_the_token_makes_the_attachment_pending_and_the_editor_shows_the_token) {
    Env e;
    const QVariantMap m = e.stage(e.make("a.png", "DATA"));
    const QString token = s(m, "token");
    const qlonglong note = e.notes->saveNote(-1, QStringLiteral("Title\n") + token + QStringLiteral("\n"), QString(), false);
    PB_CHECK(note > 0);
    auto row = e.notes->store()->get_attachment(m.value("id").toLongLong());
    PB_CHECK(row->state == pbnotes::AttachmentState::Pending);
    PB_CHECK_EQ(static_cast<qlonglong>(row->note_local_id.value_or(0)), note);
    PB_CHECK(e.notes->loadForEdit(note).value("text").toString().contains(token));
    e.attachments->discardStaged();   // leaving the editor after the save: the bound row stays
    PB_CHECK(e.notes->store()->get_attachment(m.value("id").toLongLong()).has_value());
}

PB_TEST(discarding_drops_the_unsaved_attachments_and_their_files_go_with_the_janitor) {
    Env e;
    const QVariantMap m = e.stage(e.make("a.png", "DATA"));
    const qlonglong id = m.value("id").toLongLong();
    e.attachments->discardStaged();
    PB_CHECK(!e.notes->store()->get_attachment(id).has_value());
}

PB_TEST(a_pending_attachment_shows_in_the_reader_as_pending_with_a_local_image) {
    Env e;
    const QVariantMap img = e.stage(e.make("a.png", "DATA"));
    const QVariantMap doc = e.stage(e.make("d.pdf", "DOC"));
    const qlonglong note = e.notes->saveNote(-1, QStringLiteral("Title\n") + s(img, "token") + QStringLiteral("\n") + s(doc, "token"), QString(), false);
    const QVariantList blocks = e.notes->openNote(note).value("blocks").toList();
    QVariantMap image_block, file_block;
    for (const QVariant& v : blocks) {
        const QVariantMap b = v.toMap();
        if (s(b, "kind") != QLatin1String("attachment")) continue;
        if (s(b, "attKind") == QLatin1String("image")) image_block = b;
        else file_block = b;
    }
    PB_CHECK_EQ(s(image_block, "attState").toStdString(), std::string("pending"));
    PB_CHECK_EQ(s(image_block, "imageSource").toStdString(), "image://attachment/" + std::to_string(img.value("id").toLongLong()));
    PB_CHECK_EQ(s(file_block, "attState").toStdString(), std::string("pending"));
    PB_CHECK(s(file_block, "imageSource").isEmpty());
    PB_CHECK_EQ(image_block.value("size").toLongLong(), static_cast<qlonglong>(4));
}

PB_TEST(a_token_whose_row_belongs_to_another_note_is_not_shown_as_an_attachment_of_this_one) {
    Env e;
    const QVariantMap m = e.stage(e.make("a.png", "DATA"));
    const qlonglong first = e.notes->saveNote(-1, QStringLiteral("One\n") + s(m, "token"), QString(), false);
    const qlonglong other = e.notes->saveNote(-1, QStringLiteral("Two\n") + s(m, "token"), QString(), false);
    PB_CHECK(first > 0 && other > 0);
    bool found_pending = false;
    for (const QVariant& v : e.notes->openNote(other).value("blocks").toList())
        if (s(v.toMap(), "kind") == QLatin1String("attachment")) found_pending = s(v.toMap(), "attState") == QLatin1String("pending");
    PB_CHECK(!found_pending);   // it stays the first note's attachment
}

PB_TEST(the_file_browser_lists_directories_first_then_files_ignoring_case_and_hidden_entries) {
    Env e;
    ::mkdir(e.files.file("Zeta").c_str(), 0755);
    ::mkdir(e.files.file("alpha").c_str(), 0755);
    ::mkdir(e.files.file(".hidden_dir").c_str(), 0755);
    e.make("b.txt", "12345");
    e.make("A.pdf", "x");
    e.make(".secret", "x");
    const QVariantList list = e.attachments->listDirectory(QString::fromStdString(e.files.path()));
    QStringList names;
    for (const QVariant& v : list) names << s(v.toMap(), "name");
    PB_CHECK_EQ(names.join(QLatin1Char('|')).toStdString(), std::string("alpha|Zeta|A.pdf|b.txt"));
    PB_CHECK(list.at(0).toMap().value("dir").toBool());
    PB_CHECK(!list.at(2).toMap().value("dir").toBool());
    PB_CHECK_EQ(list.at(3).toMap().value("size").toLongLong(), static_cast<qlonglong>(5));
}

PB_TEST(an_unreadable_or_missing_directory_lists_nothing) {
    Env e;
    PB_CHECK(e.attachments->listDirectory(QString::fromStdString(e.files.file("missing"))).isEmpty());
    PB_CHECK(e.attachments->listDirectory(QString()).isEmpty());
    ::mkdir(e.files.file("locked").c_str(), 0755);
    ::chmod(e.files.file("locked").c_str(), 0);
    if (::access(e.files.file("locked").c_str(), R_OK) != 0)
        PB_CHECK(e.attachments->listDirectory(QString::fromStdString(e.files.file("locked"))).isEmpty());
    ::chmod(e.files.file("locked").c_str(), 0755);
}

PB_TEST(entries_that_are_links_to_nowhere_or_unreadable_are_skipped) {
    Env e;
    e.make("ok.txt", "x");
    ::symlink(e.files.file("gone-target").c_str(), e.files.file("dangling").c_str());
    const QVariantList list = e.attachments->listDirectory(QString::fromStdString(e.files.path()));
    QStringList names;
    for (const QVariant& v : list) names << s(v.toMap(), "name");
    PB_CHECK(names.contains(QStringLiteral("ok.txt")));
    PB_CHECK(!names.contains(QStringLiteral("dangling")));
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    return pbtest::run_all();
}
