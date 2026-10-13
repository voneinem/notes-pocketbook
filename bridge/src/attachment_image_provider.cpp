#include "attachment_image_provider.h"

#include "image_texture_factory.h"

#include <QFileInfo>
#include <QImageReader>
#include <QMetaObject>
#include <QPointer>
#include <QQuickTextureFactory>
#include <QQuickWindow>
#include <QSGTexture>
#include <QThreadPool>

namespace {

// "<digits>" and "<digits>.<ext>" only: a file name that is an id with a plain extension.
bool is_stored_file_name(const QString& name) {
    int i = 0;
    while (i < name.size() && name.at(i).isDigit()) ++i;
    if (i == 0) return false;
    if (i == name.size()) return true;
    if (name.at(i) != QLatin1Char('.') || name.size() - i - 1 < 1 || name.size() - i - 1 > 8) return false;
    for (int k = i + 1; k < name.size(); ++k) {
        const QChar c = name.at(k);
        if (!((c >= QLatin1Char('a') && c <= QLatin1Char('z')) || c.isDigit())) return false;
    }
    return true;
}

// The decoded image for a request id "<attachment id>[?w=<pixels>]".
QImage load_for_request(pbnotes::NoteStore* store, const QString& dir, const QString& request) {
    const int q = request.indexOf(QLatin1Char('?'));
    const QString id_part = q < 0 ? request : request.left(q);
    bool ok = false;
    const qlonglong id = id_part.toLongLong(&ok);
    if (!ok || id <= 0) return QImage();
    int width = 0;
    if (q >= 0) {
        for (const QString& pair : request.mid(q + 1).split(QLatin1Char('&'))) {
            if (pair.startsWith(QLatin1String("w="))) {
                bool wok = false;
                const int w = pair.mid(2).toInt(&wok);
                if (wok && w > 0 && w < 20000) width = w;
            }
        }
    }
    const auto row = store->get_attachment(id);
    if (!row || row->local_file.empty()) return QImage();
    const QString file = QString::fromStdString(row->local_file);
    if (!is_stored_file_name(file)) return QImage();
    return load_attachment_image(dir + QLatin1Char('/') + file, width);
}

}  // namespace

QImage load_attachment_image(const QString& path, int max_width, qint64 max_pixels) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (size.isValid() && static_cast<qint64>(size.width()) * size.height() > max_pixels) return QImage();
    if (size.isValid() && max_width > 0 && size.width() > max_width)
        reader.setScaledSize(QSize(max_width, qMax(1, static_cast<int>(static_cast<qint64>(size.height()) * max_width / size.width()))));
    QImage image = reader.read();
    if (image.isNull()) return QImage();
    if (max_width > 0 && image.width() > max_width) image = image.scaledToWidth(max_width, Qt::SmoothTransformation);
    return image.convertToFormat(QImage::Format_Grayscale8);
}

AttachmentImageJob::AttachmentImageJob(pbnotes::NoteStore* store, QString dir, QString request)
    : store_(store), dir_(std::move(dir)), request_(std::move(request)) {
    setAutoDelete(true);
}

void AttachmentImageJob::run() { emit done(load_for_request(store_, dir_, request_)); }

AttachmentImageResponse::AttachmentImageResponse(pbnotes::NoteStore* store, const QString& dir, const QString& request) {
    auto* job = new AttachmentImageJob(store, dir, request);
    connect(job, &AttachmentImageJob::done, this, &AttachmentImageResponse::handleDone, Qt::QueuedConnection);
    QThreadPool::globalInstance()->start(job);
}

QQuickTextureFactory* AttachmentImageResponse::textureFactory() const {
    return new ImageTextureFactory(image_);
}

void AttachmentImageResponse::handleDone(QImage image) {
    image_ = std::move(image);
    emit finished();
}

AttachmentImageProvider::AttachmentImageProvider(pbnotes::NoteStore* store, QString attachments_dir)
    : store_(store), dir_(std::move(attachments_dir)) {}

QQuickImageResponse* AttachmentImageProvider::requestImageResponse(const QString& id, const QSize&) {
    return new AttachmentImageResponse(store_, dir_, id);
}

