#include "edit_bridge.h"

#include <QClipboard>
#include <QGuiApplication>

#include <limits>

#include <QVariantMap>

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
