// pbnotes: Nextcloud Notes client for the PocketBook InkPad One. Pattern adapted from InkFeed (see NOTICE).

#include <sys/stat.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QImage>
#include <QImageReader>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QStringList>
#include <QStyleHints>
#include <QTimer>
#include <iostream>

#include "device_bridge.h"
#include "attachment_image_provider.h"
#include "about_bridge.h"
#include "edit_bridge.h"
#include "file_preview_provider.h"
#include "attachments_bridge.h"
#include "notes_bridge.h"
#include "pbnotes/attachment_service.h"
#include "pbnotes/log.h"
#include "pbnotes/version.h"

#ifdef PLATFORM_POCKETBOOK
#include <inkview.h>
#endif

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("pbnotes"));
    const bool self_test = app.arguments().contains(QStringLiteral("--self-test"));

    // Emoji: the monochrome Noto Emoji font (SIL OFL, see NOTICE) is added behind the default font, so
    // every Text/TextField/TextArea shows emojis without a per-item setting. A missing or unreadable font
    // is not an error: the device font is used as before (spec 002 FR-015).
#ifdef PLATFORM_POCKETBOOK
    const QString emoji_font = QStringLiteral("/mnt/ext1/applications/pbnotes-emoji.ttf");
#else
    const QString emoji_font = qEnvironmentVariable("PBNOTES_EMOJI_FONT", QStringLiteral(PBNOTES_EMOJI_FONT));
#endif
    const int emoji_id = QFontDatabase::addApplicationFont(emoji_font);
    const QStringList emoji_families = emoji_id >= 0 ? QFontDatabase::applicationFontFamilies(emoji_id) : QStringList();
    // The user interface typeface (spec 010 R-5): the reader's own apps are set in Roboto, so use it when the font
    // database has it, otherwise the default font as before.
    const bool roboto = QFontDatabase::families().contains(QStringLiteral("Roboto"));
    // the family that is really used (the default may be a generic name such as "Sans Serif", which is
    // not matched by name once a family list is set)
    const QString ui_family = roboto ? QStringLiteral("Roboto") : QFontInfo(app.font()).family();
    if (!emoji_families.isEmpty()) {
        QFont font = app.font();
        font.setFamilies({ui_family, emoji_families.first()});
        app.setFont(font);
    } else {
        pbnotes::log::event("font.emoji_missing", {});
        if (roboto) {
            QFont font = app.font();
            font.setFamilies({ui_family});
            app.setFont(font);
        }
    }

#ifdef PLATFORM_POCKETBOOK
    // PocketBook convention: app data lives under USERDATA (/mnt/ext1/system).
    const QString data_dir = QString::fromLatin1(USERDATA) + QStringLiteral("/pbnotes");
    const QString bundled_ca = QStringLiteral("/mnt/ext1/applications/pbnotes-cacert.pem");
#else
    const QString data_dir = QStringLiteral("./pbnotes-data");
    const QString bundled_ca = qEnvironmentVariable("PBNOTES_CA", QStringLiteral(PBNOTES_BUNDLED_CA));
#endif
    mkdir(data_dir.toStdString().c_str(), 0755);

    // A blinking caret makes the e-ink panel refresh periodically; keep it static.
    app.styleHints()->setCursorFlashTime(0);

    NotesBridge::Config config;
    config.dataDir = data_dir;
    config.bundledCaPath = bundled_ca;
    config.deviceCaCandidates = {QStringLiteral("/etc/ssl/certs/ca-certificates.crt"), QStringLiteral("/etc/ssl/cert.pem")};
    config.attachmentsDir = data_dir + QStringLiteral("/attachments");
    mkdir(config.attachmentsDir.toStdString().c_str(), 0755);
    // Auto Sync asks whether Wi-Fi is up before it starts a sync. The device bridge is created after the notes bridge
    // (it reads the rotation preference), so the callback reaches it through a pointer.
    DeviceBridge* wifiProbe = nullptr;
    config.wifiConnected = [&wifiProbe]() { return wifiProbe != nullptr && wifiProbe->wifiConnected(); };
    NotesBridge notes(config);
    if (!notes.ok()) std::cerr << "[pbnotes] the database could not be opened" << std::endl;

    // The janitor at start: files no attachment row names (a crash can leave some) and half-written ones go.
    if (notes.store()) pbnotes::AttachmentService::purge(*notes.store(), config.attachmentsDir.toStdString());
    {
        QStringList formats;  // which picture formats this Qt can decode (format names only)
        for (const QByteArray& f : QImageReader::supportedImageFormats()) formats << QString::fromLatin1(f);
        pbnotes::log::event("image.formats", {pbnotes::log::text("list", formats.join(QLatin1Char(',')).toStdString())});
    }
    // which typeface the interface uses (a family name and a flag only; read on the device, spec 010 R-5)
    pbnotes::log::event("font.ui", {pbnotes::log::text("family", ui_family.toStdString()), pbnotes::log::count("roboto", roboto ? 1 : 0)});

    DeviceBridge device;
    wifiProbe = &device;
    // With Wi-Fi off (the reader's usual state) Auto Sync asks the device to connect, as "Sync now" does.
    notes.setWifiRequest([&device]() { device.connectNetwork(); });
    QObject::connect(&device, &DeviceBridge::networkResult, &notes, &NotesBridge::networkAnswered);
    device.setRotationPreference(notes.rotationPreference());
    QObject::connect(&notes, &NotesBridge::rotationPreferenceChanged, &device,
                     [&]() { device.setRotationPreference(notes.rotationPreference()); });
    device.setAttachmentsDirectory(config.attachmentsDir);
    app.installEventFilter(&device);  // Back / Escape

    AttachmentsBridge::Config attachment_config;
#ifdef PLATFORM_POCKETBOOK
    attachment_config.rootDirectory = QStringLiteral("/mnt/ext1");
#else
    attachment_config.rootDirectory = QDir::homePath();
#endif
    attachment_config.topFolders = AttachmentsBridge::defaultTopFolders();   // the file picker's top level (spec 009)
    attachment_config.openFile = [&device](const QString& path) { return device.openFile(path); };
    attachment_config.wifiConnected = [&device]() { return device.wifiConnected(); };
    AttachmentsBridge attachments(&notes, attachment_config);

    // About screens: the manifest and the license texts live in pbnotes-licenses/ next to the app on the reader,
    // and in the repository on the host (PBNOTES_LICENSES_ROOT overrides both).
    AboutBridge::Config about_config;
#ifdef PLATFORM_POCKETBOOK
    about_config.licensesRoot = qEnvironmentVariable("PBNOTES_LICENSES_ROOT", QStringLiteral("/mnt/ext1/applications/pbnotes-licenses"));
#else
    about_config.licensesRoot = qEnvironmentVariable("PBNOTES_LICENSES_ROOT", QStringLiteral(PBNOTES_LICENSES_ROOT));
#endif
    about_config.version = QString::fromUtf8(pbnotes::version());
    AboutBridge about(about_config);
    ClipboardBridge clipboard;
    EditSession editSession;
    SlashBridge slash;

    QQmlApplicationEngine engine;
    int qml_warnings = 0;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app, [&](const QList<QQmlError>& list) {
        for (const QQmlError& e : list) std::cerr << "[pbnotes] QML: " << e.toString().toStdString() << std::endl;
        qml_warnings += list.size();
    });
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &notes);
    engine.rootContext()->setContextProperty(QStringLiteral("device"), &device);
    engine.rootContext()->setContextProperty(QStringLiteral("attachments"), &attachments);
    engine.rootContext()->setContextProperty(QStringLiteral("about"), &about);
    engine.rootContext()->setContextProperty(QStringLiteral("clipboard"), &clipboard);
    engine.rootContext()->setContextProperty(QStringLiteral("editSession"), &editSession);
    engine.rootContext()->setContextProperty(QStringLiteral("slash"), &slash);
    // Previews of the image files the file browser lists (spec 009); the shared index keeps its ids alive for the engine.
    engine.addImageProvider(QStringLiteral("filepreview"), new FilePreviewProvider(attachments.previewIndex()));
    if (notes.store())  // the engine owns the provider
        engine.addImageProvider(QStringLiteral("attachment"), new AttachmentImageProvider(notes.store(), config.attachmentsDir));
    // Desktop-only start options (PBNOTES_START_PAGE / PBNOTES_START_NOTE); both are always defined,
    // empty/0 on the device, so the QML never depends on the platform.
    QString start_page;
    int start_note = 0;
#ifndef PLATFORM_POCKETBOOK
    start_page = qEnvironmentVariable("PBNOTES_START_PAGE");
    start_note = qEnvironmentVariableIntValue("PBNOTES_START_NOTE");
#endif
    engine.rootContext()->setContextProperty(QStringLiteral("startPage"), start_page);
    engine.rootContext()->setContextProperty(QStringLiteral("startNote"), start_note);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        std::cerr << "[pbnotes] failed to load QML" << std::endl;
        return 1;
    }
    if (auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first())) device.setWindow(window);

    // Desktop verification: PBNOTES_QML_SHOT=<file.png> renders the window once and quits.
    const QByteArray shot = qgetenv("PBNOTES_QML_SHOT");
    if (!shot.isEmpty()) {
        if (auto* w = qobject_cast<QQuickWindow*>(engine.rootObjects().first())) {
            w->resize(qEnvironmentVariableIntValue("PBNOTES_SHOT_W") ? qEnvironmentVariableIntValue("PBNOTES_SHOT_W") : 1080,
                      qEnvironmentVariableIntValue("PBNOTES_SHOT_H") ? qEnvironmentVariableIntValue("PBNOTES_SHOT_H") : 1335);
            QTimer::singleShot(900, &app, [w, shot, &app]() {
                w->grabWindow().save(QString::fromLocal8Bit(shot));
                app.quit();
            });
        }
    }

    if (self_test) {  // CTest: every page compiles and instantiates without QML errors
        QTimer::singleShot(300, &app, [&]() {
            int failed = 0;
            for (const QString& name : QDir(QStringLiteral(":/qml/pages")).entryList(QStringList() << QStringLiteral("*.qml"))) {
                QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/pages/") + name));
                QObject* page = component.isError() ? nullptr : component.create();
                if (!page) {
                    std::cerr << "[pbnotes] page failed: " << name.toStdString() << " " << component.errorString().toStdString() << std::endl;
                    ++failed;
                }
                delete page;
            }
            // a blank screen (no page shown) is a failure too
            auto* window = engine.rootObjects().isEmpty() ? nullptr : engine.rootObjects().first();
            const bool shown = window && window->property("pageLoaded").toBool();
            if (!shown) std::cerr << "[pbnotes] no page is shown" << std::endl;
            QTimer::singleShot(300, &app, [&, failed, shown]() { app.exit((failed == 0 && shown && qml_warnings == 0) ? 0 : 2); });
        });
    }
    return app.exec();
}
