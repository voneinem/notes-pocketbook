// pbnotes: Nextcloud Notes client for the PocketBook InkPad One. Pattern adapted from InkFeed (see NOTICE).

#include <sys/stat.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QDir>
#include <QImage>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QStringList>
#include <QStyleHints>
#include <QTimer>
#include <iostream>

#include "device_bridge.h"
#include "notes_bridge.h"

#ifdef PLATFORM_POCKETBOOK
#include <inkview.h>
#endif

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("pbnotes"));
    const bool self_test = app.arguments().contains(QStringLiteral("--self-test"));

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
    NotesBridge notes(config);
    if (!notes.ok()) std::cerr << "[pbnotes] the database could not be opened" << std::endl;

    DeviceBridge device;
    device.setRotationPreference(notes.rotationPreference());
    QObject::connect(&notes, &NotesBridge::rotationPreferenceChanged, &device,
                     [&]() { device.setRotationPreference(notes.rotationPreference()); });
    app.installEventFilter(&device);  // Back / Escape

    QQmlApplicationEngine engine;
    int qml_warnings = 0;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app, [&](const QList<QQmlError>& list) {
        for (const QQmlError& e : list) std::cerr << "[pbnotes] QML: " << e.toString().toStdString() << std::endl;
        qml_warnings += list.size();
    });
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &notes);
    engine.rootContext()->setContextProperty(QStringLiteral("device"), &device);
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
