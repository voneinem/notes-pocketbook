#pragma once
// Device glue: screen orientation, the hardware/Escape Back key, and leaving the app cleanly.
//
// Rotation design (measured on the InkPad One, research R-10/R-15): on a sensor change the app calls
// InkView SetOrientation(n) and then the Qt platform screen's updateGeometry(). The system then
// rotates the picture, the status bar and the on-screen keyboard, and Qt resizes its window. The UI
// itself never rotates its content. An open keyboard is closed first and reopened afterwards,
// otherwise it stays in the old orientation. On the desktop build all of this is a no-op.

#include <QObject>
#include <QString>

class QQuickWindow;
class QTimer;

class DeviceBridge : public QObject {
    Q_OBJECT
    // InkView orientation currently applied (0 portrait, 1 and 2 landscape, 3 portrait upside down)
    Q_PROPERTY(int orientation READ orientation NOTIFY orientationChanged)

public:
    explicit DeviceBridge(QObject* parent = nullptr);

    void setWindow(QQuickWindow* window) { window_ = window; }
    int orientation() const { return applied_; }

    // "auto" follows the sensor, "portrait" and "landscape" lock the orientation.
    void setRotationPreference(const QString& preference);

    // True if the reader reports a usable network connection (Wi-Fi up). Always true on the desktop.
    Q_INVOKABLE bool wifiConnected() const;

    // Asks the system to bring Wi-Fi up (it may show its own connect dialog) without blocking the UI.
    // networkResult(true) follows when the reader is online, networkResult(false) if it did not work
    // out (also after a 60 s safety timeout). On the desktop it reports success at once.
    Q_INVOKABLE void connectNetwork();
    void finishConnect(bool ok);  // internal: result of the system's connect attempt

    // Opens a web address in the system's default browser. Re-validates the address (http/https only, no
    // blanks or control characters, at most 2,048 characters) and returns false if it is rejected or
    // no browser could be started. Device: OpenBook(BROWSER_FOR_AUTH, ...); desktop: QDesktopServices.
    Q_INVOKABLE bool openUrl(const QString& url);

    // Opens a stored attachment in the reader application's viewer. Only a regular file inside the attachment
    // folder (see setAttachmentsDirectory) whose extension the viewer handles (pdf, epub, fb2, txt, rtf, doc, docx,
    // html, htm, djvu, mobi, cbz, cbr) is passed on. False otherwise. Device: OpenBook(path, "", 0); desktop:
    // QDesktopServices.
    Q_INVOKABLE bool openFile(const QString& path);
    void setAttachmentsDirectory(const QString& directory) { attachmentsDir_ = directory; }

    // Leaves the app cleanly (device: whiten the panel and hand back to the launcher).
    Q_INVOKABLE void quit();

signals:
    void orientationChanged();
    void networkResult(bool connected);
    // Back or Escape was pressed (the reader has no hardware Back button; used on the desktop).
    void backRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void poll();
    void rotateTo(int inkviewOrientation);
    void updateQtGeometry();

    QString attachmentsDir_;
    QQuickWindow* window_ = nullptr;
    QTimer* timer_ = nullptr;
    QString preference_ = QStringLiteral("auto");
    int applied_ = 0;       // orientation the system is in
    int startOrientation_ = 0;
    bool rotating_ = false;
    bool connecting_ = false;
};
