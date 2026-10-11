#include "notes_bridge.h"
#include "reader_blocks.h"

#include <cstdio>
#include <ctime>
#include <cstring>

#include <QDir>
#include <QMetaObject>

#include "pbnotes/attachment_service.h"
#include "pbnotes/ca_selector.h"
#include "pbnotes/connection_test.h"
#include "pbnotes/markdown.h"
#include "pbnotes/paging.h"
#include "pbnotes/sync_engine.h"
#include "pbnotes/sync_status.h"
#include "pbnotes/curl_transport.h"
#include "pbnotes/log.h"

namespace {

QString text_for(const QString& key) {
    static const struct {
        const char* key;
        const char* text;
    } kTexts[] = {
        {"ok", "The connection works."},
        {"url_empty", "Enter the address of your Nextcloud server."},
        {"url_not_https", "The address must start with https:// because a secure connection is required."},
        {"url_invalid", "The address is not valid."},
        {"credentials_missing", "Enter your user name and an app password."},
        {"auth_rejected", "The server rejected the user name or the app password."},
        {"wifi_off", "Wi-Fi is not connected. Turn on Wi-Fi on the reader and try again."},
        {"server_unreachable", "The server could not be reached. Check the address and the Wi-Fi connection."},
        {"certificate_unverifiable",
         "The server's certificate could not be verified, so nothing was sent. If your server uses a private "
         "certificate authority, see Settings."},
        {"server_unsupported", "This server does not offer the Notes app (API version 1)."},
        {"server_maintenance", "The server is in maintenance mode. Try again later."},
        {"server_error", "The server gave an unexpected answer."},
        {"pending_changes", "Some changes have not been synced yet. Sync first so nothing is lost."},
        {"account_exists", "Another account is already set up. Forget it first."},
        {"storage_error", "The notes could not be saved. The storage may be full."},
        {"content_empty", "The note is empty. Write something before saving."},
        {"category_too_long", "The category is too long (at most 255 characters)."},
        {"category_spaces", "The category must not start or end with a space."},
        {"category_control_chars", "The category contains characters that are not allowed."},
        {"category_invalid", "The category contains characters that are not allowed."},
        {"not_found", "This note no longer exists."},
    };
    for (const auto& t : kTexts)
        if (key == QLatin1String(t.key)) return QString::fromUtf8(t.text);
    return key;
}

}  // namespace

NotesBridge::NotesBridge(const Config& config, QObject* parent) : QObject(parent), config_(config) {
    QDir().mkpath(config_.dataDir);
    std::string error;
    store_ = pbnotes::NoteStore::open((config_.dataDir + QStringLiteral("/pbnotes.db")).toStdString(), &error);
    if (!store_) pbnotes::log::event("store.open_failed", {pbnotes::log::text("detail", error)});
    refreshCa();
}

NotesBridge::~NotesBridge() {
    if (worker_.joinable()) worker_.join();
    if (syncWorker_.joinable()) syncWorker_.join();
}

void NotesBridge::refreshCa() {
    std::vector<std::string> candidates;
    for (const QString& c : config_.deviceCaCandidates) candidates.push_back(c.toStdString());
    const pbnotes::CaSelection base = pbnotes::select_ca_file(candidates, config_.bundledCaPath.toStdString());
    const pbnotes::EffectiveCa eff = pbnotes::build_effective_ca(
        base.path, customCaPath().toStdString(), (config_.dataDir + QStringLiteral("/effective-ca.pem")).toStdString());

    const QString source = base.source == pbnotes::CaSource::Device ? QStringLiteral("device") : QStringLiteral("bundled");
    QString status = QStringLiteral("none");
    if (eff.custom_status == pbnotes::CustomCaStatus::Loaded) status = QStringLiteral("loaded");
    else if (eff.custom_status == pbnotes::CustomCaStatus::Invalid) status = QStringLiteral("invalid");

    const bool changed = source != caSource_ || status != customCaStatus_ || eff.custom_cert_count != customCaCertificates_;
    caPath_ = QString::fromStdString(eff.path);
    caSource_ = source;
    customCaStatus_ = status;
    customCaCertificates_ = eff.custom_cert_count;
    pbnotes::log::event("ca.selected", {pbnotes::log::text("source", source.toStdString()),
                                        pbnotes::log::text("custom", status.toStdString()),
                                        pbnotes::log::count("custom_certs", eff.custom_cert_count)});
    if (changed) emit caChanged();
}

QString NotesBridge::attachmentsDir() const {
    return config_.attachmentsDir.isEmpty() ? config_.dataDir + QStringLiteral("/attachments") : config_.attachmentsDir;
}

bool NotesBridge::configured() const { return store_ && store_->get_account().has_value(); }

QString NotesBridge::serverUrl() const {
    auto a = store_ ? store_->get_account() : std::nullopt;
    return a ? QString::fromStdString(a->server_url) : QString();
}

QString NotesBridge::username() const {
    auto a = store_ ? store_->get_account() : std::nullopt;
    return a ? QString::fromStdString(a->username) : QString();
}

QString NotesBridge::rotationPreference() const {
    return store_ ? QString::fromStdString(store_->get_setting("rotation")) : QStringLiteral("auto");
}

void NotesBridge::setRotationPreference(const QString& preference) {
    if (!store_ || preference == rotationPreference()) return;
    if (store_->set_setting("rotation", preference.toStdString())) emit rotationPreferenceChanged();
}

QString NotesBridge::message(const QString& key) const { return text_for(key); }

void NotesBridge::setBusy(bool busy) {
    if (busy_.exchange(busy) != busy) emit busyChanged();
}

void NotesBridge::saveDraft(const QString& url, const QString& user, const QString& password) {
    if (store_) store_->save_setup_draft({url.toStdString(), user.toStdString(), password.toStdString()});
}

QVariantMap NotesBridge::draft() const {
    QVariantMap out;
    if (!store_) return out;
    if (auto d = store_->get_setup_draft()) {
        out.insert(QStringLiteral("url"), QString::fromStdString(d->url));
        out.insert(QStringLiteral("username"), QString::fromStdString(d->username));
        out.insert(QStringLiteral("password"), QString::fromStdString(d->password));
    }
    return out;
}

void NotesBridge::testConnection(const QString& url, const QString& user, const QString& password) {
    if (busy_) return;
    saveDraft(url, user, password);
    refreshCa();

    // Input errors are answered at once, without any network traffic.
    const pbnotes::UrlCheck check = pbnotes::normalize_server_url(url.toStdString());
    QString early;
    if (!check.ok) early = QString::fromStdString(check.error_key);
    else if (user.trimmed().isEmpty() || password.isEmpty()) early = QStringLiteral("credentials_missing");
    if (!early.isEmpty()) {
        QMetaObject::invokeMethod(this, [this, early]() { emit connectionTested(false, early, text_for(early), QString()); },
                                  Qt::QueuedConnection);
        return;
    }

    if (worker_.joinable()) worker_.join();
    setBusy(true);
    const std::string ca = caPath_.toStdString();
    const pbnotes::CaSource source = caSource_ == QLatin1String("device") ? pbnotes::CaSource::Device : pbnotes::CaSource::Bundled;
    const std::string norm = check.normalized, u = user.trimmed().toStdString(), p = password.toStdString();
    worker_ = std::thread([this, ca, source, norm, u, p]() {
        pbnotes::CurlTransport transport(ca);
        pbnotes::ConnectionTester tester(transport, source);
        const pbnotes::ConnectionTestResult r = tester.test(norm, u, p);
        const QString key = QString::fromStdString(r.message_key);
        const QString normalized = QString::fromStdString(r.normalized_url);
        pbnotes::log::event("connection.tested", {pbnotes::log::text("result", r.message_key),
                                                  pbnotes::log::text("error_class", pbnotes::to_string(r.error_class)),
                                                  pbnotes::log::text("detail", r.detail)});
        if (r.error_class == pbnotes::ErrorClass::Network) {
            // A failed name lookup on the device: record how the resolver is configured (counts only).
            int servers = 0;
            bool exists = false;
            if (FILE* f = std::fopen("/etc/resolv.conf", "r")) {
                exists = true;
                char line[256];
                while (std::fgets(line, sizeof line, f))
                    if (std::strncmp(line, "nameserver", 10) == 0) ++servers;
                std::fclose(f);
            }
            pbnotes::log::event("network.diag", {pbnotes::log::text("resolv_conf", exists ? "present" : "missing"),
                                                 pbnotes::log::count("nameservers", servers)});
        }
        QString shown = text_for(key);
        if (!r.ok && !r.detail.empty()) shown += QStringLiteral(" (") + QString::fromStdString(r.detail) + QLatin1Char(')');
        QMetaObject::invokeMethod(this, [this, r, key, normalized, shown]() {
            setBusy(false);
            emit connectionTested(r.ok, key, shown, normalized);
        }, Qt::QueuedConnection);
    });
}

bool NotesBridge::saveAccount(const QString& url, const QString& user, const QString& password) {
    auto fail = [this](const QString& key) {
        emit errorOccurred(key, text_for(key));
        return false;
    };
    if (!store_) return fail(QStringLiteral("storage_error"));
    const pbnotes::UrlCheck check = pbnotes::normalize_server_url(url.toStdString());
    if (!check.ok) return fail(QString::fromStdString(check.error_key));
    if (user.trimmed().isEmpty() || password.isEmpty()) return fail(QStringLiteral("credentials_missing"));

    pbnotes::Account account;
    if (auto existing = store_->get_account()) {
        if (existing->server_url != check.normalized || existing->username != user.trimmed().toStdString())
            return fail(QStringLiteral("account_exists"));
        account = *existing;  // same account: only the password changes, the sync bookkeeping stays
    } else {
        account.server_url = check.normalized;
        account.username = user.trimmed().toStdString();
    }
    account.password = password.toStdString();
    if (!store_->save_account(account)) return fail(QStringLiteral("storage_error"));
    store_->clear_setup_draft();
    emit accountChanged();
    return true;
}

bool NotesBridge::forgetAccount() {
    if (!store_) return false;
    if (!store_->reset_local_data()) {
        emit errorOccurred(QStringLiteral("pending_changes"), text_for(QStringLiteral("pending_changes")));
        return false;
    }
    if (!store_->forget_account()) {
        emit errorOccurred(QStringLiteral("storage_error"), text_for(QStringLiteral("storage_error")));
        return false;
    }
    emit accountChanged();
    return true;
}

// ---- notes, reader, font size ----------------------------------------------------------------

namespace {

// The stored title is the first line as written; a Markdown heading marker ("## Plan") is not shown.
QString display_title(const std::string& title) {
    QString t = QString::fromStdString(title);
    int i = 0;
    while (i < t.size() && i < 6 && t.at(i) == QLatin1Char('#')) ++i;
    if (i > 0 && i < t.size() && t.at(i).isSpace()) t = t.mid(i).trimmed();
    if (t.trimmed().isEmpty()) return QStringLiteral("(empty note)");  // never an empty row title
    return t;
}

}  // namespace

QString NotesBridge::view() const {
    if (!store_) return QStringLiteral("all");
    const QString saved = QString::fromStdString(store_->get_setting("view"));
    if (!saved.startsWith(QStringLiteral("category:"))) return saved;
    for (const pbnotes::CategoryCount& c : store_->categories())
        if (QString::fromStdString(c.name) == saved.mid(9)) return saved;
    return QStringLiteral("all");  // the category has no notes any more
}

QString NotesBridge::sortOrder() const {
    return store_ ? QString::fromStdString(store_->get_setting("sort_order")) : QStringLiteral("modified");
}

QVariantList NotesBridge::categories() const {
    QVariantList out;
    if (!store_) return out;
    for (const pbnotes::CategoryCount& c : store_->categories()) {
        QVariantMap row;
        row.insert(QStringLiteral("name"), QString::fromStdString(c.name));
        row.insert(QStringLiteral("count"), c.count);
        out.append(row);
    }
    return out;
}

void NotesBridge::setView(const QString& view) {
    if (!store_ || !store_->set_setting("view", view.toStdString())) return;
    emit listOptionsChanged();
    emit notesChanged();
}

void NotesBridge::setSortOrder(const QString& order) {
    if (!store_ || !store_->set_setting("sort_order", order.toStdString())) return;
    emit listOptionsChanged();
    emit notesChanged();
}

void NotesBridge::setSearch(const QString& text) {
    if (text == searchText_) return;
    searchText_ = text;
    emit listOptionsChanged();
    emit notesChanged();
}

QVariantList NotesBridge::listNotes() const {
    QVariantList out;
    if (!store_) return out;
    pbnotes::ListQuery q;
    q.view = view().toStdString();
    q.sort = store_->get_setting("sort_order");
    q.search = searchText_.toStdString();
    for (const pbnotes::NoteSummary& n : store_->list(q)) {
        QVariantMap row;
        row.insert(QStringLiteral("localId"), static_cast<qlonglong>(n.local_id));
        row.insert(QStringLiteral("title"), display_title(n.title));
        row.insert(QStringLiteral("category"), QString::fromStdString(n.category));
        row.insert(QStringLiteral("excerpt"), QString::fromStdString(n.excerpt));
        row.insert(QStringLiteral("favorite"), n.favorite);
        row.insert(QStringLiteral("modified"), static_cast<qlonglong>(n.modified));
        row.insert(QStringLiteral("pending"), n.pending);
        out.append(row);
    }
    return out;
}

QVariantMap NotesBridge::openNote(qlonglong localId) const {
    QVariantMap out;
    if (!store_) return out;
    const auto note = store_->get(localId);
    if (!note || note->status == pbnotes::NoteStatus::Deleted) return out;
    out.insert(QStringLiteral("localId"), localId);
    out.insert(QStringLiteral("title"), display_title(note->title));
    out.insert(QStringLiteral("category"), QString::fromStdString(note->category));
    out.insert(QStringLiteral("favorite"), note->favorite);
    out.insert(QStringLiteral("modified"), static_cast<qlonglong>(note->modified));
    out.insert(QStringLiteral("pending"), note->status != pbnotes::NoteStatus::Void);
    const QVariantList blocks = reader_blocks(*note, store_.get());
    out.insert(QStringLiteral("blocks"), blocks);
    return out;
}

double NotesBridge::pageTarget(double contentY, double viewHeight, double contentHeight, double step, int direction,
                               const QVariantList& unsplittable) const {
    std::vector<pbnotes::Interval> blocks;
    for (const QVariant& v : unsplittable) {
        const QVariantMap m = v.toMap();
        blocks.push_back({m.value(QStringLiteral("top")).toDouble(), m.value(QStringLiteral("bottom")).toDouble()});
    }
    return pbnotes::page_target(contentY, viewHeight, contentHeight, step, direction, blocks);
}

QString NotesBridge::formatSize(qlonglong bytes) const { return format_size(bytes); }

QString NotesBridge::fontSize() const {
    return store_ ? QString::fromStdString(store_->get_setting("font_size")) : QStringLiteral("medium");
}

void NotesBridge::setFontSize(const QString& size) {
    if (!store_ || size == fontSize()) return;
    if (store_->set_setting("font_size", size.toStdString())) emit fontSizeChanged();
}

int NotesBridge::fontPx() const {
    const QString s = fontSize();
    return s == QLatin1String("small") ? 30 : s == QLatin1String("large") ? 50 : 38;
}

// ---- sync ------------------------------------------------------------------------------------

namespace {

QString sync_message_text(const std::string& key) {
    static const struct { const char* key; const char* text; } kTexts[] = {
        {"ok", "Synced."},
        {"up_to_date", "Everything is up to date."},
        {"partial", "Some notes could not be synced yet."},
        {"attachments_failed", "Some attachments could not be uploaded."},
        {"offline", "The server could not be reached (offline)."},
        {"maintenance", "The server is in maintenance mode, try again later."},
        {"auth_rejected", "The server rejected the login. Check the app password."},
        {"certificate_unverifiable", "The server certificate could not be verified."},
        {"server_unsupported", "The server does not support the Notes API version 1."},
        {"server_error", "The server answered with an error."},
        {"already_running", "A sync is already running."},
    };
    for (const auto& t : kTexts)
        if (key == t.key) return QString::fromUtf8(t.text);
    return QStringLiteral("The sync did not finish.");
}

QString format_time(int64_t epoch) {
    const std::time_t t = static_cast<std::time_t>(epoch);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", &tm);
    return QString::fromLatin1(buf);
}

}  // namespace

QString NotesBridge::syncOutcome() const {
    if (!store_) return QStringLiteral("never");
    const auto account = store_->get_account();
    if (!account) return QStringLiteral("never");
    return QString::fromStdString(pbnotes::sync_status(*account, syncRunning_).state);
}

int NotesBridge::conflictCount() const {
    if (!store_) return 0;
    const auto account = store_->get_account();
    return account ? account->last_sync_counts.conflicts : 0;
}

QString NotesBridge::syncStatus() const {
    if (!store_) return QStringLiteral("No storage available.");
    const auto account = store_->get_account();
    if (!account) return QStringLiteral("Not synced yet.");
    const pbnotes::SyncStatus st = pbnotes::sync_status(*account, syncRunning_);
    if (st.state == "running") return QStringLiteral("Syncing...");
    if (st.state == "never" || !st.at) return QStringLiteral("Not synced yet.");

    QString text = QStringLiteral("Last sync: ") + format_time(*st.at);
    if (st.state == "success" || st.state == "partial") {
        QStringList parts;
        if (st.counts.pushed) parts << QStringLiteral("%1 sent").arg(st.counts.pushed);
        if (st.counts.pulled) parts << QStringLiteral("%1 received").arg(st.counts.pulled);
        if (st.counts.deleted) parts << QStringLiteral("%1 removed").arg(st.counts.deleted);
        if (st.counts.attachments_uploaded) parts << QStringLiteral("%1 attachment(s) uploaded").arg(st.counts.attachments_uploaded);
        if (st.counts.conflicts) parts << QStringLiteral("%1 conflict(s) - copies kept").arg(st.counts.conflicts);
        if (st.counts.restored) parts << QStringLiteral("%1 deleted note(s) restored").arg(st.counts.restored);
        text += parts.isEmpty() ? QStringLiteral(", up to date") : QStringLiteral(", ") + parts.join(QStringLiteral(", "));
        if (st.state == "partial")
            text += QStringLiteral(". ") + sync_message_text(st.message_key == "attachments_failed" ? "attachments_failed" : "partial");
    } else {
        text += QStringLiteral(". ") + sync_message_text(st.message_key);
    }
    return text;
}

QString NotesBridge::formatTime(qlonglong epochSeconds) const { return format_time(epochSeconds); }

void NotesBridge::syncNow() {
    if (!store_ || !store_->get_account() || syncRunning_.exchange(true)) return;
    if (syncWorker_.joinable()) syncWorker_.join();
    emit syncStateChanged();
    refreshCa();
    const std::string ca = caPath_.toStdString();
    syncWorker_ = std::thread([this, ca]() {
        pbnotes::CurlTransport transport(ca);
        pbnotes::SyncOptions options;
        options.attachments_dir = attachmentsDir().toStdString();
        pbnotes::SyncEngine engine(*store_, transport, nullptr, options);
        const pbnotes::SyncResult r = engine.run();
        // the sync may have removed notes or references: files nothing names any more go away
        pbnotes::AttachmentService::purge(*store_, attachmentsDir().toStdString(), 120);   // may overlap a download
        QMetaObject::invokeMethod(this, [this, r]() {
            syncRunning_ = false;
            emit syncStateChanged();
            emit notesChanged();
            emit syncFinished(QString::fromStdString(pbnotes::to_string(r.outcome)),
                              sync_message_text(r.message_key));
            if (r.counts.conflicts || r.counts.restored) emit conflictsDetected(r.counts.conflicts, r.counts.restored);
        }, Qt::QueuedConnection);
    });
}

// ---- editing ----------------------------------------------------------------------------------

QVariantMap NotesBridge::loadForEdit(qlonglong localId) const {
    QVariantMap out;
    if (!store_) return out;
    const auto note = store_->get(localId);
    if (!note || note->status == pbnotes::NoteStatus::Deleted) return out;
    out.insert(QStringLiteral("text"), QString::fromStdString(note->content));
    out.insert(QStringLiteral("category"), QString::fromStdString(note->category));
    out.insert(QStringLiteral("favorite"), note->favorite);
    return out;
}

int NotesBridge::pendingCount() const {
    return store_ ? static_cast<int>(store_->pending_notes().size()) : 0;
}

namespace {
int64_t now_seconds() { return static_cast<int64_t>(std::time(nullptr)); }
}  // namespace

// Reports the result of a store edit: emits the change signals on success, errorOccurred() otherwise.
static bool finish_edit(NotesBridge* bridge, const pbnotes::EditResult& r) {
    if (r.error == pbnotes::EditError::None) return true;
    const QString key = QString::fromStdString(r.error_key.empty() ? std::string("storage_error") : r.error_key);
    emit bridge->errorOccurred(key, bridge->message(key));
    return false;
}

qlonglong NotesBridge::saveNote(qlonglong localId, const QString& text, const QString& category, bool favorite) {
    if (!store_) return -1;
    const std::string content = text.toStdString(), cat = category.toStdString();
    const pbnotes::EditResult r = localId > 0 ? store_->save_local(localId, content, cat, favorite, now_seconds())
                                              : store_->create_local(content, cat, favorite, now_seconds());
    if (!finish_edit(this, r)) return -1;
    emit noteChanged(r.local_id);
    emit notesChanged();
    return r.local_id;
}

bool NotesBridge::deleteNote(qlonglong localId) {
    if (!store_ || !finish_edit(this, store_->mark_deleted(localId))) return false;
    emit noteChanged(localId);
    emit notesChanged();
    return true;
}

bool NotesBridge::setCategory(qlonglong localId, const QString& category) {
    if (!store_ || !finish_edit(this, store_->set_category(localId, category.toStdString(), now_seconds()))) return false;
    emit noteChanged(localId);
    emit notesChanged();
    return true;
}

bool NotesBridge::toggleFavorite(qlonglong localId) {
    if (!store_) return false;
    const auto note = store_->get(localId);
    if (!note) return finish_edit(this, pbnotes::EditResult{pbnotes::EditError::NotFound, 0, "not_found"});
    if (!finish_edit(this, store_->set_favorite(localId, !note->favorite, now_seconds()))) return false;
    emit noteChanged(localId);
    emit notesChanged();
    return true;
}

bool NotesBridge::toggleTask(qlonglong localId, int lineIndex) {
    if (!store_) return false;
    const auto before = store_->get(localId);
    if (!before || before->status == pbnotes::NoteStatus::Deleted) return false;
    if (!finish_edit(this, store_->toggle_task(localId, lineIndex, now_seconds()))) return false;
    const auto after = store_->get(localId);
    if (!after || after->content == before->content) return false;
    emit noteChanged(localId);
    emit notesChanged();
    return true;
}
