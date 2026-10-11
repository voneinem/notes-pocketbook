#pragma once
// Editing tools of spec 005 (contracts/qml-bridge.md): the clipboard the editor and the reading page share.
// Thin: cleaning and the history rules are in notes_core. No text is ever logged, stored or sent from here.

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include <functional>

#include "pbnotes/edit_history.h"

class ClipboardBridge : public QObject {
    Q_OBJECT
    // True when there is text to paste (after cleaning).
    Q_PROPERTY(bool hasText READ hasText NOTIFY changed)

public:
    explicit ClipboardBridge(QObject* parent = nullptr);

    bool hasText() const { return has_text_; }

    // Copy and Cut (editor) and Copy (reading page). The text is normalized (paragraph separators, control
    // characters) and kept in the platform clipboard when there is one, always also in the app's own buffer; an
    // empty result is ignored.
    Q_INVOKABLE void copy(const QString& text);
    // What a Paste would take: the platform clipboard's text, else the app's own. Not yet cleaned.
    Q_INVOKABLE QString text() const;
    // {ok, text, errorKey, errorText}: the text a Paste would insert, cleaned (control characters out, line
    // breaks normalized, at most 1,000,000 characters). Nothing to paste: ok=false with an empty errorKey.
    Q_INVOKABLE QVariantMap cleanPaste() const;
    // {ok, text, caret, errorKey, errorText}: `current` (the title) with the selection [selBegin, selEnd)
    // (UTF-16 offsets) replaced by the one-line cleaned clipboard text; `caret` is the offset after it.
    // A result over 255 code points is refused (title_too_long) and nothing changes.
    Q_INVOKABLE QVariantMap titlePaste(const QString& current, int selBegin, int selEnd) const;

signals:
    void changed();

private:
    bool refresh();  // re-reads hasText; true when it changed

    QString buffer_;
    bool has_text_ = false;
    bool copying_ = false;  // our own setText raises dataChanged; copy() announces it itself
};

// The editor's undo/redo history over title, text, category and favorite (spec 005). One object; the editor
// calls reset() when it opens. Field names: "title", "text", "category", "favorite" (value "1" or "").
class EditSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)

public:
    explicit EditSession(QObject* parent = nullptr);

    bool canUndo() const { return history_.can_undo(); }
    bool canRedo() const { return history_.can_redo(); }

    // Tests drive the typing-group clock with this; the default is a monotonic millisecond clock.
    void setClockForTest(std::function<qint64()> clock) { clock_ = std::move(clock); }

    Q_INVOKABLE void reset(const QString& title, const QString& text, const QString& category, bool favorite);
    // Reports a field's new value; ignored for an unknown field, before reset and while an undo/redo is applied.
    Q_INVOKABLE void observe(const QString& field, const QString& value);
    Q_INVOKABLE void beginGroup(const QString& field);
    Q_INVOKABLE void endGroup();
    // {field, pos, removeLen, insert, value, selBegin, selEnd} to apply; empty when there is nothing to do.
    Q_INVOKABLE QVariantMap undo();
    Q_INVOKABLE QVariantMap redo();
    // The editor calls this after applying an undo/redo, passing what its fields now hold.
    Q_INVOKABLE void finishApply(const QString& title, const QString& text);

signals:
    void changed();

private:
    qint64 now() const;

    pbnotes::EditHistory history_;
    QElapsedTimer timer_;
    std::function<qint64()> clock_;
};
