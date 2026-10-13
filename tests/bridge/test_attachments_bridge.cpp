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
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include "attachments_bridge.h"
#include "file_preview_provider.h"
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
    const qlonglong note = e.notes->saveNote(-1, QStringLiteral("Title"), QStringLiteral("Title\n") + token + QStringLiteral("\n"), QString(), false);
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
    const qlonglong note = e.notes->saveNote(-1, QStringLiteral("Title"), QStringLiteral("Title\n") + s(img, "token") + QStringLiteral("\n") + s(doc, "token"), QString(), false);
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
    const qlonglong first = e.notes->saveNote(-1, QStringLiteral("One"), QStringLiteral("One\n") + s(m, "token"), QString(), false);
    const qlonglong other = e.notes->saveNote(-1, QStringLiteral("Two"), QStringLiteral("Two\n") + s(m, "token"), QString(), false);
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

// ---- spec 009 US1: the browser's top level shows only the named folders ----------------------------------------------

namespace {

QStringList names_of(const QVariantList& list) {
    QStringList out;
    for (const QVariant& v : list) out << s(v.toMap(), "name");
    return out;
}

// A bridge whose browser root is `root` and whose top level is limited to the seven folders of the device.
std::unique_ptr<AttachmentsBridge> seven_folder_bridge(Env& e, const std::string& root) {
    AttachmentsBridge::Config c;
    c.rootDirectory = QString::fromStdString(root);
    c.topFolders = AttachmentsBridge::defaultTopFolders();
    return std::unique_ptr<AttachmentsBridge>(new AttachmentsBridge(e.notes.get(), c));
}

}  // namespace

PB_TEST(the_default_top_level_is_the_seven_named_folders) {
    const QStringList f = AttachmentsBridge::defaultTopFolders();
    PB_CHECK_EQ(f.join(QLatin1Char('|')).toStdString(), std::string("Audio Books|Downloads|Music|My pictures|Notes|Photo|screens"));
}

PB_TEST(the_top_level_lists_only_the_seven_folders_and_no_files) {
    Env e;
    for (const char* d : {"Audio Books", "Downloads", "Music", "My pictures", "Notes", "Photo", "screens", "System", "Other"})
        ::mkdir(e.files.file(d).c_str(), 0755);
    e.make("Autorun.inf", "[autorun]");
    e.make("install.md", "# install");
    auto bridge = seven_folder_bridge(e, e.files.path());
    const QStringList names = names_of(bridge->listDirectory(QString::fromStdString(e.files.path())));
    PB_CHECK_EQ(names.join(QLatin1Char('|')).toStdString(), std::string("Audio Books|Downloads|Music|My pictures|Notes|Photo|screens"));
    for (const QVariant& v : bridge->listDirectory(QString::fromStdString(e.files.path()))) PB_CHECK(v.toMap().value("dir").toBool());
}

PB_TEST(top_level_names_match_whole_and_ignoring_case) {
    Env e;
    for (const char* d : {"photo", "SCREENS", "Photos", "Photo2", "my Pictures", "Music backup"})
        ::mkdir(e.files.file(d).c_str(), 0755);
    auto bridge = seven_folder_bridge(e, e.files.path());
    const QStringList names = names_of(bridge->listDirectory(QString::fromStdString(e.files.path())));
    PB_CHECK_EQ(names.join(QLatin1Char('|')).toStdString(), std::string("my Pictures|photo|SCREENS"));
}

PB_TEST(a_file_with_a_folders_name_is_not_listed_at_the_top_level_but_a_link_to_a_folder_is) {
    Env e;
    e.make("Music", "not a folder");
    ::mkdir(e.files.file("real-notes").c_str(), 0755);
    ::symlink(e.files.file("real-notes").c_str(), e.files.file("Notes").c_str());
    auto bridge = seven_folder_bridge(e, e.files.path());
    const QVariantList list = bridge->listDirectory(QString::fromStdString(e.files.path()));
    PB_CHECK_EQ(names_of(list).join(QLatin1Char('|')).toStdString(), std::string("Notes"));
    PB_CHECK(list.at(0).toMap().value("dir").toBool());
}

PB_TEST(only_the_existing_named_folders_are_listed_and_none_gives_an_empty_list) {
    Env e;
    ::mkdir(e.files.file("Downloads").c_str(), 0755);
    ::mkdir(e.files.file("Stuff").c_str(), 0755);
    e.make("install.md", "x");
    auto bridge = seven_folder_bridge(e, e.files.path());
    PB_CHECK_EQ(names_of(bridge->listDirectory(QString::fromStdString(e.files.path()))).join(QLatin1Char('|')).toStdString(), std::string("Downloads"));
    Env none;
    none.make("Autorun.inf", "x");
    ::mkdir(none.files.file("Other").c_str(), 0755);
    auto bridge2 = seven_folder_bridge(none, none.files.path());
    PB_CHECK(bridge2->listDirectory(QString::fromStdString(none.files.path())).isEmpty());
}

PB_TEST(inside_a_named_folder_everything_is_listed_in_the_old_order) {
    Env e;
    ::mkdir(e.files.file("Photo").c_str(), 0755);
    ::mkdir(e.files.file("Photo/Zeta").c_str(), 0755);
    ::mkdir(e.files.file("Photo/alpha").c_str(), 0755);
    e.make("Photo/b.txt", "12345");
    e.make("Photo/A.pdf", "x");
    e.make("Photo/Other.log", "x");
    auto bridge = seven_folder_bridge(e, e.files.path());
    const QStringList names = names_of(bridge->listDirectory(QString::fromStdString(e.files.file("Photo"))));
    PB_CHECK_EQ(names.join(QLatin1Char('|')).toStdString(), std::string("alpha|Zeta|A.pdf|b.txt|Other.log"));
    ::mkdir(e.files.file("Photo/alpha/Inner").c_str(), 0755);   // and below it, still unfiltered
    e.make("Photo/alpha/x.bin", "x");
    PB_CHECK_EQ(names_of(bridge->listDirectory(QString::fromStdString(e.files.file("Photo/alpha")))).join(QLatin1Char('|')).toStdString(), std::string("Inner|x.bin"));
}

PB_TEST(without_top_folders_the_root_is_listed_unfiltered) {
    Env e;   // its AttachmentsBridge has no topFolders
    ::mkdir(e.files.file("Anything").c_str(), 0755);
    e.make("a.txt", "x");
    PB_CHECK_EQ(names_of(e.attachments->listDirectory(QString::fromStdString(e.files.path()))).join(QLatin1Char('|')).toStdString(), std::string("Anything|a.txt"));
}

PB_TEST(a_root_with_a_trailing_slash_or_an_unclean_path_is_still_the_root) {
    Env e;
    ::mkdir(e.files.file("Photo").c_str(), 0755);
    e.make("install.md", "x");
    auto with_slash = seven_folder_bridge(e, e.files.path() + "/");
    PB_CHECK_EQ(names_of(with_slash->listDirectory(QString::fromStdString(e.files.path()))).join(QLatin1Char('|')).toStdString(), std::string("Photo"));
    auto plain = seven_folder_bridge(e, e.files.path());
    PB_CHECK_EQ(names_of(plain->listDirectory(QString::fromStdString(e.files.path() + "/"))).join(QLatin1Char('|')).toStdString(), std::string("Photo"));
    PB_CHECK_EQ(names_of(plain->listDirectory(QString::fromStdString(e.files.path() + "/Photo/.."))).join(QLatin1Char('|')).toStdString(), std::string("Photo"));
}

// ---- spec 009 US2: the layout choice of the file browser ----------------------------------------------------------------

PB_TEST(the_picker_view_is_list_on_a_fresh_install_and_remembers_a_change) {
    Env e;
    int announced = 0;
    QObject::connect(e.attachments.get(), &AttachmentsBridge::pickerViewChanged, [&] { ++announced; });
    PB_CHECK_EQ(e.attachments->property("pickerView").toString().toStdString(), std::string("list"));   // also without a server session
    PB_CHECK(e.attachments->setProperty("pickerView", QStringLiteral("tile")));
    PB_CHECK_EQ(e.attachments->property("pickerView").toString().toStdString(), std::string("tile"));
    PB_CHECK_EQ(announced, 1);

    e.attachments->setProperty("pickerView", QStringLiteral("tile"));   // the same value again: nothing is announced
    PB_CHECK_EQ(announced, 1);

    e.attachments->setProperty("pickerView", QStringLiteral("grid"));   // not a layout: ignored
    PB_CHECK_EQ(e.attachments->property("pickerView").toString().toStdString(), std::string("tile"));
    PB_CHECK_EQ(announced, 1);

    // a new bridge pair on the same data folder (an app restart) reads the stored choice
    e.attachments.reset();
    e.notes.reset();
    NotesBridge::Config c;
    c.dataDir = QString::fromStdString(e.data.path());
    c.bundledCaPath = QString::fromStdString(std::string(PBNOTES_SOURCE_DIR) + "/third_party/cacert/cacert.pem");
    e.notes.reset(new NotesBridge(c));
    AttachmentsBridge::Config ac;
    ac.rootDirectory = QString::fromStdString(e.files.path());
    e.attachments.reset(new AttachmentsBridge(e.notes.get(), ac));
    PB_CHECK_EQ(e.attachments->property("pickerView").toString().toStdString(), std::string("tile"));
    PB_CHECK(e.attachments->setProperty("pickerView", QStringLiteral("list")));
    PB_CHECK_EQ(e.attachments->property("pickerView").toString().toStdString(), std::string("list"));
}

// ---- spec 009 US3: which entries get a preview, and the ids the tiles use ---------------------------------------------------

namespace {

QVariantMap entry_named(const QVariantList& list, const char* name) {
    for (const QVariant& v : list)
        if (s(v.toMap(), "name") == QString::fromLatin1(name)) return v.toMap();
    return QVariantMap();
}

}  // namespace

PB_TEST(only_regular_readable_image_files_are_marked_as_images_and_given_a_preview_id) {
    Env e;
    e.make("a.JPG", "x");
    e.make("b.png", "x");
    e.make("c.txt", "x");
    e.make("d.webp", "x");
    e.make("noextension", "x");
    ::mkdir(e.files.file("sub").c_str(), 0755);
    ::mkdir(e.files.file("folder.jpg").c_str(), 0755);   // a folder named like an image
    PB_CHECK(::mkfifo(e.files.file("f.jpg").c_str(), 0644) == 0);   // not a regular file: never previewed
    e.make("locked.jpg", "x");
    ::chmod(e.files.file("locked.jpg").c_str(), 0);
    const bool locked_is_hidden = ::access(e.files.file("locked.jpg").c_str(), R_OK) != 0;

    const QVariantList list = e.attachments->listDirectory(QString::fromStdString(e.files.path()));
    for (const char* name : {"a.JPG", "b.png", "d.webp"}) {
        PB_CHECK(entry_named(list, name).value("image").toBool());
        PB_CHECK(entry_named(list, name).value("previewId").toInt() > 0);
    }
    for (const char* name : {"c.txt", "noextension", "sub", "folder.jpg", "f.jpg"}) {
        const QVariantMap m = entry_named(list, name);
        if (m.isEmpty()) continue;   // a FIFO may be left out by the listing; either way it has no id
        PB_CHECK(!m.value("image").toBool());
        PB_CHECK_EQ(m.value("previewId").toInt(), 0);
    }
    if (locked_is_hidden) PB_CHECK(entry_named(list, "locked.jpg").isEmpty());   // unreadable entries are skipped as before

    QSet<int> ids;
    for (const char* name : {"a.JPG", "b.png", "d.webp"}) ids.insert(entry_named(list, name).value("previewId").toInt());
    PB_CHECK_EQ(ids.size(), 3);   // pairwise different

    const auto index = e.attachments->previewIndex();
    PB_CHECK(index != nullptr);
    PB_CHECK_EQ(index->path(entry_named(list, "a.JPG").value("previewId").toInt()).toStdString(), e.files.file("a.JPG"));
    PB_CHECK_EQ(index->path(entry_named(list, "b.png").value("previewId").toInt()).toStdString(), e.files.file("b.png"));
    PB_CHECK(index->path(0).isEmpty());
    PB_CHECK(index->path(99999).isEmpty());
    ::chmod(e.files.file("locked.jpg").c_str(), 0644);
}

PB_TEST(a_new_listing_replaces_the_preview_index_and_ids_are_never_reused) {
    Env e;
    e.make("one.jpg", "x");
    ::mkdir(e.files.file("other").c_str(), 0755);
    e.make("other/two.png", "x");
    const auto index = e.attachments->previewIndex();
    const QVariantList first = e.attachments->listDirectory(QString::fromStdString(e.files.path()));
    const int first_id = entry_named(first, "one.jpg").value("previewId").toInt();
    PB_CHECK(first_id > 0);
    PB_CHECK(!index->path(first_id).isEmpty());

    const QVariantList second = e.attachments->listDirectory(QString::fromStdString(e.files.file("other")));
    const int second_id = entry_named(second, "two.png").value("previewId").toInt();
    PB_CHECK(second_id > first_id);              // the counter only grows
    PB_CHECK(index->path(first_id).isEmpty());   // an id of the earlier listing no longer resolves
    PB_CHECK_EQ(index->path(second_id).toStdString(), e.files.file("other/two.png"));

    const QVariantList again = e.attachments->listDirectory(QString::fromStdString(e.files.path()));
    PB_CHECK(entry_named(again, "one.jpg").value("previewId").toInt() > second_id);   // listing the same folder again: new ids
    PB_CHECK(index->path(second_id).isEmpty());

    PB_CHECK(e.attachments->listDirectory(QString::fromStdString(e.files.file("missing"))).isEmpty());
    PB_CHECK(index->path(entry_named(again, "one.jpg").value("previewId").toInt()).isEmpty());   // nothing listed: nothing resolves
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    return pbtest::run_all();
}
