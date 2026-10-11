// Spec 005 (contracts/qml-bridge.md): the clipboard bridge and the edit session. Plain QCoreApplication, so the
// clipboard bridge uses its in-app buffer; the platform clipboard is checked on the reader (spike S-3).
#include <cstdint>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include "edit_bridge.h"
#include "test_support.h"

namespace {
// Counts a bridge's `changed` signals (QtTest is not linked).
struct Counter {
    int n = 0;
    template <typename T>
    explicit Counter(T* o) { QObject::connect(o, &T::changed, [this] { ++n; }); }
    int count() const { return n; }
};
}  // namespace

// ---- ClipboardBridge ------------------------------------------------------------------------------------------

PB_TEST(a_new_clipboard_has_no_text) {
    ClipboardBridge c;
    PB_CHECK(!c.hasText());
    PB_CHECK_EQ(c.text().toStdString(), std::string());
}

PB_TEST(copy_stores_the_text_and_announces_it) {
    ClipboardBridge c;
    Counter spy(&c);
    c.copy(QStringLiteral("hello"));
    PB_CHECK(c.hasText());
    PB_CHECK_EQ(c.text().toStdString(), std::string("hello"));
    PB_CHECK_EQ(spy.count(), 1);
}

PB_TEST(copy_of_nothing_is_ignored) {
    ClipboardBridge c;
    c.copy(QStringLiteral("keep"));
    Counter spy(&c);
    c.copy(QString());
    c.copy(QString(QChar(0x01)));  // cleans to empty
    PB_CHECK_EQ(spy.count(), 0);
    PB_CHECK_EQ(c.text().toStdString(), std::string("keep"));
}

PB_TEST(copy_normalizes_paragraph_separators_and_drops_controls) {
    ClipboardBridge c;
    c.copy(QStringLiteral("a") + QChar(0x2029) + QStringLiteral("b"));
    PB_CHECK_EQ(c.text().toStdString(), std::string("a\nb"));
    c.copy(QStringLiteral("a\tb\n") + QChar(0x01) + QStringLiteral("c"));
    PB_CHECK_EQ(c.text().toStdString(), std::string("a\tb\nc"));
}

// ---- ClipboardBridge: paste helpers -----------------------------------------------------------------------------

namespace {
QString str(const QVariantMap& m, const char* key) { return m.value(QLatin1String(key)).toString(); }
}  // namespace

PB_TEST(clean_paste_returns_the_cleaned_text) {
    ClipboardBridge c;
    c.copy(QStringLiteral("a\r\nb\x01"));
    const QVariantMap r = c.cleanPaste();
    PB_CHECK(r.value("ok").toBool());
    PB_CHECK_EQ(str(r, "text").toStdString(), std::string("a\nb"));
}

PB_TEST(clean_paste_with_nothing_to_paste_is_a_silent_no_op) {
    ClipboardBridge c;
    const QVariantMap r = c.cleanPaste();
    PB_CHECK(!r.value("ok").toBool());
    PB_CHECK_EQ(str(r, "errorKey").toStdString(), std::string());
    PB_CHECK(str(r, "errorText").isEmpty());
}

PB_TEST(clean_paste_refuses_too_much_text) {
    ClipboardBridge c;
    c.copy(QString(1000001, QLatin1Char('x')));
    const QVariantMap r = c.cleanPaste();
    PB_CHECK(!r.value("ok").toBool());
    PB_CHECK_EQ(str(r, "errorKey").toStdString(), std::string("paste_too_large"));
    PB_CHECK_EQ(str(r, "errorText").toStdString(), std::string("The copied text is too large to paste."));
    c.copy(QString(1000000, QLatin1Char('x')));
    PB_CHECK(c.cleanPaste().value("ok").toBool());
}

PB_TEST(title_paste_replaces_the_selection_with_one_line) {
    ClipboardBridge c;
    c.copy(QStringLiteral("i\nthere"));
    const QVariantMap r = c.titlePaste(QStringLiteral("Hello"), 1, 3);
    PB_CHECK(r.value("ok").toBool());
    PB_CHECK_EQ(str(r, "text").toStdString(), std::string("Hi therelo"));
    PB_CHECK_EQ(r.value("caret").toInt(), 8);
}

PB_TEST(title_paste_without_a_selection_inserts_at_the_caret) {
    ClipboardBridge c;
    c.copy(QStringLiteral("XY"));
    const QVariantMap r = c.titlePaste(QStringLiteral("Hello"), 3, 3);
    PB_CHECK_EQ(str(r, "text").toStdString(), std::string("HelXYlo"));
    PB_CHECK_EQ(r.value("caret").toInt(), 5);
}

PB_TEST(title_paste_turns_tabs_and_breaks_into_single_spaces) {
    ClipboardBridge c;
    c.copy(QStringLiteral("a\tb\n\nc"));
    PB_CHECK_EQ(str(c.titlePaste(QString(), 0, 0), "text").toStdString(), std::string("a b c"));
}

PB_TEST(title_paste_refuses_a_title_over_255_code_points) {
    ClipboardBridge c;
    c.copy(QString(10, QLatin1Char('b')));
    const QVariantMap r = c.titlePaste(QString(250, QLatin1Char('a')), 250, 250);
    PB_CHECK(!r.value("ok").toBool());
    PB_CHECK_EQ(str(r, "errorKey").toStdString(), std::string("title_too_long"));
    PB_CHECK_EQ(str(r, "errorText").toStdString(), std::string("The title is too long (at most 255 characters)."));
    // exactly 255 is fine
    c.copy(QString(5, QLatin1Char('b')));
    PB_CHECK(c.titlePaste(QString(250, QLatin1Char('a')), 250, 250).value("ok").toBool());
}

PB_TEST(title_limit_counts_code_points_not_utf16_units) {
    ClipboardBridge c;
    c.copy(QString(127, QLatin1Char('a')));
    QString emoji;
    for (int i = 0; i < 128; ++i) emoji += QStringLiteral("\U0001F600");  // 256 UTF-16 units, 128 code points
    PB_CHECK(c.titlePaste(emoji, emoji.size(), emoji.size()).value("ok").toBool());
    c.copy(QString(128, QLatin1Char('a')));
    PB_CHECK(!c.titlePaste(emoji, emoji.size(), emoji.size()).value("ok").toBool());
}

PB_TEST(title_paste_clamps_the_selection_and_keeps_surrogate_pairs_whole) {
    ClipboardBridge c;
    c.copy(QStringLiteral("X"));
    // reversed and out-of-range offsets are clamped (the reversed selection covers the whole title)
    PB_CHECK_EQ(str(c.titlePaste(QStringLiteral("abc"), 9, -4), "text").toStdString(), std::string("X"));
    // an offset inside a pair moves to the pair's start (begin) / end (end)
    const QString s = QStringLiteral("a\U0001F600b");  // a, 2 units, b
    const QVariantMap r = c.titlePaste(s, 2, 2);          // inside the pair: inserted before it
    PB_CHECK_EQ(str(r, "text").toStdString(), std::string("aX\xF0\x9F\x98\x80" "b"));
    PB_CHECK_EQ(r.value("caret").toInt(), 2);
    const QVariantMap r2 = c.titlePaste(s, 1, 2);  // end inside the pair: the whole pair is replaced
    PB_CHECK_EQ(str(r2, "text").toStdString(), std::string("aXb"));
}

PB_TEST(title_paste_with_nothing_to_paste_is_a_silent_no_op) {
    ClipboardBridge c;
    const QVariantMap r = c.titlePaste(QStringLiteral("abc"), 1, 2);
    PB_CHECK(!r.value("ok").toBool());
    PB_CHECK(str(r, "errorKey").isEmpty());
}

// ---- EditSession -------------------------------------------------------------------------------------------------

namespace {
struct FakeClock {
    qint64 now = 0;
};
}  // namespace

PB_TEST(a_session_after_reset_has_nothing_to_undo) {
    EditSession e;
    e.reset(QStringLiteral("T"), QStringLiteral("body"), QStringLiteral("Cat"), false);
    PB_CHECK(!e.canUndo());
    PB_CHECK(!e.canRedo());
    PB_CHECK(e.undo().isEmpty());
    PB_CHECK(e.redo().isEmpty());
}

PB_TEST(observe_records_a_step_and_announces_it) {
    EditSession e;
    e.reset(QStringLiteral("T"), QStringLiteral("body"), QStringLiteral("Cat"), false);
    Counter spy(&e);
    e.observe(QStringLiteral("text"), QStringLiteral("bodyX"));
    PB_CHECK(e.canUndo());
    PB_CHECK(spy.count() >= 1);
    const QVariantMap u = e.undo();
    PB_CHECK_EQ(str(u, "field").toStdString(), std::string("text"));
    PB_CHECK_EQ(u.value("pos").toInt(), 4);
    PB_CHECK_EQ(u.value("removeLen").toInt(), 1);
    PB_CHECK_EQ(str(u, "insert").toStdString(), std::string());
    PB_CHECK_EQ(u.value("selBegin").toInt(), 4);
    PB_CHECK_EQ(u.value("selEnd").toInt(), 4);
}

PB_TEST(unknown_field_names_are_ignored) {
    EditSession e;
    e.reset(QStringLiteral("T"), QStringLiteral("body"), QString(), false);
    e.observe(QStringLiteral("nonsense"), QStringLiteral("x"));
    PB_CHECK(!e.canUndo());
}

PB_TEST(category_and_favorite_undo_return_the_previous_value) {
    EditSession e;
    e.reset(QStringLiteral("T"), QStringLiteral("b"), QStringLiteral("Cat"), false);
    e.observe(QStringLiteral("category"), QStringLiteral("Work"));
    e.observe(QStringLiteral("favorite"), QStringLiteral("1"));
    QVariantMap u = e.undo();
    PB_CHECK_EQ(str(u, "field").toStdString(), std::string("favorite"));
    PB_CHECK_EQ(str(u, "value").toStdString(), std::string());
    e.finishApply(QStringLiteral("T"), QStringLiteral("b"));
    u = e.undo();
    PB_CHECK_EQ(str(u, "field").toStdString(), std::string("category"));
    PB_CHECK_EQ(str(u, "value").toStdString(), std::string("Cat"));
    e.finishApply(QStringLiteral("T"), QStringLiteral("b"));
    PB_CHECK(!e.canUndo());
    PB_CHECK(e.canRedo());
}

PB_TEST(a_group_is_one_step_in_the_session) {
    EditSession e;
    e.reset(QStringLiteral("T"), QStringLiteral("hello world"), QString(), false);
    e.beginGroup(QStringLiteral("text"));
    e.observe(QStringLiteral("text"), QStringLiteral("hello "));
    e.observe(QStringLiteral("text"), QStringLiteral("hello there"));
    e.endGroup();
    const QVariantMap u = e.undo();
    PB_CHECK_EQ(u.value("pos").toInt(), 6);
    PB_CHECK_EQ(str(u, "insert").toStdString(), std::string("world"));
    e.finishApply(QStringLiteral("T"), QStringLiteral("hello world"));
    PB_CHECK(!e.canUndo());
}

PB_TEST(positions_are_utf16_units) {
    EditSession e;
    const QString text = QStringLiteral("\U0001F600 ab");  // the emoji is 2 UTF-16 units
    e.reset(QStringLiteral("T"), text, QString(), false);
    e.observe(QStringLiteral("text"), text + QLatin1Char('c'));
    PB_CHECK_EQ(e.undo().value("pos").toInt(), 5);
}

PB_TEST(finish_apply_lets_the_editor_report_again) {
    EditSession e;
    e.reset(QStringLiteral("T"), QStringLiteral("abc"), QString(), false);
    e.observe(QStringLiteral("text"), QStringLiteral("abcd"));
    e.undo();
    e.observe(QStringLiteral("text"), QStringLiteral("ab"));  // ignored while applying
    e.finishApply(QStringLiteral("T"), QStringLiteral("abc"));
    e.observe(QStringLiteral("text"), QStringLiteral("abcX"));
    PB_CHECK(e.canUndo());
    PB_CHECK(!e.canRedo());
}

PB_TEST(the_clock_seam_drives_typing_groups) {
    FakeClock clock;
    EditSession e;
    e.setClockForTest([&clock] { return clock.now; });
    e.reset(QStringLiteral("T"), QString(), QString(), false);
    e.observe(QStringLiteral("text"), QStringLiteral("a"));
    clock.now += 100;
    e.observe(QStringLiteral("text"), QStringLiteral("ab"));  // merged
    clock.now += 2000;
    e.observe(QStringLiteral("text"), QStringLiteral("abc"));  // a pause: a new step
    PB_CHECK_EQ(e.undo().value("removeLen").toInt(), 1);
    e.finishApply(QStringLiteral("T"), QStringLiteral("ab"));
    PB_CHECK_EQ(e.undo().value("removeLen").toInt(), 2);
    e.finishApply(QString(), QString());
    PB_CHECK(!e.canUndo());
}

PB_TEST(reset_clears_the_history) {
    EditSession e;
    e.reset(QStringLiteral("T"), QStringLiteral("a"), QString(), false);
    e.observe(QStringLiteral("text"), QStringLiteral("ab"));
    e.reset(QStringLiteral("T"), QStringLiteral("x"), QString(), false);
    PB_CHECK(!e.canUndo());
    PB_CHECK(!e.canRedo());
}

PB_TEST(undoing_a_replayed_scenario_runs_in_reverse_order) {
    FakeClock clock;
    EditSession e;
    e.setClockForTest([&clock] { return clock.now; });
    e.reset(QStringLiteral("T"), QStringLiteral("body"), QStringLiteral("Cat"), false);
    e.observe(QStringLiteral("category"), QStringLiteral("Work"));
    clock.now += 10;
    e.observe(QStringLiteral("favorite"), QStringLiteral("1"));
    clock.now += 10;
    e.observe(QStringLiteral("text"), QStringLiteral("bodyX"));
    const char* order[] = {"text", "favorite", "category"};
    for (const char* f : order) {
        const QVariantMap u = e.undo();
        PB_CHECK_EQ(str(u, "field").toStdString(), std::string(f));
        e.finishApply(QStringLiteral("T"), QStringLiteral("body"));
    }
    PB_CHECK(!e.canUndo());
    const char* redo[] = {"category", "favorite", "text"};
    for (const char* f : redo) {
        const QVariantMap r = e.redo();
        PB_CHECK_EQ(str(r, "field").toStdString(), std::string(f));
        e.finishApply(QStringLiteral("T"), QStringLiteral("bodyX"));
    }
    PB_CHECK(!e.canRedo());
}

// ---- SlashBridge (spec 006, contracts/qml-bridge.md) ----------------------------------------------------------

PB_TEST(the_slash_bridge_lists_the_twelve_entries_in_order) {
    SlashBridge s;
    const QVariantList e = s.entries();
    PB_CHECK_EQ(e.size(), 12);
    const char* ids[] = {"bullet_list", "table", "info_box", "link", "bold", "italic",
                         "underline", "strikethrough", "details", "quote", "code_block", "emoji"};
    for (int i = 0; i < 12; ++i) {
        const QVariantMap m = e.at(i).toMap();
        PB_CHECK_EQ(m.value("id").toString().toStdString(), std::string(ids[i]));
        PB_CHECK(!m.value("label").toString().isEmpty());
    }
}

PB_TEST(detect_reports_menu_datetime_or_none) {
    SlashBridge s;
    QVariantMap m = s.detect(QStringLiteral(""), QStringLiteral("/"), 1, -1);
    PB_CHECK_EQ(str(m, "trigger").toStdString(), std::string("menu"));
    PB_CHECK_EQ(m.value("pos").toInt(), 0);
    m = s.detect(QStringLiteral("a"), QStringLiteral("a/"), 2, -1);
    PB_CHECK_EQ(str(m, "trigger").toStdString(), std::string("none"));
    m = s.detect(QStringLiteral("a /"), QStringLiteral("a //"), 4, 2);
    PB_CHECK_EQ(str(m, "trigger").toStdString(), std::string("datetime"));
    PB_CHECK_EQ(m.value("pos").toInt(), 3);
}

PB_TEST(expand_returns_the_replacement_and_ignores_unknown_ids_and_emoji) {
    SlashBridge s;
    const QVariantMap m = s.expand(QStringLiteral("code_block"), QStringLiteral("/"), 0, 1);
    PB_CHECK_EQ(m.value("begin").toInt(), 0);
    PB_CHECK_EQ(m.value("end").toInt(), 1);
    PB_CHECK_EQ(str(m, "text").toStdString(), std::string("```\n\n```"));
    PB_CHECK_EQ(m.value("caret").toInt(), 4);
    PB_CHECK(s.expand(QStringLiteral("emoji"), QStringLiteral("/"), 0, 1).isEmpty());
    PB_CHECK(s.expand(QStringLiteral("nope"), QStringLiteral("/"), 0, 1).isEmpty());
    // out-of-range positions never crash and give nothing
    PB_CHECK(s.expand(QStringLiteral("bold"), QStringLiteral("/"), 5, 1).isEmpty());
}

PB_TEST(replace_counts_utf16_units) {
    SlashBridge s;
    const QVariantMap m = s.replace(3, 1, QString::fromUtf8("\xF0\x9F\x99\x82"));
    PB_CHECK_EQ(m.value("begin").toInt(), 3);
    PB_CHECK_EQ(m.value("end").toInt(), 4);
    PB_CHECK_EQ(m.value("caret").toInt(), 5);   // the emoji is two UTF-16 units
}

// The glyphs of a TrueType font, read from its cmap (formats 12 and 4), so no GUI application is needed.
namespace {

uint32_t be(const std::string& d, size_t at, int bytes) {
    uint32_t v = 0;
    for (int i = 0; i < bytes; ++i) v = (v << 8) | static_cast<unsigned char>(d.at(at + static_cast<size_t>(i)));
    return v;
}

bool font_has_glyph(const std::string& font, uint32_t cp) {
    const uint32_t tables = be(font, 4, 2);
    size_t cmap = 0;
    for (uint32_t i = 0; i < tables; ++i)
        if (font.compare(12 + 16 * i, 4, "cmap") == 0) cmap = be(font, 12 + 16 * i + 8, 4);
    if (cmap == 0) return false;
    const uint32_t subtables = be(font, cmap + 2, 2);
    for (uint32_t j = 0; j < subtables; ++j) {
        const size_t sub = cmap + be(font, cmap + 4 + 8 * j + 4, 4);
        const uint32_t format = be(font, sub, 2);
        if (format == 12) {
            const uint32_t groups = be(font, sub + 12, 4);
            for (uint32_t g = 0; g < groups; ++g) {
                const size_t at = sub + 16 + 12 * g;
                if (cp >= be(font, at, 4) && cp <= be(font, at + 4, 4)) return true;
            }
            return false;
        }
    }
    for (uint32_t j = 0; j < subtables && cp < 0x10000; ++j) {  // a font without format 12: format 4 covers the BMP
        const size_t sub = cmap + be(font, cmap + 4 + 8 * j + 4, 4);
        if (be(font, sub, 2) != 4) continue;
        const uint32_t segs = be(font, sub + 6, 2) / 2;
        for (uint32_t k = 0; k < segs; ++k) {
            const uint32_t end = be(font, sub + 14 + 2 * k, 2);
            const uint32_t start = be(font, sub + 16 + 2 * segs + 2 * k, 2);
            if (cp >= start && cp <= end) return true;
        }
        return false;
    }
    return false;
}

}  // namespace

PB_TEST(timestamp_and_days_in_month_follow_the_core_rules) {
    SlashBridge s;
    PB_CHECK_EQ(s.timestamp(2026, 10, 11, 14, 30).toStdString(), std::string("2026-10-11 14:30"));
    PB_CHECK_EQ(s.timestamp(2026, 2, 31, 0, 0).toStdString(), std::string("2026-02-28 00:00"));
    PB_CHECK_EQ(s.daysInMonth(2024, 2), 29);
    PB_CHECK_EQ(s.daysInMonth(2100, 2), 28);
}

PB_TEST(the_emoji_grid_has_48_distinct_single_code_point_emojis_that_the_bundled_font_can_draw) {
    SlashBridge s;
    const QVariantList list = s.emojis();
    PB_CHECK_EQ(list.size(), 48);
    std::set<uint> seen;
    std::vector<uint> code_points;
    for (const QVariant& v : list) {
        const QVector<uint> ucs4 = v.toString().toUcs4();
        PB_CHECK_EQ(ucs4.size(), 1);                          // one code point: one tap, one character to delete
        const uint cp = ucs4.at(0);
        PB_CHECK(cp != 0xFE0F && cp != 0xFE0E && cp != 0x200D && !(cp >= 0x1F3FB && cp <= 0x1F3FF));  // no selector, joiner, skin tone
        PB_CHECK(seen.insert(cp).second);                      // distinct
        code_points.push_back(cp);
    }
    std::ifstream in(std::string(PBNOTES_SOURCE_DIR) + "/third_party/noto-emoji/NotoEmoji.ttf", std::ios::binary);
    if (!in.good()) PB_SKIP("the bundled emoji font is not in the tree");
    const std::string font((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    for (uint cp : code_points) PB_CHECK(font_has_glyph(font, cp));
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    return pbtest::run_all();
}
