#pragma once
// Previews for the file browser's tile view (spec 009, research R-4): QML image source
// "image://filepreview/<previewId>". The id is one AttachmentsBridge::listDirectory handed out; it names a file the
// browser itself listed (PreviewIndex), so no file name ever goes through a URL and nothing else can be read.

#include <QCache>
#include <QHash>
#include <QImage>
#include <QList>
#include <QObject>
#include <QQuickAsyncImageProvider>
#include <QQuickImageResponse>
#include <QRunnable>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <atomic>
#include <memory>
#include <mutex>

// previewId -> absolute path, for the image files of the latest listing only. Thread-safe: the GUI thread fills it,
// the decoder thread reads it.
class PreviewIndex {
public:
    // Forgets every earlier id and numbers `paths` anew; returns their ids in the same order. Ids start at 1 and only
    // grow, so an id of an earlier listing never names another file later.
    QList<int> replace(const QStringList& paths);
    // The path of `id`, empty for an unknown or outdated id.
    QString path(int id) const;

private:
    mutable std::mutex mutex_;
    QHash<int, QString> paths_;
    int last_ = 0;
};

class FilePreviewProvider;

// One decode on the provider's worker thread; implementation detail, declared here only so moc sees it.
class FilePreviewJob : public QObject, public QRunnable {
    Q_OBJECT
public:
    FilePreviewJob(FilePreviewProvider* provider, QString path, QString key, int width, std::shared_ptr<std::atomic<bool>> cancelled);
    void run() override;
signals:
    void done(QImage image);

private:
    FilePreviewProvider* provider_;
    QString path_, key_;
    int width_;
    std::shared_ptr<std::atomic<bool>> cancelled_;
};

class FilePreviewResponse : public QQuickImageResponse {
    Q_OBJECT
public:
    FilePreviewResponse() : cancelled_(std::make_shared<std::atomic<bool>>(false)) {}
    QQuickTextureFactory* textureFactory() const override;
    // The engine calls this when the tile that asked is gone: a job that has not started then never decodes.
    void cancel() override { cancelled_->store(true); }
    std::shared_ptr<std::atomic<bool>> cancelFlag() const { return cancelled_; }
public slots:
    void complete(QImage image);

private:
    QImage image_;
    std::shared_ptr<std::atomic<bool>> cancelled_;
};

// Serves image://filepreview/<previewId>?: the file the id names (see PreviewIndex), scaled to at most the requested
// width as 8-bit grayscale (the decoder of attachments, load_attachment_image), on a private single-thread pool so
// that photos never occupy more than one core and never wait behind or in front of other work. Decoded images are
// kept in memory only, up to 8 MB, least recently used out first. Anything else gives a null image: an unknown or
// outdated id, a file that is gone, not a regular file, unreadable, undecodable or of too many pixels.
class FilePreviewProvider : public QQuickAsyncImageProvider {
public:
    static constexpr qint64 kCacheBytes = 8 * 1024 * 1024;
    static qint64 defaultMaxPixels() { return 40000000; }

    explicit FilePreviewProvider(std::shared_ptr<PreviewIndex> index, qint64 maxPixels = defaultMaxPixels());
    ~FilePreviewProvider() override;
    QQuickImageResponse* requestImageResponse(const QString& id, const QSize& requestedSize) override;

    // Test hooks (tests/bridge/test_file_preview_provider.cpp).
    qint64 cachedBytes() const;
    int cacheHits() const { return hits_.load(); }
    int decodeCount() const { return decodes_.load(); }
    void setDecodeDelayForTest(int ms) { delayMs_.store(ms); }

private:
    friend class FilePreviewJob;
    QImage decode(const QString& path, const QString& key, int width);

    std::shared_ptr<PreviewIndex> index_;
    qint64 maxPixels_;
    mutable std::mutex cacheMutex_;
    QCache<QString, QImage> cache_;
    std::atomic<int> hits_{0}, decodes_{0}, delayMs_{0};
    QThreadPool pool_;   // last member: destroyed (and drained) first
};
