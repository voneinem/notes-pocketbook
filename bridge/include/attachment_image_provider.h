#pragma once
// QML image source "image://attachment/<attachment id>?w=<pixels>" (contracts/qml-bridge.md, research R-7).
// Decodes the stored file off the UI thread, scaled to at most the requested width, as an 8-bit grayscale
// image (predictable on the e-ink panel). An unknown row, a missing or corrupt file, or a format the device
// Qt cannot read give a null image, and the reader then shows the placeholder with the reason.

#include <QImage>
#include <QObject>
#include <QQuickAsyncImageProvider>
#include <QQuickImageResponse>
#include <QRunnable>
#include <QString>

#include "pbnotes/note_store.h"

// Loads `path` scaled to at most `max_width` pixels wide (0 = no limit), keeping the aspect ratio, never
// enlarging, honoring the EXIF orientation, as Format_Grayscale8. A null image on any failure, and for an image
// of more than `max_pixels` pixels (a small file can describe a huge picture; it is refused before decoding).
QImage load_attachment_image(const QString& path, int max_width, qint64 max_pixels = 40000000);

// Decodes on a worker thread; implementation detail of the provider, declared here only so moc sees it.
class AttachmentImageJob : public QObject, public QRunnable {
    Q_OBJECT
public:
    AttachmentImageJob(pbnotes::NoteStore* store, QString dir, QString request);
    void run() override;
signals:
    void done(QImage image);

private:
    pbnotes::NoteStore* store_;
    QString dir_;
    QString request_;
};

class AttachmentImageResponse : public QQuickImageResponse {
    Q_OBJECT
public:
    AttachmentImageResponse(pbnotes::NoteStore* store, const QString& dir, const QString& request);
    QQuickTextureFactory* textureFactory() const override;

private slots:
    void handleDone(QImage image);

private:
    QImage image_;
};

class AttachmentImageProvider : public QQuickAsyncImageProvider {
public:
    // `store` must outlive the provider.
    AttachmentImageProvider(pbnotes::NoteStore* store, QString attachments_dir);
    QQuickImageResponse* requestImageResponse(const QString& id, const QSize& requested_size) override;

private:
    pbnotes::NoteStore* store_;
    QString dir_;
};
