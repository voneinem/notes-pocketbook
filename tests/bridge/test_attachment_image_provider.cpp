// US4 (spec 002 FR-019, FR-031, research R-7): the image provider decodes a stored attachment to a grayscale
// image no wider than the request, and answers a corrupt or unknown file with a null image.
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QImage>
#include <QQuickImageResponse>
#include <QQuickTextureFactory>
#include <QTimer>

#include "attachment_image_provider.h"
#include "pbnotes/note_store.h"
#include "test_support.h"
#include "tmp_dir.h"

namespace {

struct Env {
    pbtest::TmpDir dir;
    QString attachments;
    std::unique_ptr<pbnotes::NoteStore> store;
    int64_t note = 0;
    Env() {
        attachments = QString::fromStdString(dir.file("attachments"));
        QDir().mkpath(attachments);
        store = pbnotes::NoteStore::open(dir.file("db"));
        pbnotes::ApiNote n;
        n.id = 1;
        n.etag = "e1";
        n.content = "Note";
        n.modified = 100;
        store->apply_remote({n});
        note = store->get_by_remote_id(1)->local_id;
    }
    // Registers a downloaded row for `remote_path` whose file is written by `write(path)`.
    template <typename F>
    int64_t add(const std::string& remote_path, const std::string& ext, F write) {
        const int64_t id = store->ensure_attachment(note, remote_path)->id;
        const std::string file = std::to_string(id) + (ext.empty() ? "" : "." + ext);
        write(attachments + QLatin1Char('/') + QString::fromStdString(file));
        PB_CHECK(store->record_downloaded(note, remote_path, 100, file));
        return id;
    }
};

// The image the provider returns for `request` ("<id>?w=<px>").
QImage request(AttachmentImageProvider& provider, const QString& id) {
    QQuickImageResponse* response = provider.requestImageResponse(id, QSize());
    QImage out;
    bool done = false;
    QEventLoop loop;
    QObject::connect(response, &QQuickImageResponse::finished, &loop, [&] {
        done = true;
        if (QQuickTextureFactory* f = response->textureFactory()) {
            out = f->image();
            delete f;
        }
        loop.quit();
    });
    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    if (!done) loop.exec();
    PB_CHECK(done);
    response->deleteLater();
    return out;
}

}  // namespace

PB_TEST(a_png_is_returned_as_grayscale_no_wider_than_requested_with_its_aspect_ratio) {
    Env e;
    const int64_t id = e.add("a.png", "png", [](const QString& path) {
        QImage img(400, 200, QImage::Format_RGB32);
        img.fill(Qt::red);
        PB_CHECK(img.save(path, "PNG"));
    });
    AttachmentImageProvider provider(e.store.get(), e.attachments);
    const QImage out = request(provider, QString::number(id) + QStringLiteral("?w=100"));
    PB_CHECK(!out.isNull());
    PB_CHECK(out.format() == QImage::Format_Grayscale8);
    PB_CHECK_EQ(out.width(), 100);
    PB_CHECK_EQ(out.height(), 50);
}

PB_TEST(an_image_smaller_than_the_request_is_not_enlarged) {
    Env e;
    const int64_t id = e.add("small.png", "png", [](const QString& path) {
        QImage img(60, 30, QImage::Format_RGB32);
        img.fill(Qt::blue);
        PB_CHECK(img.save(path, "PNG"));
    });
    AttachmentImageProvider provider(e.store.get(), e.attachments);
    const QImage out = request(provider, QString::number(id) + QStringLiteral("?w=500"));
    PB_CHECK_EQ(out.width(), 60);
    PB_CHECK_EQ(out.height(), 30);
}

PB_TEST(without_a_width_the_image_keeps_its_size) {
    Env e;
    const int64_t id = e.add("a.png", "png", [](const QString& path) {
        QImage img(80, 40, QImage::Format_RGB32);
        img.fill(Qt::green);
        PB_CHECK(img.save(path, "PNG"));
    });
    AttachmentImageProvider provider(e.store.get(), e.attachments);
    const QImage out = request(provider, QString::number(id));
    PB_CHECK_EQ(out.width(), 80);
}

PB_TEST(an_image_with_too_many_pixels_is_refused_before_decoding) {
    pbtest::TmpDir d;
    const QString path = QString::fromStdString(d.file("big.png"));
    QImage img(300, 200, QImage::Format_Grayscale8);
    img.fill(Qt::gray);
    PB_CHECK(img.save(path, "PNG"));
    PB_CHECK(!load_attachment_image(path, 0, 100000).isNull());   // 60,000 pixels: fine
    PB_CHECK(load_attachment_image(path, 0, 50000).isNull());     // over the (lowered) limit: refused
    PB_CHECK(!load_attachment_image(path, 0).isNull());           // the default limit is far higher
}

PB_TEST(a_corrupt_file_gives_a_null_image) {
    Env e;
    const int64_t id = e.add("bad.png", "png", [](const QString& path) {
        QFile f(path);
        PB_CHECK(f.open(QIODevice::WriteOnly));
        f.write("this is not a png at all");
    });
    AttachmentImageProvider provider(e.store.get(), e.attachments);
    PB_CHECK(request(provider, QString::number(id) + QStringLiteral("?w=100")).isNull());
}

PB_TEST(an_unknown_row_or_a_row_without_a_file_gives_a_null_image) {
    Env e;
    AttachmentImageProvider provider(e.store.get(), e.attachments);
    PB_CHECK(request(provider, QStringLiteral("424242?w=100")).isNull());
    PB_CHECK(request(provider, QStringLiteral("not-a-number")).isNull());
    PB_CHECK(request(provider, QStringLiteral("../../etc/passwd")).isNull());
    const int64_t remote = e.store->ensure_attachment(e.note, "r.png")->id;   // a remote row has no file
    PB_CHECK(request(provider, QString::number(remote)).isNull());
}

PB_TEST(a_missing_file_behind_a_row_gives_a_null_image) {
    Env e;
    const int64_t id = e.store->ensure_attachment(e.note, "gone.png")->id;
    PB_CHECK(e.store->record_downloaded(e.note, "gone.png", 5, std::to_string(id) + ".png"));
    AttachmentImageProvider provider(e.store.get(), e.attachments);
    PB_CHECK(request(provider, QString::number(id) + QStringLiteral("?w=100")).isNull());
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    return pbtest::run_all();
}
