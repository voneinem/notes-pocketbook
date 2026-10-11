// Reader block model (contracts/qml-bridge.md): link markup and the `links` list (US1), table rows (US2),
// attachment blocks (US4/US5). Pure conversion tests, no network.
#include <cmath>
#include <string>

#include <QCoreApplication>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <memory>

#include "pbnotes/attachment_ref.h"
#include "pbnotes/note_store.h"
#include "pbnotes/types.h"
#include "reader_blocks.h"
#include "test_support.h"
#include "tmp_dir.h"

namespace {

pbnotes::Note note_with(const std::string& content) {
    pbnotes::Note n;
    n.content = content;
    return n;
}

QVariantMap block(const QVariantList& blocks, int i) { return blocks.at(i).toMap(); }

QVariantList text_blocks(const QVariantList& all) {
    QVariantList out;
    for (const QVariant& v : all)
        if (v.toMap().value("kind").toString() != QLatin1String("blank")) out.append(v);
    return out;
}

}  // namespace

// ---- US1: links -------------------------------------------------------------------------------

PB_TEST(a_link_becomes_an_anchor_with_an_index_into_links) {
    const QVariantList b = text_blocks(reader_blocks(note_with("see [Read this](https://example.org/a?b=1) now")));
    PB_CHECK_EQ(b.size(), 1);
    const QVariantMap m = block(b, 0);
    PB_CHECK_EQ(m.value("html").toString().toStdString(),
                std::string("see <a href=\"l0\"><u>Read this</u></a> now"));
    const QVariantList links = m.value("links").toList();
    PB_CHECK_EQ(links.size(), 1);
    PB_CHECK_EQ(links.at(0).toMap().value("url").toString().toStdString(), std::string("https://example.org/a?b=1"));
    PB_CHECK(links.at(0).toMap().value("confirm").toBool());
}

PB_TEST(a_bare_address_needs_no_confirmation_and_links_are_numbered_in_order) {
    const QVariantList b = text_blocks(reader_blocks(note_with("https://one.example and [two](https://two.example)")));
    const QVariantMap m = block(b, 0);
    const QString html = m.value("html").toString();
    PB_CHECK(html.contains(QStringLiteral("<a href=\"l0\"><u>https://one.example</u></a>")));
    PB_CHECK(html.contains(QStringLiteral("<a href=\"l1\"><u>two</u></a>")));
    const QVariantList links = m.value("links").toList();
    PB_CHECK_EQ(links.size(), 2);
    PB_CHECK(!links.at(0).toMap().value("confirm").toBool());
    PB_CHECK(links.at(1).toMap().value("confirm").toBool());
}

PB_TEST(the_raw_address_never_appears_in_the_markup_of_a_titled_link) {
    const QVariantList b = text_blocks(reader_blocks(note_with("[Click](https://secret.example/path?token=1)")));
    PB_CHECK(!block(b, 0).value("html").toString().contains(QStringLiteral("secret.example")));
}

PB_TEST(special_characters_in_link_text_and_address_are_escaped) {
    const QVariantList b = text_blocks(reader_blocks(note_with("[a <b> & \"c\"](https://x.example/?a=1&b=<2>)")));
    const QVariantMap m = block(b, 0);
    const QString html = m.value("html").toString();
    PB_CHECK(html.contains(QStringLiteral("a &lt;b&gt; &amp; &quot;c&quot;")));
    PB_CHECK(!html.contains(QStringLiteral("<b>")));
    // the address is kept as written in the links list (data, not markup)
    PB_CHECK_EQ(m.value("links").toList().at(0).toMap().value("url").toString().toStdString(),
                std::string("https://x.example/?a=1&b=<2>"));
}

PB_TEST(a_link_that_cannot_be_followed_shows_only_its_label) {
    const QVariantList b = text_blocks(reader_blocks(note_with("[write](mailto:someone@example.org)")));
    const QVariantMap m = block(b, 0);
    PB_CHECK_EQ(m.value("html").toString().toStdString(), std::string("write"));
    PB_CHECK_EQ(m.value("links").toList().size(), 0);
}

PB_TEST(text_blocks_without_links_have_an_empty_links_list) {
    const QVariantList b = text_blocks(reader_blocks(note_with("# Title\n- item\nplain")));
    for (const QVariant& v : b) PB_CHECK_EQ(v.toMap().value("links").toList().size(), 0);
}

PB_TEST(emphasis_and_links_combine) {
    const QVariantList b = text_blocks(reader_blocks(note_with("[a **bold** b](https://x.example)")));
    const QVariantMap m = block(b, 0);
    PB_CHECK_EQ(m.value("html").toString().toStdString(),
                std::string("<a href=\"l0\"><u>a <b>bold</b> b</u></a>"));
    PB_CHECK_EQ(m.value("links").toList().size(), 1);
}

// ---- US2: tables ------------------------------------------------------------------------------

PB_TEST(a_table_becomes_one_block_per_row) {
    const QVariantList b = text_blocks(reader_blocks(note_with(
        "before\n| Item | Qty |\n|:--|--:|\n| Tea | 2 |\n| Rice | 10 |\nafter")));
    PB_CHECK_EQ(b.size(), 5);  // before, 3 rows, after
    PB_CHECK_EQ(block(b, 0).value("kind").toString().toStdString(), std::string("paragraph"));
    for (int i = 1; i <= 3; ++i) {
        const QVariantMap m = block(b, i);
        PB_CHECK_EQ(m.value("kind").toString().toStdString(), std::string("tablerow"));
        PB_CHECK(m.value("unsplittable").toBool());
        PB_CHECK_EQ(m.value("cols").toList().size(), 2);
        PB_CHECK_EQ(m.value("weights").toList().size(), 2);
    }
    PB_CHECK(block(b, 1).value("header").toBool());
    PB_CHECK(!block(b, 2).value("header").toBool());
    PB_CHECK(!block(b, 3).value("header").toBool());
    PB_CHECK(!block(b, 1).value("lastRow").toBool());
    PB_CHECK(!block(b, 2).value("lastRow").toBool());
    PB_CHECK(block(b, 3).value("lastRow").toBool());
    PB_CHECK_EQ(block(b, 4).value("kind").toString().toStdString(), std::string("paragraph"));
}

PB_TEST(table_columns_carry_html_links_and_alignment) {
    const QVariantList b = text_blocks(reader_blocks(note_with(
        "| a | b |\n|:--|--:|\n| **x** | [shop](https://x.example) |")));
    const QVariantMap header = block(b, 0), row = block(b, 1);
    const QVariantList hc = header.value("cols").toList(), rc = row.value("cols").toList();
    PB_CHECK_EQ(hc.at(0).toMap().value("align").toString().toStdString(), std::string("left"));
    PB_CHECK_EQ(hc.at(1).toMap().value("align").toString().toStdString(), std::string("right"));
    PB_CHECK_EQ(rc.at(0).toMap().value("html").toString().toStdString(), std::string("<b>x</b>"));
    PB_CHECK_EQ(rc.at(1).toMap().value("html").toString().toStdString(),
                std::string("<a href=\"l0\"><u>shop</u></a>"));
    const QVariantList links = rc.at(1).toMap().value("links").toList();
    PB_CHECK_EQ(links.size(), 1);
    PB_CHECK_EQ(links.at(0).toMap().value("url").toString().toStdString(), std::string("https://x.example"));
    PB_CHECK_EQ(rc.at(0).toMap().value("links").toList().size(), 0);
}

PB_TEST(column_weights_are_the_same_for_every_row_and_sum_to_one) {
    const QVariantList b = text_blocks(reader_blocks(note_with("| a | b |\n|---|---|\n| long long long cell | 1 |")));
    const QVariantList w1 = block(b, 0).value("weights").toList(), w2 = block(b, 1).value("weights").toList();
    PB_CHECK_EQ(w1.size(), 2);
    PB_CHECK_EQ(w1.at(0).toDouble(), w2.at(0).toDouble());
    PB_CHECK(std::abs(w1.at(0).toDouble() + w1.at(1).toDouble() - 1.0) < 1e-9);
}

// ---- US4: attachments ---------------------------------------------------------------------------------

namespace {

struct StoreEnv {
    pbtest::TmpDir dir;
    std::unique_ptr<pbnotes::NoteStore> store;
    int64_t note = 0;
    explicit StoreEnv(const std::string& content) {
        store = pbnotes::NoteStore::open(dir.file("db"));
        pbnotes::ApiNote n;
        n.id = 1;
        n.etag = "e1";
        n.content = content;
        n.modified = 100;
        store->apply_remote({n});
        note = store->get_by_remote_id(1)->local_id;
    }
    QVariantList blocks() const { return text_blocks(reader_blocks(*store->get(note), store.get())); }
};

QString str(const QVariantMap& m, const char* key) { return m.value(QString::fromLatin1(key)).toString(); }

}  // namespace

PB_TEST(an_attachment_without_a_row_is_a_placeholder_with_its_name) {
    StoreEnv e("Note\n![my photo](.attachments.1/photo%20one.png)");
    const QVariantList b = e.blocks();
    PB_CHECK_EQ(b.size(), 2);
    const QVariantMap m = block(b, 1);
    PB_CHECK_EQ(str(m, "kind").toStdString(), std::string("attachment"));
    PB_CHECK_EQ(str(m, "attState").toStdString(), std::string("placeholder"));
    PB_CHECK_EQ(str(m, "name").toStdString(), std::string("my photo"));
    PB_CHECK_EQ(str(m, "attKind").toStdString(), std::string("image"));
    PB_CHECK_EQ(m.value("size").toLongLong(), static_cast<qlonglong>(-1));
    PB_CHECK_EQ(m.value("attachmentId").toLongLong(), static_cast<qlonglong>(0));
    PB_CHECK_EQ(str(m, "remotePath").toStdString(), std::string(".attachments.1/photo one.png"));
    PB_CHECK(str(m, "imageSource").isEmpty());
    PB_CHECK(!m.value("viewable").toBool());
    PB_CHECK(str(m, "errorText").isEmpty());
    PB_CHECK(m.value("unsplittable").toBool());
}

PB_TEST(the_name_falls_back_to_the_file_name_of_the_path) {
    StoreEnv e("Note\n![](.attachments.1/report%20final.pdf)");
    const QVariantMap m = block(e.blocks(), 1);
    PB_CHECK_EQ(str(m, "name").toStdString(), std::string("report final.pdf"));
    PB_CHECK_EQ(str(m, "attKind").toStdString(), std::string("file"));
}

PB_TEST(a_known_size_is_shown_on_the_placeholder) {
    StoreEnv e("Note\n![](.attachments.1/a.png)");
    PB_CHECK(e.store->record_size(e.note, ".attachments.1/a.png", 123456));
    const QVariantMap m = block(e.blocks(), 1);
    PB_CHECK_EQ(str(m, "attState").toStdString(), std::string("placeholder"));
    PB_CHECK_EQ(m.value("size").toLongLong(), static_cast<qlonglong>(123456));
}

PB_TEST(a_downloaded_image_is_ready_and_has_an_image_source) {
    StoreEnv e("Note\n![](.attachments.1/a.png)");
    PB_CHECK(e.store->record_downloaded(e.note, ".attachments.1/a.png", 2048, "1.png"));
    const int64_t id = e.store->attachment_for(e.note, ".attachments.1/a.png")->id;
    const QVariantMap m = block(e.blocks(), 1);
    PB_CHECK_EQ(str(m, "attState").toStdString(), std::string("ready"));
    PB_CHECK_EQ(m.value("attachmentId").toLongLong(), static_cast<qlonglong>(id));
    PB_CHECK_EQ(str(m, "imageSource").toStdString(), "image://attachment/" + std::to_string(id));
    PB_CHECK(!m.value("viewable").toBool());   // an image is shown in place, not handed to a viewer
    PB_CHECK_EQ(m.value("size").toLongLong(), static_cast<qlonglong>(2048));
}

PB_TEST(a_downloaded_file_is_viewable_only_for_types_the_reader_opens) {
    StoreEnv e("Note\n[a](.attachments.1/a.pdf)\n[b](.attachments.1/b.xyz)\n[c](.attachments.1/c)");
    PB_CHECK(e.store->record_downloaded(e.note, ".attachments.1/a.pdf", 10, "1.pdf"));
    PB_CHECK(e.store->record_downloaded(e.note, ".attachments.1/b.xyz", 10, "2.xyz"));
    PB_CHECK(e.store->record_downloaded(e.note, ".attachments.1/c", 10, "3"));
    const QVariantList b = e.blocks();
    PB_CHECK(block(b, 1).value("viewable").toBool());
    PB_CHECK(!block(b, 2).value("viewable").toBool());
    PB_CHECK(!block(b, 3).value("viewable").toBool());
    PB_CHECK(str(block(b, 1), "imageSource").isEmpty());
}

PB_TEST(a_failed_attachment_shows_a_plain_sentence_without_names_or_paths) {
    StoreEnv e("Note\n![](.attachments.1/secret-name.pdf)");
    for (const char* key : {"too_large", "not_found", "rejected", "unsupported_server", "storage_full", "unreadable",
                            "format_unsupported", "network"}) {
        PB_CHECK(e.store->set_attachment_error(0, e.note, ".attachments.1/secret-name.pdf", key));
        const QVariantMap m = block(e.blocks(), 1);
        PB_CHECK_EQ(str(m, "attState").toStdString(), std::string("error"));
        const QString text = str(m, "errorText");
        PB_CHECK(!text.isEmpty());
        PB_CHECK(text.endsWith(QLatin1Char('.')));
        PB_CHECK(!text.contains(QStringLiteral("secret")));
        PB_CHECK(!text.contains(QStringLiteral(".pdf")));
    }
}

PB_TEST(without_a_store_attachments_are_plain_placeholders) {
    const QVariantList b = text_blocks(reader_blocks(note_with("![x](.attachments.1/a.png)")));
    PB_CHECK_EQ(str(block(b, 0), "attState").toStdString(), std::string("placeholder"));
}

PB_TEST(sizes_are_formatted_for_people) {
    PB_CHECK_EQ(format_size(0).toStdString(), std::string("0 B"));
    PB_CHECK_EQ(format_size(1023).toStdString(), std::string("1023 B"));
    PB_CHECK_EQ(format_size(1536).toStdString(), std::string("1.5 KB"));
    PB_CHECK_EQ(format_size(1048576).toStdString(), std::string("1.0 MB"));
    PB_CHECK_EQ(format_size(1500000).toStdString(), std::string("1.4 MB"));
    PB_CHECK_EQ(format_size(10485760).toStdString(), std::string("10.0 MB"));
    PB_CHECK_EQ(format_size(-1).toStdString(), std::string());   // unknown
}

// ---- spec 005: where each entry comes from ---------------------------------------------------------------------------

PB_TEST(every_entry_says_which_parsed_block_and_row_it_is) {
    const QVariantList all = reader_blocks(note_with("# T\n\ntext\n| a | b |\n|---|---|\n| 1 | 2 |\n![x](.attachments.1/y.png)"));
    // heading, blank, paragraph, two table rows (one block), attachment
    PB_CHECK_EQ(all.size(), 6);
    PB_CHECK_EQ(all.at(0).toMap().value("block").toInt(), 0);
    PB_CHECK_EQ(all.at(1).toMap().value("block").toInt(), 1);
    PB_CHECK_EQ(all.at(2).toMap().value("block").toInt(), 2);
    PB_CHECK_EQ(all.at(3).toMap().value("block").toInt(), 3);
    PB_CHECK_EQ(all.at(4).toMap().value("block").toInt(), 3);   // the same table block ...
    PB_CHECK_EQ(all.at(3).toMap().value("row").toInt(), 0);     // ... row 0 (the header)
    PB_CHECK_EQ(all.at(4).toMap().value("row").toInt(), 1);
    PB_CHECK_EQ(all.at(5).toMap().value("block").toInt(), 4);
    PB_CHECK_EQ(all.at(5).toMap().value("row").toInt(), 0);
    // text entries tell their extent in the source line
    const QVariantMap heading = all.at(0).toMap().value("src").toMap();
    PB_CHECK_EQ(heading.value("begin").toInt(), 0);
    PB_CHECK_EQ(heading.value("end").toInt(), 3);
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    return pbtest::run_all();
}
