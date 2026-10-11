#pragma once
// Thin Qt layer between the QML UI and the Qt-free core (contracts/qml-bridge.md). It contains no
// sync or storage logic of its own: every member delegates to notes_core. The password is never
// exposed back to QML after it was saved.

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QStringList>
#include <atomic>
#include <memory>
#include <thread>

#include "pbnotes/note_store.h"

class NotesBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool configured READ configured NOTIFY accountChanged)
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY accountChanged)
    Q_PROPERTY(QString username READ username NOTIFY accountChanged)
    // Which CA file is trusted: "device" (the reader's own store) or "bundled" (shipped with the app)
    Q_PROPERTY(QString caSource READ caSource NOTIFY caChanged)
    // custom-ca.pem in the data folder: "none", "loaded" or "invalid" (then it is ignored)
    Q_PROPERTY(QString customCaStatus READ customCaStatus NOTIFY caChanged)
    Q_PROPERTY(int customCaCertificates READ customCaCertificates NOTIFY caChanged)
    Q_PROPERTY(QString customCaPath READ customCaPath CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // "auto" (follow the sensor), "portrait" or "landscape" (locked); persisted
    Q_PROPERTY(QString rotationPreference READ rotationPreference WRITE setRotationPreference NOTIFY rotationPreferenceChanged)

    // Reader font size: "small" | "medium" | "large" (persisted) and its pixel size.
    Q_PROPERTY(QString fontSize READ fontSize WRITE setFontSize NOTIFY fontSizeChanged)
    Q_PROPERTY(int fontPx READ fontPx NOTIFY fontSizeChanged)
    // Sync state for the status line: running flag, the outcome key (never, running, success, partial,
    // failed, offline, maintenance) and a ready-to-show text (never empty).
    // Notes with changes the server has not confirmed yet (new, edited, deleted).
    Q_PROPERTY(int pendingCount READ pendingCount NOTIFY notesChanged)
    Q_PROPERTY(bool syncRunning READ syncRunning NOTIFY syncStateChanged)
    Q_PROPERTY(QString syncOutcome READ syncOutcome NOTIFY syncStateChanged)
    Q_PROPERTY(QString syncStatus READ syncStatus NOTIFY syncStateChanged)
    Q_PROPERTY(QString view READ view NOTIFY notesChanged)  // also changes when a category runs empty
    Q_PROPERTY(QString sortOrder READ sortOrder NOTIFY listOptionsChanged)
    Q_PROPERTY(QString searchText READ searchText NOTIFY listOptionsChanged)
    Q_PROPERTY(QVariantList categories READ categories NOTIFY notesChanged)
    Q_PROPERTY(int conflictCount READ conflictCount NOTIFY syncStateChanged)

public:
    struct Config {
        QString dataDir;                // holds pbnotes.db, custom-ca.pem, effective-ca.pem
        QString bundledCaPath;          // the CA file shipped with the app
        QStringList deviceCaCandidates; // the reader's own CA files, tried first
        QString attachmentsDir;         // downloaded and staged attachment files; empty = <dataDir>/attachments
    };

    explicit NotesBridge(const Config& config, QObject* parent = nullptr);
    ~NotesBridge() override;

    // False if the database could not be opened (then nothing else works).
    bool ok() const { return static_cast<bool>(store_); }
    pbnotes::NoteStore* store() const { return store_.get(); }

    bool configured() const;
    QString serverUrl() const;
    QString username() const;
    QString caSource() const { return caSource_; }
    QString customCaStatus() const { return customCaStatus_; }
    int customCaCertificates() const { return customCaCertificates_; }
    QString customCaPath() const { return config_.dataDir + QStringLiteral("/custom-ca.pem"); }
    // The CA file the transport trusts right now (for the attachment worker), and the attachment folder.
    QString effectiveCaPath() const { return caPath_; }
    QString attachmentsDir() const;
    // Re-reads the CA setup (device store, bundled file, custom-ca.pem) before a network operation.
    void refreshTrust() { refreshCa(); }
    bool busy() const { return busy_; }
    QString rotationPreference() const;
    void setRotationPreference(const QString& preference);

    // Checks the URL and credentials, then asks the server (capabilities) over a verified TLS
    // connection. Asynchronous: the result arrives as connectionTested().
    Q_INVOKABLE void testConnection(const QString& url, const QString& user, const QString& password);
    // Stores the account (one only). Returns false and emits errorOccurred() for bad input or if a
    // different account already exists; the same account may be saved again to change the password.
    // The setup entries are kept (also when the test fails) so nothing has to be retyped.
    // draft() returns {url, username, password}, empty when there is none.
    Q_INVOKABLE void saveDraft(const QString& url, const QString& user, const QString& password);
    Q_INVOKABLE QVariantMap draft() const;
    Q_INVOKABLE bool saveAccount(const QString& url, const QString& user, const QString& password);
    // Removes the saved login and the local notes. Refused (false, errorOccurred("pending_changes"))
    // while any note has changes that were not synced yet.
    Q_INVOKABLE bool forgetAccount();
    // The note list for the saved view and sort order: [{localId, title, category, excerpt, favorite,
    // modified, pending}]. Cheap enough to call again after every change (notesChanged()).
    Q_INVOKABLE QVariantList listNotes() const;
    // One note for the reader: {localId, title, titleText, hasTitle, category, favorite, modified, pending, blocks:[{kind
    // ("heading"|"paragraph"|"listitem"|"blank"), line, level, ordered, number, depth, task, checked,
    // html}]}. html is escaped text with <b>/<i> only (Text.StyledText). Empty map if unknown. `title` is the list's
    // title ("(untitled)" for a legacy note without one); `titleText` is the stored title as is and `hasTitle` is
    // false for an empty one: the reading page shows a title block only when it is true.
    Q_INVOKABLE QVariantMap openNote(qlonglong localId) const;
    // Copy on the reading page (spec 005): the note's source text for the selected range. Units are the title (unit 0,
    // only when the note has a title) followed by one unit per entry of openNote().blocks; offsets are UTF-16 units
    // of the unit's displayed text (0 for a whole unit: a table row, an attachment, a blank line). The range may come
    // in either order. "" for an unknown note or an invalid range. Never writes.
    Q_INVOKABLE QString readerCopy(qlonglong localId, int fromUnit, int fromOffset, int toUnit, int toOffset) const;
    // "1.5 KB", "1.0 MB"; "" for an unknown (negative) size.
    Q_INVOKABLE QString formatSize(qlonglong bytes) const;
    // Scroll offset of the next/previous page that does not cut a table row or an attachment: wraps the
    // core's page_target. `unsplittable` is a list of {top, bottom} maps in content coordinates.
    Q_INVOKABLE double pageTarget(double contentY, double viewHeight, double contentHeight, double step,
                                  int direction, const QVariantList& unsplittable) const;
    // ---- editing (all offline; every change stays pending until a sync sends it) ----------------
    // {title, text, category, favorite} for the editor (the title as stored, not display-processed); empty map if
    // the note is unknown or deleted.
    Q_INVOKABLE QVariantMap loadForEdit(qlonglong localId) const;
    // Explicit save of the title the user entered and the text. localId <= 0 creates a new note (empty text is
    // rejected). Returns the note's local id, or -1 after emitting errorOccurred() (title_*, content_empty,
    // category_*, storage_error, not_found); a failed save leaves the previous version untouched.
    Q_INVOKABLE qlonglong saveNote(qlonglong localId, const QString& title, const QString& text, const QString& category,
                                   bool favorite);
    // After the UI asked for confirmation. A note that never reached the server disappears at once; a
    // synced note is hidden until the server delete succeeds.
    Q_INVOKABLE bool deleteNote(qlonglong localId);
    Q_INVOKABLE bool setCategory(qlonglong localId, const QString& category);
    Q_INVOKABLE bool toggleFavorite(qlonglong localId);
    int pendingCount() const;
    // Flips the task box on `lineIndex` of the note as a normal local edit. False if the line is not
    // a task item or the note is unknown (nothing changes).
    Q_INVOKABLE bool toggleTask(qlonglong localId, int lineIndex);

    // ---- list options -----------------------------------------------------------------------
    // view: "all" | "favorites" | "uncategorized" | "category:<name>" (remembered; a category without
    // notes reads as "all"). sortOrder: "modified" | "title" (remembered). Search text: case-insensitive
    // substring of title and content, kept only while the app runs. Invalid values are ignored.
    // categories: [{name, count}] of the non-empty categories.
    QString view() const;
    QString sortOrder() const;
    QString searchText() const { return searchText_; }
    QVariantList categories() const;
    // The category picker's list for ChoiceDialog: [{label, value}] = "No category" (value "none"), "New category..."
    // (value "new"), then one entry per category of any note on the reader, value "c:" + name, in the store's
    // case-insensitive order. `current` (the editor's choice, possibly not saved yet) is always listed, once.
    Q_INVOKABLE QVariantList categoryChoices(const QString& current) const;
    // Judges a category name typed in "New category...": trims ASCII whitespace, refuses an empty name and
    // anything validate_category refuses. {ok, name (trimmed), errorText ("" when ok)}. Stores nothing; an
    // exact match with an existing category is simply that name (assigning it creates no second spelling).
    Q_INVOKABLE QVariantMap checkCategory(const QString& name) const;
    Q_INVOKABLE void setView(const QString& view);
    Q_INVOKABLE void setSortOrder(const QString& order);
    Q_INVOKABLE void setSearch(const QString& text);

    // Starts a sync (push, then pull) on a worker thread; ignored while one is running. The result
    // arrives as syncFinished() and the status properties update.
    Q_INVOKABLE void syncNow();
    // "yyyy-MM-dd HH:mm" in the system's local time (the C library's zone, the one the reader's own
    // clock uses; Qt's time zone lookup is not relied on).
    Q_INVOKABLE QString formatTime(qlonglong epochSeconds) const;

    QString fontSize() const;
    void setFontSize(const QString& size);
    int fontPx() const;
    bool syncRunning() const { return syncRunning_; }
    QString syncOutcome() const;
    QString syncStatus() const;
    // Conflict copies created by the last finished sync (0 if none).
    int conflictCount() const;

    // English text for a message key (connection test, errors).
    Q_INVOKABLE QString message(const QString& key) const;

signals:
    void connectionTested(bool ok, const QString& messageKey, const QString& message, const QString& normalizedUrl);
    void errorOccurred(const QString& messageKey, const QString& message);
    void accountChanged();
    void caChanged();
    void busyChanged();
    void rotationPreferenceChanged();
    void fontSizeChanged();
    void syncStateChanged();
    void listOptionsChanged();  // view, sort order or search text changed (notesChanged follows)
    void notesChanged();  // the list content changed (sync, edit); call listNotes() again
    void noteChanged(qlonglong localId);  // one note was saved, deleted or modified
    void syncFinished(const QString& outcome, const QString& message);
    // Emitted after a sync that kept both versions of a note (`conflicts`: copies created) or brought
    // back locally deleted notes that were changed on the server (`restored`).
    void conflictsDetected(int conflicts, int restored);

private:
    void refreshCa();
    void setBusy(bool busy);

    Config config_;
    std::unique_ptr<pbnotes::NoteStore> store_;
    QString caPath_;          // the effective CA file handed to the transport
    QString caSource_ = QStringLiteral("bundled");
    QString customCaStatus_ = QStringLiteral("none");
    int customCaCertificates_ = 0;
    std::atomic<bool> busy_{false};
    std::thread worker_;
    std::atomic<bool> syncRunning_{false};
    QString searchText_;
    std::thread syncWorker_;
};
