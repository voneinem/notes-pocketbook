#include "device_bridge.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QInputMethod>
#include <QKeyEvent>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>
#include <QUrl>

#include "pbnotes/attachment_ref.h"
#include "pbnotes/links.h"
#include "pbnotes/log.h"

#ifdef PLATFORM_POCKETBOOK
#include <inkview.h>

#include <qpa/qplatformscreen.h>  // private QPA header: only to reach the platform screen as a QObject
#endif

DeviceBridge::DeviceBridge(QObject* parent) : QObject(parent) {
#ifdef PLATFORM_POCKETBOOK
    startOrientation_ = GetOrientation();
    applied_ = startOrientation_;
    SetGSensorEnabled(true);
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &DeviceBridge::poll);
    timer_->start(500);
#endif
}

void DeviceBridge::setRotationPreference(const QString& preference) {
    if (preference != QLatin1String("auto") && preference != QLatin1String("portrait") &&
        preference != QLatin1String("landscape"))
        return;
    preference_ = preference;
    poll();
}

void DeviceBridge::poll() {
#ifdef PLATFORM_POCKETBOOK
    if (rotating_) return;
    int target;
    if (preference_ == QLatin1String("portrait")) target = 0;
    else if (preference_ == QLatin1String("landscape")) target = 1;
    else target = GetGSensorOrientation();
    if (target != applied_) rotateTo(target);
#endif
}

void DeviceBridge::rotateTo(int n) {
#ifdef PLATFORM_POCKETBOOK
    rotating_ = true;
    QInputMethod* im = QGuiApplication::inputMethod();
    const bool reopen_keyboard = im && im->isVisible();
    if (reopen_keyboard) im->hide();  // otherwise the keyboard stays in the old orientation
    pbnotes::log::event("rotate.begin", {pbnotes::log::count("orientation", n), pbnotes::log::count("keyboard_open", reopen_keyboard)});
    QTimer::singleShot(reopen_keyboard ? 350 : 0, this, [this, n, reopen_keyboard]() {
        SetOrientation(n);
        applied_ = n;
        QTimer::singleShot(400, this, [this, reopen_keyboard]() {  // let the system settle
            updateQtGeometry();
            if (window_) window_->update();
            emit orientationChanged();
            QTimer::singleShot(reopen_keyboard ? 500 : 0, this, [this, reopen_keyboard]() {
                if (reopen_keyboard) QGuiApplication::inputMethod()->show();
                rotating_ = false;
                pbnotes::log::event("rotate.done", {pbnotes::log::count("orientation", applied_)});
            });
        });
    });
#else
    Q_UNUSED(n);
#endif
}

// Tells Qt's platform screen (EinkPlatformScreen, a QObject) to re-read the screen size, so the
// window gets the landscape or portrait size. Without this Qt keeps the old window size.
void DeviceBridge::updateQtGeometry() {
#ifdef PLATFORM_POCKETBOOK
    QScreen* screen = QGuiApplication::primaryScreen();
    QObject* platform_screen = screen ? dynamic_cast<QObject*>(screen->handle()) : nullptr;
    if (!platform_screen || !QMetaObject::invokeMethod(platform_screen, "updateGeometry", Qt::DirectConnection))
        pbnotes::log::event("rotate.geometry_failed", {});
#endif
}

#ifdef PLATFORM_POCKETBOOK
namespace {
DeviceBridge* g_bridge = nullptr;

int net_connect_done(int status) {  // InkView callback, possibly on another thread
    if (g_bridge)
        QMetaObject::invokeMethod(g_bridge, [status]() { g_bridge->finishConnect(status == NET_OK); },
                                  Qt::QueuedConnection);
    return 0;
}
}  // namespace
#endif

void DeviceBridge::finishConnect(bool ok) {
    if (!connecting_) return;
    connecting_ = false;
    pbnotes::log::event("network.connect", {pbnotes::log::text("result", ok ? "connected" : "failed")});
    emit networkResult(ok && wifiConnected());
}

void DeviceBridge::connectNetwork() {
    if (wifiConnected()) {
        emit networkResult(true);
        return;
    }
#ifdef PLATFORM_POCKETBOOK
    if (connecting_) return;
    connecting_ = true;
    g_bridge = this;
    QTimer::singleShot(60000, this, [this]() { finishConnect(false); });
    NetConnectAsync(&net_connect_done);
#else
    emit networkResult(false);
#endif
}

bool DeviceBridge::wifiConnected() const {
#ifdef PLATFORM_POCKETBOOK
    return (QueryNetwork() & NET_CONNECTED) != 0;  // 0x203 = connected, 0x003 = Wi-Fi on but not connected
#else
    return true;
#endif
}

bool DeviceBridge::openUrl(const QString& url) {
    const QByteArray bytes = url.toUtf8();
    if (!pbnotes::is_followable_url(std::string(bytes.constData(), static_cast<size_t>(bytes.size())))) {
        pbnotes::log::event("link.open", {pbnotes::log::text("result", "rejected")});
        return false;
    }
#ifdef PLATFORM_POCKETBOOK
    // The browser is started the way InkFeed does it (see NOTICE); the app stays alive in the background.
    OpenBook(BROWSER_FOR_AUTH, bytes.constData(), 0);
    const bool ok = true;
#else
    const bool ok = QDesktopServices::openUrl(QUrl::fromEncoded(bytes));
#endif
    pbnotes::log::event("link.open", {pbnotes::log::text("result", ok ? "started" : "failed")});
    return ok;
}

bool DeviceBridge::openFile(const QString& path) {
    const QFileInfo info(path);
    const QString dir = QFileInfo(attachmentsDir_).canonicalFilePath();
    const QString file = info.canonicalFilePath();  // resolves links and ".."; empty if the file does not exist
    const bool inside = !dir.isEmpty() && !file.isEmpty() && file.startsWith(dir + QLatin1Char('/'));
    const bool viewable = pbnotes::is_viewable_extension(pbnotes::safe_extension(file.toStdString()));
    if (!inside || !viewable || !QFileInfo(file).isFile()) {
        pbnotes::log::event("file.open", {pbnotes::log::text("result", "rejected")});
        return false;
    }
#ifdef PLATFORM_POCKETBOOK
    OpenBook(file.toUtf8().constData(), "", 0);  // the reader application picks the viewer by file type
    const bool ok = true;
#else
    const bool ok = QDesktopServices::openUrl(QUrl::fromLocalFile(file));
#endif
    pbnotes::log::event("file.open", {pbnotes::log::text("result", ok ? "started" : "failed")});
    return ok;
}

void DeviceBridge::quit() {
#ifdef PLATFORM_POCKETBOOK
    SetOrientation(startOrientation_);  // never leave the reader in a changed orientation
    // Exiting a fullscreen EGL app otherwise leaves a black framebuffer until the launcher repaints:
    // whiten the InkView canvas, then hand back through CloseApp() (adapted from InkFeed).
    ClearScreen();
    FullUpdate();
    CloseApp();
#endif
    QCoreApplication::quit();
}

bool DeviceBridge::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent*>(event)->key();
        if (key == Qt::Key_Back || key == Qt::Key_Escape) {
            emit backRequested();
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}
