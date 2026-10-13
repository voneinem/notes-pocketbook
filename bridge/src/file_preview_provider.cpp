#include "file_preview_provider.h"

#include <QDateTime>
#include <QFileInfo>
#include <QMetaObject>
#include <QThread>

#include "attachment_image_provider.h"   // load_attachment_image
#include "image_texture_factory.h"

QList<int> PreviewIndex::replace(const QStringList& paths) {
    std::lock_guard<std::mutex> lock(mutex_);
    paths_.clear();
    QList<int> ids;
    ids.reserve(paths.size());
    for (const QString& p : paths) {
        paths_.insert(++last_, p);
        ids.append(last_);
    }
    return ids;
}

QString PreviewIndex::path(int id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return paths_.value(id);
}

FilePreviewJob::FilePreviewJob(FilePreviewProvider* provider, QString path, QString key, int width,
                               std::shared_ptr<std::atomic<bool>> cancelled)
    : provider_(provider), path_(std::move(path)), key_(std::move(key)), width_(width), cancelled_(std::move(cancelled)) {
    setAutoDelete(true);
}

void FilePreviewJob::run() {
    if (cancelled_->load()) {   // its tile left the page while it waited: nothing to decode
        emit done(QImage());
        return;
    }
    emit done(provider_->decode(path_, key_, width_));
}

QQuickTextureFactory* FilePreviewResponse::textureFactory() const { return new ImageTextureFactory(image_); }

void FilePreviewResponse::complete(QImage image) {
    image_ = std::move(image);
    emit finished();
}

FilePreviewProvider::FilePreviewProvider(std::shared_ptr<PreviewIndex> index, qint64 maxPixels)
    : index_(std::move(index)), maxPixels_(maxPixels), cache_(static_cast<qsizetype>(kCacheBytes)) {
    pool_.setMaxThreadCount(1);
}

FilePreviewProvider::~FilePreviewProvider() { pool_.waitForDone(); }

qint64 FilePreviewProvider::cachedBytes() const {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    return cache_.totalCost();
}

QImage FilePreviewProvider::decode(const QString& path, const QString& key, int width) {
    ++decodes_;
    const int delay = delayMs_.load();
    if (delay > 0) QThread::msleep(static_cast<unsigned long>(delay));
    const QImage image = load_attachment_image(path, width, maxPixels_);
    if (!image.isNull()) {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        cache_.insert(key, new QImage(image), image.sizeInBytes());
    }
    return image;
}

QQuickImageResponse* FilePreviewProvider::requestImageResponse(const QString& id, const QSize& requestedSize) {
    auto* response = new FilePreviewResponse;
    auto finishLater = [response](const QImage& image) {
        QMetaObject::invokeMethod(response, [response, image]() { response->complete(image); }, Qt::QueuedConnection);
    };
    bool ok = false;
    const int previewId = id.toInt(&ok);
    const QString path = ok ? index_->path(previewId) : QString();
    const QFileInfo info(path);
    if (path.isEmpty() || !info.isFile()) {   // unknown or outdated id, a file that is gone, a pipe or a device
        finishLater(QImage());
        return response;
    }
    const int width = requestedSize.width() > 0 ? requestedSize.width() : 0;
    const QString key = path + QLatin1Char('|') + QString::number(info.size()) + QLatin1Char('|') +
                        QString::number(info.lastModified().toMSecsSinceEpoch()) + QLatin1Char('|') + QString::number(width);
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        if (const QImage* cached = cache_.object(key)) {
            ++hits_;
            finishLater(*cached);
            return response;
        }
    }
    auto* job = new FilePreviewJob(this, path, key, width, response->cancelFlag());
    QObject::connect(job, &FilePreviewJob::done, response, &FilePreviewResponse::complete, Qt::QueuedConnection);
    pool_.start(job);
    return response;
}
