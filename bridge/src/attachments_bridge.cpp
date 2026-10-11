#include "attachments_bridge.h"

#include <QDir>
#include <QFileInfo>
#include <QMetaObject>

#include "pbnotes/api_client.h"
#include "pbnotes/attachment_ref.h"
#include "pbnotes/attachment_service.h"
#include "pbnotes/curl_transport.h"
#include "pbnotes/log.h"
#include "pbnotes/markdown.h"
#include "reader_blocks.h"

AttachmentsBridge::AttachmentsBridge(NotesBridge* notes, Config config, QObject* parent)
    : QObject(parent), notes_(notes), config_(std::move(config)) {
    worker_ = std::thread([this]() { runWorker(); });
    connect(notes_, &NotesBridge::syncFinished, this, [this]() { emit supportedChanged(); });
}

AttachmentsBridge::~AttachmentsBridge() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    wake_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void AttachmentsBridge::enqueue(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(std::move(task));
    }
    wake_.notify_one();
}

void AttachmentsBridge::runWorker() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [this] { return stop_ || !queue_.empty(); });
            if (stop_) return;  // queued work is dropped: nothing in it is needed to stay consistent
            task = std::move(queue_.front());
            queue_.pop_front();
        }
        task();
    }
}

bool AttachmentsBridge::supported() const {
    auto* store = notes_->store();
    auto account = store ? store->get_account() : std::nullopt;
    return account && pbnotes::supports_attachments(account->api_versions);
}

qlonglong AttachmentsBridge::maxBytes() const { return static_cast<qlonglong>(pbnotes::kMaxAttachmentBytes); }

void AttachmentsBridge::download(qlonglong noteId, const QString& remotePath) {
    pbnotes::NoteStore* store = notes_->store();
    if (!store) return;
    const QString key = QString::number(noteId) + QLatin1Char('|') + remotePath;
    if (inFlight_.contains(key)) return;  // one operation at a time per attachment
    inFlight_.insert(key);
    notes_->refreshTrust();
    const std::string ca = notes_->effectiveCaPath().toStdString();
    const std::string dir = notes_->attachmentsDir().toStdString();
    const std::string path = remotePath.toStdString();
    enqueue([this, store, ca, dir, path, noteId, key, remotePath]() {
        pbnotes::CurlTransport transport(ca);
        pbnotes::AttachmentService service(*store, transport, dir);
        const auto result = service.download(noteId, path);
        QMetaObject::invokeMethod(this, [this, result, noteId, remotePath, key]() {
            inFlight_.remove(key);
            if (result.ok) emit attachmentsChanged(noteId);
            else emit attachmentFailed(noteId, remotePath, attachment_error_text(result.error_key));
        }, Qt::QueuedConnection);
    });
}

void AttachmentsBridge::probeSizes(qlonglong noteId) {
    pbnotes::NoteStore* store = notes_->store();
    if (!store || !supported()) return;
    if (config_.wifiConnected && !config_.wifiConnected()) return;
    const auto note = store->get(noteId);
    if (!note) return;
    // only references whose size is not known and that are not downloaded yet, and not too many at once
    std::vector<std::string> paths;
    for (const pbnotes::AttachmentRef& ref : pbnotes::find_attachment_refs(note->content)) {
        if (ref.path.empty() || paths.size() >= 20) continue;
        const auto row = store->attachment_for(noteId, ref.path);
        if (row && (row->size || row->state == pbnotes::AttachmentState::Downloaded)) continue;
        paths.push_back(ref.path);
    }
    if (paths.empty()) return;
    notes_->refreshTrust();
    const std::string ca = notes_->effectiveCaPath().toStdString();
    const std::string dir = notes_->attachmentsDir().toStdString();
    enqueue([this, store, ca, dir, paths, noteId]() {
        pbnotes::CurlTransport transport(ca);
        pbnotes::AttachmentService service(*store, transport, dir);
        bool learned = false;
        for (const std::string& p : paths) learned = service.probe_size(noteId, p).has_value() || learned;
        if (learned) QMetaObject::invokeMethod(this, [this, noteId]() { emit attachmentsChanged(noteId); }, Qt::QueuedConnection);
    });
}

void AttachmentsBridge::stageAttachment(const QString& path) {
    pbnotes::NoteStore* store = notes_->store();
    if (!store) {
        emit stageFinished({{QStringLiteral("ok"), false}, {QStringLiteral("errorKey"), QStringLiteral("unreadable")},
                            {QStringLiteral("errorText"), attachment_error_text("unreadable")}});
        return;
    }
    notes_->refreshTrust();
    const std::string ca = notes_->effectiveCaPath().toStdString();
    const std::string dir = notes_->attachmentsDir().toStdString();
    const std::string source = path.toStdString();
    enqueue([this, store, ca, dir, source]() {
        pbnotes::CurlTransport transport(ca);  // nothing is sent: the service only needs the seam
        pbnotes::AttachmentService service(*store, transport, dir);
        const pbnotes::AddResult r = service.add(source);
        QVariantMap m;
        m.insert(QStringLiteral("ok"), r.ok);
        if (r.ok) {
            m.insert(QStringLiteral("id"), static_cast<qlonglong>(r.id));
            m.insert(QStringLiteral("token"), QString::fromStdString(r.token));
            m.insert(QStringLiteral("name"), QString::fromStdString(r.display_name));
            m.insert(QStringLiteral("size"), static_cast<qlonglong>(r.size));
        } else {
            m.insert(QStringLiteral("errorKey"), QString::fromStdString(r.error_key));
            m.insert(QStringLiteral("errorText"), attachment_error_text(r.error_key));
        }
        QMetaObject::invokeMethod(this, [this, m]() { emit stageFinished(m); }, Qt::QueuedConnection);
    });
}

void AttachmentsBridge::discardStaged() {
    pbnotes::NoteStore* store = notes_->store();
    if (!store) return;
    store->discard_staged();
    // their files: on the worker, so it never overlaps a copy or a download in progress
    const std::string dir = notes_->attachmentsDir().toStdString();
    enqueue([store, dir]() { pbnotes::AttachmentService::purge(*store, dir); });
}

QVariantList AttachmentsBridge::listDirectory(const QString& path) const {
    QVariantList out;
    if (path.isEmpty()) return out;
    const QDir dir(path);
    if (!dir.exists() || !QFileInfo(path).isReadable()) return out;
    const QFileInfoList entries = dir.entryInfoList(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot,
                                                    QDir::DirsFirst | QDir::IgnoreCase | QDir::Name);
    for (const QFileInfo& e : entries) {
        if (!e.exists() || !e.isReadable()) continue;  // dangling links and entries the user cannot read
        QVariantMap m;
        m.insert(QStringLiteral("name"), e.fileName());
        m.insert(QStringLiteral("dir"), e.isDir());
        m.insert(QStringLiteral("size"), e.isDir() ? 0 : static_cast<qlonglong>(e.size()));
        out.append(m);
    }
    return out;
}

bool AttachmentsBridge::openAttachment(qlonglong attachmentId) {
    pbnotes::NoteStore* store = notes_->store();
    const auto row = store ? store->get_attachment(attachmentId) : std::nullopt;
    if (!row || row->state != pbnotes::AttachmentState::Downloaded || row->local_file.empty()) return false;
    const qlonglong note = row->note_local_id.value_or(0);
    const QString remote = QString::fromStdString(row->remote_path.value_or(std::string()));
    if (!pbnotes::is_viewable_extension(pbnotes::safe_extension(row->local_file)) || !config_.openFile) {
        emit attachmentFailed(note, remote, QStringLiteral("No viewer for this file on this device."));
        return false;
    }
    const bool ok = config_.openFile(notes_->attachmentsDir() + QLatin1Char('/') + QString::fromStdString(row->local_file));
    if (!ok) emit attachmentFailed(note, remote, QStringLiteral("No viewer for this file on this device."));
    return ok;
}
