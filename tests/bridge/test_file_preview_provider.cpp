// Spec 009 US3 (contracts/qml-bridge.md "Image source image://filepreview/<previewId>", research R-4): the provider
// decodes a file the file browser listed to a grayscale image no wider than the request, off the UI thread, keeps the
// results in a small bounded cache and refuses everything it was not given an id for.
#include <sys/stat.h>
#include <unistd.h>

#include <memory>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QQuickImageResponse>
#include <QQuickTextureFactory>
#include <QStringList>
#include <QTimer>

#include "file_preview_provider.h"
#include "test_support.h"
#include "tmp_dir.h"

namespace {

void write_png(const QString& path, int width, int height, Qt::GlobalColor color = Qt::darkGreen) {
    QImage img(width, height, QImage::Format_RGB32);
    img.fill(color);
    PB_CHECK(img.save(path, "PNG"));
}

// Waits for `response` to finish (the response must not have finished before the call returned) and returns its image.
QImage await(QQuickImageResponse* response) {
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

QImage request(FilePreviewProvider& provider, int id, int width = 0) {
    return await(provider.requestImageResponse(QString::number(id), QSize(width, 0)));
}

// Lets queued events and finished jobs run for `ms` milliseconds.
void spin(int ms) {
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

struct Env {
    pbtest::TmpDir dir;
    std::shared_ptr<PreviewIndex> index = std::make_shared<PreviewIndex>();
    QString path(const char* name) const { return QString::fromStdString(dir.file(name)); }
    // Registers one path as the whole index and returns its id.
    int register_one(const QString& p) { return index->replace({p}).at(0); }
};

}  // namespace

PB_TEST(an_image_is_scaled_to_the_requested_width_as_grayscale_keeping_its_proportions) {
    Env e;
    write_png(e.path("a.png"), 400, 200);
    const int id = e.register_one(e.path("a.png"));
    FilePreviewProvider provider(e.index);
    const QImage out = request(provider, id, 100);
    PB_CHECK_EQ(out.width(), 100);
    PB_CHECK_EQ(out.height(), 50);
    PB_CHECK(out.format() == QImage::Format_Grayscale8);
}

PB_TEST(an_image_smaller_than_the_request_is_not_enlarged) {
    Env e;
    write_png(e.path("small.png"), 60, 30);
    const int id = e.register_one(e.path("small.png"));
    FilePreviewProvider provider(e.index);
    const QImage out = request(provider, id, 500);
    PB_CHECK_EQ(out.width(), 60);
    PB_CHECK_EQ(out.height(), 30);
}

PB_TEST(an_unknown_an_outdated_and_a_malformed_id_give_a_null_image) {
    Env e;
    write_png(e.path("a.png"), 40, 20);
    const int old_id = e.register_one(e.path("a.png"));
    FilePreviewProvider provider(e.index);
    PB_CHECK(!request(provider, old_id, 40).isNull());
    e.index->replace({});   // a newer listing: the old id is outdated
    PB_CHECK(request(provider, old_id, 40).isNull());
    PB_CHECK(request(provider, 987654, 40).isNull());
    PB_CHECK(await(provider.requestImageResponse(QStringLiteral("not-a-number"), QSize(40, 0))).isNull());
    PB_CHECK(await(provider.requestImageResponse(QStringLiteral("../../etc/passwd"), QSize(40, 0))).isNull());
    PB_CHECK(await(provider.requestImageResponse(QStringLiteral("0"), QSize(40, 0))).isNull());
    PB_CHECK(await(provider.requestImageResponse(QString(), QSize(40, 0))).isNull());
}

PB_TEST(a_missing_a_corrupt_and_a_non_regular_file_give_a_null_image) {
    Env e;
    QFile bad(e.path("x.jpg"));
    PB_CHECK(bad.open(QIODevice::WriteOnly));
    bad.write("this is not a jpeg at all");
    bad.close();
    PB_CHECK(::mkfifo(e.dir.file("pipe.jpg").c_str(), 0644) == 0);   // opening it for reading would block forever
    const QList<int> ids = e.index->replace({e.path("x.jpg"), e.path("gone.png"), e.path("pipe.jpg")});
    FilePreviewProvider provider(e.index);
    PB_CHECK(request(provider, ids.at(0), 100).isNull());
    PB_CHECK(request(provider, ids.at(1), 100).isNull());
    PB_CHECK(request(provider, ids.at(2), 100).isNull());
}

PB_TEST(an_image_with_too_many_pixels_is_refused_before_decoding) {
    Env e;
    write_png(e.path("big.png"), 100, 100);   // 10,000 pixels
    const int id = e.register_one(e.path("big.png"));
    FilePreviewProvider strict(e.index, 5000);
    PB_CHECK(request(strict, id, 50).isNull());
    FilePreviewProvider normal(e.index);
    PB_CHECK(!request(normal, id, 50).isNull());   // the default limit (40 million) is far higher
    PB_CHECK_EQ(FilePreviewProvider::defaultMaxPixels(), static_cast<qint64>(40000000));
}

PB_TEST(the_cache_never_holds_more_than_eight_megabytes_and_serves_repeats_without_decoding) {
    Env e;
    write_png(e.path("src.png"), 300, 300);
    QStringList paths;
    for (int i = 0; i < 400; ++i) {
        const QString p = e.path(("copy" + std::to_string(i) + ".png").c_str());
        PB_CHECK(QFile::copy(e.path("src.png"), p));
        paths << p;
    }
    const QList<int> ids = e.index->replace(paths);
    FilePreviewProvider provider(e.index);
    for (int id : ids) {
        const QImage out = request(provider, id, 300);   // 300 x 300 gray = 90,000 bytes each: 36 MB in all
        PB_CHECK_EQ(out.width(), 300);
        PB_CHECK(provider.cachedBytes() <= 8 * 1024 * 1024);
    }
    PB_CHECK_EQ(provider.decodeCount(), 400);
    PB_CHECK(provider.cachedBytes() > 4 * 1024 * 1024);   // the cache does keep a good part of them
    const int decodes = provider.decodeCount(), hits = provider.cacheHits();
    PB_CHECK(!request(provider, ids.back(), 300).isNull());   // the most recent one is still cached
    PB_CHECK_EQ(provider.decodeCount(), decodes);
    PB_CHECK_EQ(provider.cacheHits(), hits + 1);
}

PB_TEST(a_replaced_file_is_not_served_from_the_old_cache_entry) {
    Env e;
    write_png(e.path("pic.png"), 60, 30);
    const int id = e.register_one(e.path("pic.png"));
    FilePreviewProvider provider(e.index);
    PB_CHECK_EQ(request(provider, id, 500).width(), 60);
    write_png(e.path("pic.png"), 80, 40, Qt::red);   // another picture under the same name
    const QImage out = request(provider, id, 500);
    PB_CHECK_EQ(out.width(), 80);
    PB_CHECK_EQ(out.height(), 40);
}

PB_TEST(a_request_cancelled_while_queued_never_decodes) {
    Env e;
    write_png(e.path("first.png"), 50, 50);
    write_png(e.path("second.png"), 50, 50);
    const QList<int> ids = e.index->replace({e.path("first.png"), e.path("second.png")});
    FilePreviewProvider provider(e.index);
    provider.setDecodeDelayForTest(300);   // the first decode occupies the single worker for 300 ms
    QQuickImageResponse* first = provider.requestImageResponse(QString::number(ids.at(0)), QSize(50, 0));
    QQuickImageResponse* second = provider.requestImageResponse(QString::number(ids.at(1)), QSize(50, 0));
    second->cancel();   // its tile left the page
    PB_CHECK(!await(first).isNull());
    spin(600);          // the queued job has had its turn
    PB_CHECK_EQ(provider.decodeCount(), 1);
    second->deleteLater();
}

PB_TEST(the_original_file_is_untouched_by_previews) {
    Env e;
    write_png(e.path("keep.png"), 200, 100);
    const auto digest = [&] {
        QFile f(e.path("keep.png"));
        PB_CHECK(f.open(QIODevice::ReadOnly));
        return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256);
    };
    const QByteArray before = digest();
    const QDateTime time_before = QFileInfo(e.path("keep.png")).lastModified();
    const int id = e.register_one(e.path("keep.png"));
    FilePreviewProvider provider(e.index);
    PB_CHECK(!request(provider, id, 64).isNull());
    PB_CHECK(!request(provider, id, 32).isNull());
    PB_CHECK(digest() == before);
    PB_CHECK(QFileInfo(e.path("keep.png")).lastModified() == time_before);
    PB_CHECK_EQ(QDir(QString::fromStdString(e.dir.path())).entryList(QDir::Files).size(), 1);   // nothing was written next to it
}

PB_TEST(an_uncached_request_finishes_later_on_a_worker_not_inside_the_call) {
    Env e;
    write_png(e.path("a.png"), 100, 100);
    const int id = e.register_one(e.path("a.png"));
    FilePreviewProvider provider(e.index);
    QQuickImageResponse* response = provider.requestImageResponse(QString::number(id), QSize(50, 0));
    bool finished_inside = false;
    QObject::connect(response, &QQuickImageResponse::finished, [&] { finished_inside = true; });
    PB_CHECK(!finished_inside);   // the connection was made after the call returned, so a synchronous finish would be missed...
    const QImage out = await(response);   // ...and await would time out (and fail) if it had been
    PB_CHECK(!out.isNull());
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    return pbtest::run_all();
}
