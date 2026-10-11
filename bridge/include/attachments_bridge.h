#pragma once
// Attachment actions for the QML UI (contracts/qml-bridge.md): downloading, size probing, opening a file in
// the reader's viewer, and (spec 002 US5) adding a file from the editor. All network and file work runs on one
// worker thread, one operation at a time; results arrive as signals. Thin: the work is in notes_core.

#include <QObject>
#include <QSet>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

#include "notes_bridge.h"

class AttachmentsBridge : public QObject {
    Q_OBJECT
    // True when the server announces API version 1.4 (attachments); the editor hides Add otherwise.
    Q_PROPERTY(bool supported READ supported NOTIFY supportedChanged)
    // The size limit for one attachment, in bytes.
    Q_PROPERTY(qlonglong maxBytes READ maxBytes CONSTANT)
    // The folder the file picker starts in.
    Q_PROPERTY(QString rootDirectory READ rootDirectory CONSTANT)

public:
    struct Config {
        QString rootDirectory;                          // start folder of the file picker
        std::function<bool(const QString&)> openFile;   // hands a stored file to the system viewer
        std::function<bool()> wifiConnected;            // empty = always online
    };

    AttachmentsBridge(NotesBridge* notes, Config config, QObject* parent = nullptr);
    ~AttachmentsBridge() override;

    bool supported() const;
    qlonglong maxBytes() const;
    QString rootDirectory() const { return config_.rootDirectory; }

    // Downloads the file a note references (by the reference's path) on the worker thread; emits
    // attachmentsChanged(noteId) when stored, attachmentFailed(noteId, remotePath, text) otherwise.
    Q_INVOKABLE void download(qlonglong noteId, const QString& remotePath);
    // With Wi-Fi up, learns the sizes of the note's attachments that are not downloaded and whose size is
    // unknown (a short HEAD each); emits attachmentsChanged(noteId) if any size was found.
    Q_INVOKABLE void probeSizes(qlonglong noteId);
    // ---- adding a file in the editor (spec 002 US5) -------------------------------------------------
    // Copies the chosen file into the app folder as a staged attachment on the worker thread. The result arrives as
    // stageFinished({ok, id, token, name, size} or {ok:false, errorKey, errorText}); the editor inserts the token.
    Q_INVOKABLE void stageAttachment(const QString& path);
    // Called when the editor is left: staged attachments no saved note names are dropped (their files go too).
    Q_INVOKABLE void discardStaged();
    // The entries of a folder for the file picker: [{name, dir, size}], folders first, then files, both in
    // case-insensitive order; hidden, unreadable and dangling entries are skipped; empty for an unreadable path.
    Q_INVOKABLE QVariantList listDirectory(const QString& path) const;

    // Hands a downloaded file to the system viewer when its type is one the reader opens. False (and
    // attachmentFailed) if not.
    Q_INVOKABLE bool openAttachment(qlonglong attachmentId);

signals:
    void attachmentsChanged(qlonglong noteId);
    void attachmentFailed(qlonglong noteId, const QString& remotePath, const QString& text);
    void stageFinished(const QVariantMap& result);
    void supportedChanged();

private:
    void enqueue(std::function<void()> task);
    void runWorker();

    NotesBridge* notes_;
    Config config_;

    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::function<void()>> queue_;
    bool stop_ = false;
    QSet<QString> inFlight_;   // "noteId|path" of downloads queued or running (GUI thread only)
};
