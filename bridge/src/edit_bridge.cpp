#include "edit_bridge.h"

#include <QClipboard>
#include <QGuiApplication>

#include <limits>

#include <QVariantMap>

#include "pbnotes/slash.h"
#include "pbnotes/text_clean.h"

namespace {

QClipboard* platform_clipboard() {
    return qobject_cast<QGuiApplication*>(QCoreApplication::instance()) ? QGuiApplication::clipboard() : nullptr;
}

// The text as it is shown and pasted, without the size limit (that one applies on paste).
QString normalized(const QString& text) {
    const auto r = pbnotes::clean_pasted_text(text.toUtf8().toStdString(), std::numeric_limits<size_t>::max());
    return r.ok ? QString::fromStdString(r.text) : QString();
}

QString error_text(const std::string& key) {
    if (key == "paste_invalid") return QStringLiteral("The copied text cannot be pasted.");
    if (key == "paste_too_large") return QStringLiteral("The copied text is too large to paste.");
    if (key == "title_too_long") return QStringLiteral("The title is too long (at most 255 characters).");
    return QString();
}

QVariantMap failure(const std::string& key) {
    QVariantMap m;
    m.insert(QStringLiteral("ok"), false);
    m.insert(QStringLiteral("errorKey"), QString::fromStdString(key));
    m.insert(QStringLiteral("errorText"), error_text(key));
    return m;
}

constexpr int kMaxTitleCodePoints = 255;

bool is_low_surrogate(QChar c) { return c.isLowSurrogate(); }

}  // namespace

ClipboardBridge::ClipboardBridge(QObject* parent) : QObject(parent) {
    if (QClipboard* c = platform_clipboard()) connect(c, &QClipboard::dataChanged, this, [this] {
            if (!copying_ && refresh()) emit changed();
        });
}

void ClipboardBridge::copy(const QString& text) {
    const QString clean = normalized(text);
    if (clean.isEmpty()) return;
    buffer_ = clean;
    copying_ = true;
    if (QClipboard* c = platform_clipboard()) c->setText(clean);
    copying_ = false;
    refresh();
    emit changed();
}

QString ClipboardBridge::text() const {
    if (QClipboard* c = platform_clipboard()) {
        const QString t = c->text();
        if (!t.isEmpty()) return t;
    }
    return buffer_;
}

bool ClipboardBridge::refresh() {
    const bool now = !normalized(text()).isEmpty();
    if (now == has_text_) return false;
    has_text_ = now;
    return true;
}

QVariantMap ClipboardBridge::cleanPaste() const {
    const auto r = pbnotes::clean_pasted_text(text().toUtf8().toStdString());
    if (!r.ok) return failure(r.error_key);
    if (r.text.empty()) return failure(std::string());
    QVariantMap m;
    m.insert(QStringLiteral("ok"), true);
    m.insert(QStringLiteral("text"), QString::fromStdString(r.text));
    return m;
}

QVariantMap ClipboardBridge::titlePaste(const QString& current, int selBegin, int selEnd) const {
    const auto r = pbnotes::clean_pasted_text(text().toUtf8().toStdString());
    if (!r.ok) return failure(r.error_key);
    const QString line = QString::fromStdString(pbnotes::single_line(r.text));
    if (line.isEmpty()) return failure(std::string());

    int b = qBound(0, selBegin, static_cast<int>(current.size()));
    int e = qBound(0, selEnd, static_cast<int>(current.size()));
    if (b > e) std::swap(b, e);
    // an offset inside a surrogate pair never splits it: a caret goes before the pair, a range edge outward
    const bool caret = b == e;
    if (b > 0 && b < current.size() && is_low_surrogate(current.at(b))) --b;
    if (caret) e = b;
    else if (e > 0 && e < current.size() && is_low_surrogate(current.at(e))) ++e;

    const QString result = current.left(b) + line + current.mid(e);
    if (result.toUcs4().size() > kMaxTitleCodePoints) return failure("title_too_long");
    QVariantMap m;
    m.insert(QStringLiteral("ok"), true);
    m.insert(QStringLiteral("text"), result);
    m.insert(QStringLiteral("caret"), b + static_cast<int>(line.size()));
    return m;
}

// ---- EditSession ---------------------------------------------------------------------------------------------------

namespace {

std::u16string to_u16(const QString& s) { return std::u16string(reinterpret_cast<const char16_t*>(s.utf16()), static_cast<size_t>(s.size())); }
QString from_u16(const std::u16string& s) { return QString(reinterpret_cast<const QChar*>(s.data()), static_cast<qsizetype>(s.size())); }

bool field_of(const QString& name, pbnotes::EditField* out) {
    if (name == QLatin1String("title")) *out = pbnotes::EditField::Title;
    else if (name == QLatin1String("text")) *out = pbnotes::EditField::Text;
    else if (name == QLatin1String("category")) *out = pbnotes::EditField::Category;
    else if (name == QLatin1String("favorite")) *out = pbnotes::EditField::Favorite;
    else return false;
    return true;
}

const char* name_of(pbnotes::EditField f) {
    switch (f) {
        case pbnotes::EditField::Title: return "title";
        case pbnotes::EditField::Text: return "text";
        case pbnotes::EditField::Category: return "category";
        case pbnotes::EditField::Favorite: break;
    }
    return "favorite";
}

QVariantMap to_map(const pbnotes::EditApplied& a) {
    QVariantMap m;
    m.insert(QStringLiteral("field"), QString::fromLatin1(name_of(a.field)));
    m.insert(QStringLiteral("pos"), static_cast<int>(a.pos));
    m.insert(QStringLiteral("removeLen"), static_cast<int>(a.remove_len));
    m.insert(QStringLiteral("insert"), from_u16(a.insert));
    m.insert(QStringLiteral("value"), from_u16(a.value));
    m.insert(QStringLiteral("selBegin"), static_cast<int>(a.sel_begin));
    m.insert(QStringLiteral("selEnd"), static_cast<int>(a.sel_end));
    return m;
}

}  // namespace

EditSession::EditSession(QObject* parent) : QObject(parent) { timer_.start(); }

qint64 EditSession::now() const { return clock_ ? clock_() : timer_.elapsed(); }

void EditSession::reset(const QString& title, const QString& text, const QString& category, bool favorite) {
    history_.reset(to_u16(title), to_u16(text), to_u16(category), favorite);
    emit changed();
}

void EditSession::observe(const QString& field, const QString& value) {
    pbnotes::EditField f;
    if (!field_of(field, &f)) return;
    const bool could_undo = history_.can_undo(), could_redo = history_.can_redo();
    const std::u16string v = to_u16(value);
    history_.observe(f, v, now());
    if (could_undo != history_.can_undo() || could_redo != history_.can_redo()) emit changed();
}

void EditSession::beginGroup(const QString& field) {
    pbnotes::EditField f;
    if (field_of(field, &f)) history_.begin_group(f);
}

void EditSession::endGroup() {
    history_.end_group();
    emit changed();
}

QVariantMap EditSession::undo() {
    const auto a = history_.undo();
    if (!a) return QVariantMap();
    emit changed();
    return to_map(*a);
}

QVariantMap EditSession::redo() {
    const auto a = history_.redo();
    if (!a) return QVariantMap();
    emit changed();
    return to_map(*a);
}

void EditSession::finishApply(const QString& title, const QString& text) {
    history_.finish_apply(to_u16(title), to_u16(text));
    emit changed();
}

// ---- SlashBridge (spec 006) ----------------------------------------------------------------------------------

namespace {

QVariantMap edit_map(const pbnotes::SlashEdit& e) {
    QVariantMap m;
    m.insert(QStringLiteral("begin"), static_cast<int>(e.begin));
    m.insert(QStringLiteral("end"), static_cast<int>(e.end));
    m.insert(QStringLiteral("text"), from_u16(e.text));
    m.insert(QStringLiteral("caret"), static_cast<int>(e.caret));
    return m;
}

}  // namespace

QVariantList SlashBridge::entries() const {
    const QString labels[] = {tr("Bullet list"), tr("Table"),         tr("Info box"), tr("Link"),       tr("Bold"),       tr("Italic"),
                              tr("Underline"),   tr("Strikethrough"), tr("Details"),  tr("Quote"),      tr("Code block"), tr("Emoji")};
    QVariantList list;
    size_t i = 0;
    for (const auto& info : pbnotes::slash_entries()) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), QString::fromLatin1(info.id));
        m.insert(QStringLiteral("label"), labels[i++]);
        list.append(m);
    }
    return list;
}

QVariantMap SlashBridge::detect(const QString& before, const QString& after, int caret, int menuPos) const {
    const size_t menu = menuPos < 0 ? pbnotes::kNoSlash : static_cast<size_t>(menuPos);
    const pbnotes::SlashDetected d = pbnotes::detect_slash(to_u16(before), to_u16(after), caret < 0 ? 0 : static_cast<size_t>(caret), menu);
    QVariantMap m;
    const char* name = d.trigger == pbnotes::SlashTrigger::Menu ? "menu" : d.trigger == pbnotes::SlashTrigger::DateTime ? "datetime" : "none";
    m.insert(QStringLiteral("trigger"), QString::fromLatin1(name));
    m.insert(QStringLiteral("pos"), static_cast<int>(d.pos));
    return m;
}

QVariantMap SlashBridge::expand(const QString& id, const QString& text, int pos, int len) const {
    if (pos < 0 || len < 0 || static_cast<qsizetype>(pos) + len > text.size()) return QVariantMap();
    const std::string key = id.toStdString();
    for (const auto& info : pbnotes::slash_entries()) {
        if (key != info.id || info.entry == pbnotes::SlashEntry::Emoji) continue;
        return edit_map(pbnotes::expand_slash(info.entry, to_u16(text), static_cast<size_t>(pos), static_cast<size_t>(len)));
    }
    return QVariantMap();
}

QVariantMap SlashBridge::replace(int pos, int len, const QString& insert) const {
    if (pos < 0 || len < 0) return QVariantMap();
    return edit_map(pbnotes::replace_slash(static_cast<size_t>(pos), static_cast<size_t>(len), to_u16(insert)));
}

QString SlashBridge::timestamp(int year, int month, int day, int hour, int minute) const {
    return from_u16(pbnotes::format_timestamp(year, month, day, hour, minute));
}

int SlashBridge::daysInMonth(int year, int month) const { return pbnotes::days_in_month(year, month); }

QVariantList SlashBridge::emojis() const {
    // faces, hands, symbols, objects, food and transport that people reach for in notes
    static const char32_t kEmojis[] = {
        0x1F600, 0x1F602, 0x1F60A, 0x1F60D, 0x1F609, 0x1F60E, 0x1F914, 0x1F622, 0x1F62D, 0x1F621, 0x1F634, 0x1F642,
        0x1F44D, 0x1F44E, 0x1F44F, 0x1F64F, 0x1F4AA, 0x1F440, 0x1F64C, 0x1F44B,
        0x2764,  0x1F525, 0x2B50,  0x2705,  0x274C,  0x26A0,  0x2753,  0x2757,  0x1F4A1, 0x1F4CC, 0x1F4CE, 0x1F4C5,
        0x23F0,  0x1F4DD, 0x1F4D6, 0x1F4F7, 0x1F389, 0x1F381, 0x2615,  0x1F355, 0x1F3E0, 0x1F697, 0x2708,  0x2600,
        0x1F512, 0x1F514, 0x1F4AC, 0x270F};
    QVariantList list;
    for (char32_t cp : kEmojis) list.append(QString::fromUcs4(&cp, 1));
    return list;
}
