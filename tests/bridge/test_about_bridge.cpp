// About box (spec 003, contracts/qml-bridge.md "AboutBridge"): the read-only data of the About screens.
// Uses a fixture licenses root in a temporary directory; no network, nothing is written.
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QDir>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include "about_bridge.h"
#include "test_support.h"
#include "tmp_dir.h"

namespace {

const char* kApp =
    "id: pbnotes\nkind: app\nname: Notes Pocketbook\nlicense: MIT License\nholder: Jane Author\n"
    "copyright: Copyright (c) 2026 Jane Author\ntext: LICENSE\n";

// All files below `dir`, relative, sorted: proves nothing was written while the bridge was used.
QStringList listing(const std::string& dir) {
    QStringList out;
    QDir d(QString::fromStdString(dir));
    for (const QString& f : d.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name)) {
        out << f;
        if (QFileInfo(d.filePath(f)).isDir()) {
            for (const QString& g : listing(d.filePath(f).toStdString())) out << f + "/" + g;
        }
    }
    return out;
}

struct Env {
    pbtest::TmpDir root;
    explicit Env(const std::string& manifest, bool write_manifest = true) {
        ::mkdir(root.file("licenses").c_str(), 0755);
        if (write_manifest) root.write("licenses/manifest.txt", manifest);
        root.write("LICENSE", "MIT License\n\nCopyright (c) 2026 Jane Author\n");
    }
    AboutBridge::Config config(const char* version = "1.2.3") const {
        AboutBridge::Config c;
        c.licensesRoot = QString::fromStdString(root.path());
        c.version = QString::fromUtf8(version);
        return c;
    }
};

}  // namespace

PB_TEST(good_manifest_gives_the_app_information) {
    Env e(kApp);
    AboutBridge b(e.config());
    PB_CHECK(b.infoAvailable());
    PB_CHECK_EQ(b.appName().toStdString(), std::string("Notes Pocketbook"));
    PB_CHECK_EQ(b.version().toStdString(), std::string("1.2.3"));
    PB_CHECK_EQ(b.author().toStdString(), std::string("Jane Author"));
    PB_CHECK_EQ(b.copyright().toStdString(), std::string("Copyright (c) 2026 Jane Author"));
    PB_CHECK_EQ(b.licenseName().toStdString(), std::string("MIT License"));
}

PB_TEST(fallback_version_shows_as_unknown) {
    Env e(kApp);
    PB_CHECK_EQ(AboutBridge(e.config("0.0.0")).version().toStdString(), std::string("unknown"));
    PB_CHECK_EQ(AboutBridge(e.config("")).version().toStdString(), std::string("unknown"));
}

PB_TEST(missing_manifest_keeps_name_and_version_and_empties_the_rest) {
    Env e("", false);
    AboutBridge b(e.config());
    PB_CHECK(!b.infoAvailable());
    PB_CHECK(!b.appName().isEmpty());
    PB_CHECK_EQ(b.version().toStdString(), std::string("1.2.3"));
    PB_CHECK(b.author().isEmpty());
    PB_CHECK(b.copyright().isEmpty());
    PB_CHECK(b.licenseName().isEmpty());
}

PB_TEST(manifest_without_an_app_block_is_not_available) {
    Env e("id: sqlite\nkind: component\nname: SQLite\npurpose: p\nlicense: PD\nholder: h\n"
          "url: https://example.org\norigin: bundled\n");
    AboutBridge b(e.config());
    PB_CHECK(!b.infoAvailable());
    PB_CHECK(!b.appName().isEmpty());
    PB_CHECK(b.author().isEmpty());
}

PB_TEST(reading_every_property_changes_nothing_on_disk) {
    Env e(kApp);
    const QStringList before = listing(e.root.path());
    {
        AboutBridge b(e.config());
        (void)b.appName(); (void)b.version(); (void)b.author(); (void)b.copyright();
        (void)b.licenseName(); (void)b.infoAvailable();
    }
    PB_CHECK(listing(e.root.path()) == before);
}

PB_TEST(app_license_gives_title_header_and_paragraphs) {
    Env e(kApp);
    AboutBridge b(e.config());
    const QVariantMap l = b.appLicense();
    PB_CHECK(l.value("available").toBool());
    PB_CHECK(!l.value("title").toString().isEmpty());
    const QStringList header = l.value("header").toStringList();
    PB_CHECK(header.contains(QStringLiteral("Holder: Jane Author")));
    PB_CHECK(header.contains(QStringLiteral("License: MIT License")));
    const QStringList paragraphs = l.value("paragraphs").toStringList();
    PB_CHECK_EQ(paragraphs.size(), 2);
    PB_CHECK_EQ(paragraphs[0].toStdString(), std::string("MIT License"));
}

PB_TEST(app_license_with_a_missing_text_file_says_so) {
    Env e(kApp);
    ::unlink(e.root.file("LICENSE").c_str());
    const QVariantMap l = AboutBridge(e.config()).appLicense();
    PB_CHECK(!l.value("available").toBool());
    PB_CHECK_EQ(l.value("paragraphs").toStringList().size(), 1);
    PB_CHECK_EQ(l.value("paragraphs").toStringList()[0].toStdString(), std::string("The license text could not be read."));
    PB_CHECK(!l.value("header").toStringList().isEmpty());  // the facts are still shown
}

PB_TEST(app_license_without_a_manifest_is_not_available) {
    Env e("", false);
    const QVariantMap l = AboutBridge(e.config()).appLicense();
    PB_CHECK(!l.value("available").toBool());
    PB_CHECK_EQ(l.value("paragraphs").toStringList().size(), 1);
}

PB_TEST(reading_the_license_changes_nothing_on_disk) {
    Env e(kApp);
    const QStringList before = listing(e.root.path());
    { AboutBridge b(e.config()); (void)b.appLicense(); }
    PB_CHECK(listing(e.root.path()) == before);
}

// ---- the third-party list ------------------------------------------------------------------------------

namespace {

std::string component(const std::string& id, const std::string& name, const std::string& origin, const std::string& text = "") {
    return "\nid: " + id + "\nkind: component\nname: " + name + "\npurpose: Purpose of " + name + "\nlicense: License of " + name +
           "\nholder: Holder of " + name + "\nurl: https://example.org/" + id + "\norigin: " + origin + "\n" +
           (text.empty() ? "" : "text: " + text + "\n");
}

QString str(const QVariant& v, const char* key) { return v.toMap().value(QLatin1String(key)).toString(); }

}  // namespace

PB_TEST(components_list_valid_entries_bundled_first_then_system_in_manifest_order) {
    Env e(std::string(kApp) + component("qt", "Qt", "system") + component("sqlite", "SQLite", "bundled") +
          component("curl", "libcurl", "system") + component("font", "Emoji font", "bundled"));
    AboutBridge b(e.config());
    const QVariantList list = b.components();
    PB_CHECK_EQ(list.size(), 4);
    PB_CHECK_EQ(str(list[0], "id").toStdString(), std::string("sqlite"));
    PB_CHECK_EQ(str(list[1], "id").toStdString(), std::string("font"));
    PB_CHECK_EQ(str(list[2], "id").toStdString(), std::string("qt"));
    PB_CHECK_EQ(str(list[3], "id").toStdString(), std::string("curl"));
    PB_CHECK_EQ(str(list[0], "origin").toStdString(), std::string("bundled"));
    PB_CHECK_EQ(str(list[2], "origin").toStdString(), std::string("system"));
    PB_CHECK_EQ(str(list[0], "name").toStdString(), std::string("SQLite"));
    PB_CHECK_EQ(str(list[0], "purpose").toStdString(), std::string("Purpose of SQLite"));
    PB_CHECK_EQ(str(list[0], "license").toStdString(), std::string("License of SQLite"));
    PB_CHECK_EQ(b.unreadableEntries(), 0);
}

PB_TEST(bad_blocks_are_counted_not_listed) {
    Env e(std::string(kApp) + component("qt", "Qt", "system") + "\nid: broken\nkind: component\nname: Broken\n" +
          component("sqlite", "SQLite", "bundled"));
    AboutBridge b(e.config());
    PB_CHECK_EQ(b.components().size(), 2);
    PB_CHECK_EQ(b.unreadableEntries(), 1);
}

PB_TEST(without_a_manifest_the_list_is_empty) {
    Env e("", false);
    AboutBridge b(e.config());
    PB_CHECK(b.components().isEmpty());
    PB_CHECK_EQ(b.unreadableEntries(), 0);
}

PB_TEST(detail_of_a_component_with_text_has_header_lines_and_paragraphs) {
    Env e(std::string(kApp) + component("qt", "Qt", "system", "licenses/qt.txt"));
    ::mkdir(e.root.file("licenses").c_str(), 0755);
    e.root.write("licenses/qt.txt", "Qt license\n\nsecond paragraph\n");
    const QVariantMap d = AboutBridge(e.config()).componentDetail(QStringLiteral("qt"));
    PB_CHECK(d.value("available").toBool());
    PB_CHECK_EQ(d.value("title").toString().toStdString(), std::string("Qt"));
    const QStringList header = d.value("header").toStringList();
    PB_CHECK(header.contains(QStringLiteral("Holder: Holder of Qt")));
    PB_CHECK(header.contains(QStringLiteral("License: License of Qt")));
    PB_CHECK(header.contains(QStringLiteral("Origin: Provided by the reader")));
    PB_CHECK(header.contains(QStringLiteral("Address: https://example.org/qt")));
    PB_CHECK(header.contains(QStringLiteral("Purpose: Purpose of Qt")));
    PB_CHECK_EQ(d.value("paragraphs").toStringList().size(), 2);
}

PB_TEST(detail_origin_line_names_bundled_components) {
    Env e(std::string(kApp) + component("sqlite", "SQLite", "bundled"));
    const QVariantMap d = AboutBridge(e.config()).componentDetail(QStringLiteral("sqlite"));
    PB_CHECK(d.value("header").toStringList().contains(QStringLiteral("Origin: In the app package")));
}

PB_TEST(detail_of_a_component_without_text_points_to_the_address) {
    Env e(std::string(kApp) + component("sqlite", "SQLite", "bundled"));
    const QVariantMap d = AboutBridge(e.config()).componentDetail(QStringLiteral("sqlite"));
    PB_CHECK(d.value("available").toBool());
    const QStringList p = d.value("paragraphs").toStringList();
    PB_CHECK_EQ(p.size(), 1);
    PB_CHECK_EQ(p[0].toStdString(), std::string("The full license text is published at https://example.org/sqlite."));
}

PB_TEST(detail_with_all_text_files_missing_keeps_the_header_and_says_so) {
    Env e(std::string(kApp) + component("qt", "Qt", "system", "licenses/missing.txt"));
    const QVariantMap d = AboutBridge(e.config()).componentDetail(QStringLiteral("qt"));
    PB_CHECK(!d.value("available").toBool());
    PB_CHECK(!d.value("header").toStringList().isEmpty());
    const QStringList p = d.value("paragraphs").toStringList();
    PB_CHECK_EQ(p.size(), 1);
    PB_CHECK_EQ(p[0].toStdString(), std::string("The license text could not be read."));
}

PB_TEST(detail_of_an_unknown_id_is_empty) {
    Env e(std::string(kApp) + component("qt", "Qt", "system"));
    PB_CHECK(AboutBridge(e.config()).componentDetail(QStringLiteral("nope")).isEmpty());
    PB_CHECK(AboutBridge(e.config()).componentDetail(QString()).isEmpty());
}

PB_TEST(using_the_list_and_details_changes_nothing_on_disk) {
    Env e(std::string(kApp) + component("qt", "Qt", "system", "licenses/qt.txt") + component("sqlite", "SQLite", "bundled"));
    ::mkdir(e.root.file("licenses").c_str(), 0755);
    e.root.write("licenses/qt.txt", "Qt license\n");
    const QStringList before = listing(e.root.path());
    {
        AboutBridge b(e.config());
        (void)b.components(); (void)b.unreadableEntries();
        (void)b.componentDetail(QStringLiteral("qt")); (void)b.componentDetail(QStringLiteral("sqlite"));
        (void)b.componentDetail(QStringLiteral("nope"));
    }
    PB_CHECK(listing(e.root.path()) == before);
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    return pbtest::run_all();
}
