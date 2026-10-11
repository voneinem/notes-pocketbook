#include "about_bridge.h"

#include "pbnotes/log.h"

namespace {
// Shown when the manifest has no app block; the name is part of the build, not of the data.
const char* kFallbackName = "Notes Pocketbook";
}  // namespace

AboutBridge::AboutBridge(Config config, QObject* parent) : QObject(parent), config_(std::move(config)) {}

const pbnotes::about::Manifest& AboutBridge::manifest() const {
    if (!loaded_) {
        manifest_ = pbnotes::about::load_manifest(config_.licensesRoot.toStdString());
        loaded_ = true;
        pbnotes::log::event("about.opened", {});
    }
    return manifest_;
}

QString AboutBridge::appName() const {
    const auto& m = manifest();
    return m.app ? QString::fromStdString(m.app->name) : QString::fromUtf8(kFallbackName);
}

QString AboutBridge::version() const {
    return QString::fromStdString(pbnotes::about::display_version(config_.version.toStdString()));
}

QString AboutBridge::author() const {
    const auto& m = manifest();
    return m.app ? QString::fromStdString(m.app->holder) : QString();
}

QString AboutBridge::copyright() const {
    const auto& m = manifest();
    return m.app ? QString::fromStdString(m.app->copyright) : QString();
}

QString AboutBridge::licenseName() const {
    const auto& m = manifest();
    return m.app ? QString::fromStdString(m.app->license) : QString();
}

QVariantMap AboutBridge::pageFor(const pbnotes::about::Entry& entry, const QStringList& header) const {
    QVariantMap page;
    page.insert(QStringLiteral("title"), QString::fromStdString(entry.name));
    page.insert(QStringLiteral("header"), header);
    if (entry.text.empty()) {
        // No text exists or is distributed (public domain, proprietary): point to where it is published.
        page.insert(QStringLiteral("available"), true);
        page.insert(QStringLiteral("paragraphs"),
                    QStringList{QStringLiteral("The full license text is published at %1.").arg(QString::fromStdString(entry.url))});
        return page;
    }
    const pbnotes::about::LicenseText text = pbnotes::about::load_text(config_.licensesRoot.toStdString(), entry);
    QStringList paragraphs;
    for (const std::string& p : text.paragraphs) paragraphs << QString::fromStdString(p);
    if (!text.available) paragraphs = QStringList{QStringLiteral("The license text could not be read.")};
    page.insert(QStringLiteral("available"), text.available);
    page.insert(QStringLiteral("paragraphs"), paragraphs);
    return page;
}

QVariantMap AboutBridge::appLicense() const {
    const auto& m = manifest();
    if (!m.app) {
        QVariantMap page;
        page.insert(QStringLiteral("title"), appName());
        page.insert(QStringLiteral("header"), QStringList());
        page.insert(QStringLiteral("available"), false);
        page.insert(QStringLiteral("paragraphs"), QStringList{QStringLiteral("The license text could not be read.")});
        return page;
    }
    QStringList header;
    header << QStringLiteral("Holder: %1").arg(QString::fromStdString(m.app->holder))
           << QStringLiteral("License: %1").arg(QString::fromStdString(m.app->license));
    return pageFor(*m.app, header);
}

QVariantList AboutBridge::components() const {
    QVariantList list;
    using pbnotes::about::Origin;
    for (Origin origin : {Origin::Bundled, Origin::System}) {
        for (const auto& c : manifest().components) {
            if (c.origin != origin) continue;
            QVariantMap row;
            row.insert(QStringLiteral("id"), QString::fromStdString(c.id));
            row.insert(QStringLiteral("name"), QString::fromStdString(c.name));
            row.insert(QStringLiteral("purpose"), QString::fromStdString(c.purpose));
            row.insert(QStringLiteral("license"), QString::fromStdString(c.license));
            row.insert(QStringLiteral("origin"), origin == Origin::Bundled ? QStringLiteral("bundled") : QStringLiteral("system"));
            list << row;
        }
    }
    return list;
}

int AboutBridge::unreadableEntries() const { return manifest().bad_blocks; }

QVariantMap AboutBridge::componentDetail(const QString& id) const {
    for (const auto& c : manifest().components) {
        if (QString::fromStdString(c.id) != id) continue;
        QStringList header;
        header << QStringLiteral("Holder: %1").arg(QString::fromStdString(c.holder))
               << QStringLiteral("License: %1").arg(QString::fromStdString(c.license))
               << QStringLiteral("Origin: %1").arg(c.origin == pbnotes::about::Origin::Bundled ? QStringLiteral("In the app package")
                                                                                             : QStringLiteral("Provided by the reader"))
               << QStringLiteral("Address: %1").arg(QString::fromStdString(c.url))
               << QStringLiteral("Purpose: %1").arg(QString::fromStdString(c.purpose));
        return pageFor(c, header);
    }
    return {};
}

bool AboutBridge::infoAvailable() const { return manifest().app.has_value(); }
