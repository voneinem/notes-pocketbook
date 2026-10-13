// Drives the real QML pages offscreen (spec 005): a driver script (argv[1], a QML file) loads a page from qml/pages
// with stub `bridge` / `attachments` objects and the real ClipboardBridge and EditSession, runs its checks and
// exits with the number of failures. Usage: test_qml_pages <driver.qml> <page.qml> [key=value ...]
// Every key=value pair is passed to the driver as a context property `arg_<key>`.
#include <cstdio>
#include <cstdlib>

#include <QGuiApplication>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QQuickWindow>
#include <QUrl>
#include <QtTest/QtTest>

#include "edit_bridge.h"

// Real mouse events into the driver's window (scene coordinates), so a page is driven like a finger would: a press, a
// wait in the driver's own timer for a long press, then a release.
class Pointer : public QObject {
    Q_OBJECT
public:
    QPointer<QWindow> window;
    Q_INVOKABLE void press(double x, double y) { QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, pt(x, y)); }
    Q_INVOKABLE void move(double x, double y) {
        QTest::mouseMove(window, pt(x, y));
    }
    Q_INVOKABLE void release(double x, double y) { QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, pt(x, y)); }
    // typed characters, as the focused item receives them from a keyboard (spec 006: the "/" menu watches typing)
    Q_INVOKABLE void type(const QString& text) {
        for (const QChar c : text) QTest::keyClick(window, c.toLatin1());
    }
    // a picture of the window, for looking at a state by hand (optional context property `arg_shot` in a driver)
    Q_INVOKABLE bool shot(const QString& path) {
        auto* w = qobject_cast<QQuickWindow*>(window.data());
        return w && w->grabWindow().save(path);
    }
private:
    static QPoint pt(double x, double y) { return QPoint(qRound(x), qRound(y)); }
};

// Stand-in for the file browser's preview provider (spec 009): a small gray picture for the ids 1..30 of the stub
// listing, a null image (QML status Error) for every other id.
class StubPreviewProvider : public QQuickImageProvider {
public:
    StubPreviewProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString& id, QSize* size, const QSize&) override {
        bool ok = false;
        const int n = id.toInt(&ok);
        if (!ok || n < 1 || n > 30) return QImage();
        QImage img(40, 30, QImage::Format_Grayscale8);
        img.fill(128);
        *size = img.size();
        return img;
    }
};

int main(int argc, char** argv) {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    if (argc < 3) {
        std::fprintf(stderr, "usage: test_qml_pages <driver.qml> <page.qml>\n");
        return 2;
    }
    QQmlEngine engine;
    QObject::connect(&engine, &QQmlEngine::exit, &app, &QCoreApplication::exit);  // Qt.exit(code) in the driver
    int warnings = 0;
    QObject::connect(&engine, &QQmlEngine::warnings, [&](const QList<QQmlError>& list) {
        for (const QQmlError& e : list) {
            // a preview the provider cannot make (spec 009: ids without a picture) is an expected, handled state
            if (e.description().contains(QLatin1String("Failed to get image from provider: image://filepreview/"))) continue;
            std::fprintf(stderr, "QML: %s\n", e.toString().toUtf8().constData());
            ++warnings;
        }
    });

    engine.addImageProvider(QStringLiteral("filepreview"), new StubPreviewProvider);   // the engine owns it

    ClipboardBridge clipboard;
    EditSession editSession;
    SlashBridge slash;
    // Stubs of the objects the pages talk to (the real NotesBridge needs a store and a server).
    QQmlComponent stubs(&engine, QUrl::fromLocalFile(QStringLiteral(PBNOTES_QML_TEST_DIR "/stubs.qml")));
    QObject* stub = stubs.create();
    if (!stub) {
        std::fprintf(stderr, "stubs: %s\n", stubs.errorString().toUtf8().constData());
        return 2;
    }
    QQmlContext* ctx = engine.rootContext();
    ctx->setContextProperty(QStringLiteral("bridge"), stub->property("bridge").value<QObject*>());
    ctx->setContextProperty(QStringLiteral("attachments"), stub->property("attachments").value<QObject*>());
    ctx->setContextProperty(QStringLiteral("device"), stub->property("device").value<QObject*>());
    ctx->setContextProperty(QStringLiteral("about"), stub->property("about").value<QObject*>());
    ctx->setContextProperty(QStringLiteral("clipboard"), &clipboard);
    ctx->setContextProperty(QStringLiteral("editSession"), &editSession);
    ctx->setContextProperty(QStringLiteral("slash"), &slash);
    Pointer pointer;
    ctx->setContextProperty(QStringLiteral("pointer"), &pointer);
    ctx->setContextProperty(QStringLiteral("pageUrl"), QUrl::fromLocalFile(QString::fromLocal8Bit(argv[2])));
    ctx->setContextProperty(QStringLiteral("srcDir"), QStringLiteral(PBNOTES_SOURCE_DIR));
    for (int i = 3; i < argc; ++i) {
        const QString kv = QString::fromLocal8Bit(argv[i]);
        const int eq = kv.indexOf(QLatin1Char('='));
        if (eq > 0) ctx->setContextProperty(QStringLiteral("arg_") + kv.left(eq), kv.mid(eq + 1));
    }

    QQmlComponent driver(&engine, QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])));
    QObject* root = driver.create();
    if (!root) {
        std::fprintf(stderr, "driver: %s\n", driver.errorString().toUtf8().constData());
        return 2;
    }
    pointer.window = qobject_cast<QWindow*>(root);
    const int rc = app.exec();
    delete root;
    if (warnings > 0) {
        std::fprintf(stderr, "%d QML warning(s)\n", warnings);
        return rc ? rc : 3;
    }
    return rc;
}

#include "test_qml_pages.moc"
