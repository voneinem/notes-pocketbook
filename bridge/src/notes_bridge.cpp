#include "notes_bridge.h"
#include "reader_blocks.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <cstring>

#include <QDir>
#include <QMetaObject>
#include <QTimer>

#include "pbnotes/attachment_service.h"
#include "pbnotes/ca_selector.h"
#include "pbnotes/connection_test.h"
#include "pbnotes/markdown.h"
#include "pbnotes/paging.h"
#include "pbnotes/sync_engine.h"
#include "pbnotes/sync_status.h"
#include "pbnotes/title.h"
#include "pbnotes/curl_transport.h"
#include "pbnotes/log.h"

namespace {

// Milliseconds on a clock that never goes back: the scheduler only compares differences.
int64_t steady_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

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
        {"title_empty", "The note needs a title."},
        {"title_too_long", "The title is too long (at most 255 characters)."},
        {"title_control_chars", "The title contains characters that are not allowed."},
        {"title_invalid", "The title contains characters that are not allowed."},
        {"category_empty", "Enter a category name."},
        {"category_too_long", "The category is too long (at most 255 characters)."},
        {"category_spaces", "The category must not start or end with a space."},
        {"category_control_chars", "The category contains characters that are not allowed."},
        {"category_invalid", "The category contains characters that are not allowed."},
        {"paste_invalid", "The copied text cannot be pasted."},
        {"paste_too_large", "The copied text is too large to paste."},
        {"not_found", "This note no longer exists."},
    };
    for (const auto& t : kTexts)
        if (key == QLatin1String(t.key)) return QString::fromUtf8(t.text);
    return key;
}

}  // namespace

NotesBridge::NotesBridge(const Config& config, QObject* parent) : QObject(parent), config_(config) {
    {
        pbnotes::SyncScheduler::Options options;
        options.quiet_ms = config_.autoSyncQuietMs;
        options.gap_ms = config_.autoSyncGapMs;
        scheduler_ = pbnotes::SyncScheduler(options);
    }
    autoTimer_ = new QTimer(this);
    autoTimer_->setSingleShot(true);
    connect(autoTimer_, &QTimer::timeout, this, [this]() { evaluateAutoSync(); });
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

bool NotesBridge::syncAfterSave() const { return store_ && store_->get_setting("sync_after_save") == "on"; }

void NotesBridge::setSyncAfterSave(bool on) {
    if (!store_ || on == syncAfterSave()) return;
    if (!store_->set_setting("sync_after_save", on ? "on" : "off")) return;
    emit syncAfterSaveChanged();
    evaluateAutoSync();   // off drops a start that was waiting; on starts nothing by itself
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

// The title as the user entered it (spec 004); only a legacy row without any title gets a placeholder, so a
// list row is never blank.
QString display_title(const std::string& title) {
    const QString t = QString::fromStdString(title);
    if (t.trimmed().isEmpty()) return QStringLiteral("(untitled)");
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

QVariantList NotesBridge::categoryChoices(const QString& current) const {
    auto choice = [](const QString& label, const QString& value) {
        QVariantMap m;
        m.insert(QStringLiteral("label"), label);
        m.insert(QStringLiteral("value"), value);
        return m;
    };
    QVariantList out;
    out.append(choice(tr("No category"), QStringLiteral("none")));
    out.append(choice(tr("New category…"), QStringLiteral("new")));
    if (!store_) return out;
    std::vector<std::string> names;
    for (const pbnotes::CategoryCount& c : store_->categories()) names.push_back(c.name);
    const std::string cur = current.toStdString();
    if (!cur.empty() && std::find(names.begin(), names.end(), cur) == names.end()) names.push_back(cur);
    std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) {
        const std::string fa = pbnotes::fold_case(a), fb = pbnotes::fold_case(b);
        return fa != fb ? fa < fb : a < b;
    });
    for (const std::string& n : names) {
        const QString name = QString::fromStdString(n);
        out.append(choice(name, QStringLiteral("c:") + name));
    }
    return out;
}

QVariantMap NotesBridge::checkCategory(const QString& name) const {
    std::string trimmed = name.toStdString();
    const char* blanks = " \t\r\n\f\v";
    const size_t b = trimmed.find_first_not_of(blanks);
    trimmed = b == std::string::npos ? std::string() : trimmed.substr(b, trimmed.find_last_not_of(blanks) - b + 1);
    QVariantMap out;
    out.insert(QStringLiteral("name"), QString::fromStdString(trimmed));
    const std::string error = trimmed.empty() ? std::string("category_empty") : pbnotes::validate_category(trimmed);
    out.insert(QStringLiteral("ok"), error.empty());
    out.insert(QStringLiteral("errorText"), error.empty() ? QString() : text_for(QString::fromStdString(error)));
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
    // The reading page shows the stored title above the text (spec 005); a note without one shows no title block,
    // never the "(untitled)" placeholder the list uses.
    out.insert(QStringLiteral("titleText"), QString::fromStdString(note->title));
    out.insert(QStringLiteral("hasTitle"), !QString::fromStdString(note->title).trimmed().isEmpty());
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

QString NotesBridge::listLayout() const {
    const std::string v = store_ ? store_->get_setting("list_layout") : std::string();
    return v == "tile" ? QStringLiteral("tile") : QStringLiteral("list");
}

void NotesBridge::setListLayout(const QString& layout) {
    if (!store_ || (layout != QLatin1String("list") && layout != QLatin1String("tile")) || layout == listLayout()) return;
    if (!store_->set_setting("list_layout", layout.toStdString())) return;
    emit listLayoutChanged();
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

// ---- sync after saving (spec 008) -----------------------------------------------------------------------------------
// The six places that change a note call noteSaved(). The scheduler (core, host-tested) says whether to wait, drop or
// start; starting is the ordinary syncNow(). Nothing here retries on a timer: the timer only serves a wait the
// scheduler asked for after a save.

void NotesBridge::noteSaved() {
    if (!syncAfterSave()) return;   // off: a save changes nothing about when notes sync (FR-010)
    scheduler_.note_saved(steady_ms());
    networkTried_ = false;   // a new save may ask for Wi-Fi again
    pbnotes::log::event("autosync.armed", {});
    evaluateAutoSync();
}

void NotesBridge::publishActive() {
    if (syncActive() == lastActive_) return;
    lastActive_ = syncActive();
    emit syncStateChanged();
}

void NotesBridge::evaluateAutoSync() {
    evaluateAutoSyncStep();
    publishActive();   // the Sync now button follows: armed or connecting reads as syncing
}

void NotesBridge::evaluateAutoSyncStep() {
    if (!store_) return;
    pbnotes::SyncScheduler::Inputs in;
    in.enabled = syncAfterSave();
    in.configured = store_->get_account().has_value();
    in.running = syncRunning_;
    in.editor_open = editorOpen_;
    in.has_pending = !store_->pending_notes().empty();
    in.online = !config_.wifiConnected || config_.wifiConnected();
    in.can_connect = static_cast<bool>(wifiRequest_) && !networkTried_;
    const bool was_armed = scheduler_.armed();
    const pbnotes::SyncScheduler::Step step = scheduler_.next(steady_ms(), in);
    switch (step.decision) {
    case pbnotes::SyncScheduler::Decision::Start:
        autoTimer_->stop();
        pbnotes::log::event("autosync.start", {pbnotes::log::count("pending", static_cast<long>(store_->pending_notes().size()))});
        syncNow();
        break;
    case pbnotes::SyncScheduler::Decision::Wait:
        autoTimer_->start(static_cast<int>(std::max<int64_t>(1, step.wait_ms)));
        break;
    case pbnotes::SyncScheduler::Decision::Connect:
        autoTimer_->stop();
        if (!awaitingNetwork_) {   // saves made while the device connects share the one request
            awaitingNetwork_ = true;
            networkTried_ = true;
            pbnotes::log::event("autosync.connect", {});
            wifiRequest_();
        }
        break;
    case pbnotes::SyncScheduler::Decision::Idle:
        autoTimer_->stop();
        if (was_armed && !scheduler_.armed())
            pbnotes::log::event("autosync.drop", {pbnotes::log::text("reason", step.reason)});
        else if (was_armed)
            pbnotes::log::event("autosync.deferred", {pbnotes::log::text("reason", step.reason)});
        break;
    }
}

void NotesBridge::networkAnswered(bool connected) {
    if (!awaitingNetwork_) return;   // somebody else's connection (Sync now, a download)
    awaitingNetwork_ = false;
    pbnotes::log::event("autosync.network", {pbnotes::log::text("result", connected ? "connected" : "failed")});
    evaluateAutoSync();   // connected: the sync starts; failed: networkTried_ makes the start drop quietly
}

void NotesBridge::setEditorOpen(bool open) {
    if (open == editorOpen_) return;
    editorOpen_ = open;
    if (!open) {
        editBase_.clear();   // nothing is being edited any more
        evaluateAutoSync();
    }
}

void NotesBridge::syncNow() {
    if (!store_ || !store_->get_account() || syncRunning_.exchange(true)) return;
    scheduler_.sync_started(steady_ms());
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
            evaluateAutoSync();   // a save made while this sync ran is sent by one more (spec 008)
        }, Qt::QueuedConnection);
    });
}

// ---- editing ----------------------------------------------------------------------------------

QString NotesBridge::readerCopy(qlonglong localId, int fromUnit, int fromOffset, int toUnit, int toOffset) const {
    if (!store_) return QString();
    const auto note = store_->get(localId);
    if (!note || note->status == pbnotes::NoteStatus::Deleted) return QString();
    const QString title = QString::fromStdString(note->title);
    const bool has_title = !title.trimmed().isEmpty();
    const int offset_units = has_title ? 1 : 0;
    const QVariantList entries = reader_blocks(*note, store_.get());
    const std::vector<pbnotes::Block> blocks = pbnotes::parse_markdown(note->content);
    const int units = offset_units + static_cast<int>(entries.size());

    if (fromUnit > toUnit || (fromUnit == toUnit && fromOffset > toOffset)) {
        std::swap(fromUnit, toUnit);
        std::swap(fromOffset, toOffset);
    }
    if (fromUnit < 0 || toUnit >= units) return QString();

    // A displayed position of a unit as a core SourcePoint. UTF-16 offsets become byte offsets in the block's text.
    auto point = [&](int unit, int offset) {
        pbnotes::SourcePoint p;
        const QVariantMap entry = entries.at(unit - offset_units).toMap();
        p.block = static_cast<size_t>(entry.value(QStringLiteral("block")).toInt());
        p.row = static_cast<size_t>(entry.value(QStringLiteral("row")).toInt());
        const QString kind = entry.value(QStringLiteral("kind")).toString();
        if ((kind == QLatin1String("heading") || kind == QLatin1String("paragraph") || kind == QLatin1String("listitem")) &&
            p.block < blocks.size()) {
            std::string plain;
            for (const pbnotes::Span& sp : blocks[p.block].spans) plain += sp.text;
            const QString text = QString::fromStdString(plain);
            p.byte = static_cast<size_t>(text.left(qBound(0, offset, static_cast<int>(text.size()))).toUtf8().size());
        }
        return p;
    };

    QString out;
    int first_text_unit = fromUnit;
    if (has_title && fromUnit == 0) {
        const int len = static_cast<int>(title.size());
        const int a = qBound(0, fromOffset, len);
        const int b = toUnit == 0 ? qBound(a, toOffset, len) : len;
        out = title.mid(a, b - a);
        if (toUnit == 0) return out;
        out += QLatin1Char('\n');
        first_text_unit = 1;
        fromOffset = 0;  // the text part starts at the beginning of the first block
    }
    out += QString::fromStdString(
        pbnotes::copy_source(note->content, blocks, point(first_text_unit, fromOffset), point(toUnit, toOffset)));
    return out;
}

QVariantMap NotesBridge::loadForEdit(qlonglong localId) const {
    QVariantMap out;
    if (!store_) return out;
    const auto note = store_->get(localId);
    if (!note || note->status == pbnotes::NoteStatus::Deleted) return out;
    out.insert(QStringLiteral("title"), QString::fromStdString(note->title));
    out.insert(QStringLiteral("text"), QString::fromStdString(note->content));
    out.insert(QStringLiteral("category"), QString::fromStdString(note->category));
    out.insert(QStringLiteral("favorite"), note->favorite);
    editBase_[localId] = pbnotes::EditBase{note->content, note->category, note->favorite};
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

qlonglong NotesBridge::saveNote(qlonglong localId, const QString& title, const QString& text, const QString& category,
                                bool favorite) {
    if (!store_) return -1;
    const std::string ttl = title.toStdString(), content = text.toStdString(), cat = category.toStdString();
    pbnotes::EditResult r;
    const auto base = localId > 0 ? editBase_.find(localId) : editBase_.end();
    if (localId <= 0) {
        r = store_->create_local(ttl, content, cat, favorite, now_seconds());
    } else if (base == editBase_.end()) {
        r = store_->save_local(localId, ttl, content, cat, favorite, now_seconds());
    } else {
        r = store_->save_local_if_unchanged(localId, base->second, ttl, content, cat, favorite, now_seconds());
        if (r.error == pbnotes::EditError::ChangedOnServer) {
            // A sync replaced the note while the editor was open: keep the server's version as the note and the
            // editor's text as a new note, and tell the reader once (the conflict dialog says exactly this).
            const pbnotes::EditResult copy = store_->save_edit_as_conflict_copy(localId, ttl, content, cat, favorite, now_seconds());
            if (!finish_edit(this, copy)) return -1;
            editBase_.erase(localId);
            pbnotes::log::event("note.conflict_on_save", {pbnotes::log::count("copies", 1)});
            emit noteChanged(localId);
            emit noteChanged(copy.local_id);
            emit notesChanged();
            emit conflictsDetected(1, 0);
            noteSaved();
            return copy.local_id;
        }
    }
    if (!finish_edit(this, r)) return -1;
    if (localId > 0) editBase_.erase(localId);
    emit noteChanged(r.local_id);
    emit notesChanged();
    noteSaved();
    return r.local_id;
}

bool NotesBridge::deleteNote(qlonglong localId) {
    if (!store_ || !finish_edit(this, store_->mark_deleted(localId))) return false;
    editBase_.erase(localId);
    emit noteChanged(localId);
    emit notesChanged();
    noteSaved();
    return true;
}

bool NotesBridge::setCategory(qlonglong localId, const QString& category) {
    if (!store_ || !finish_edit(this, store_->set_category(localId, category.toStdString(), now_seconds()))) return false;
    emit noteChanged(localId);
    emit notesChanged();
    noteSaved();
    return true;
}

bool NotesBridge::toggleFavorite(qlonglong localId) {
    if (!store_) return false;
    const auto note = store_->get(localId);
    if (!note) return finish_edit(this, pbnotes::EditResult{pbnotes::EditError::NotFound, 0, "not_found"});
    if (!finish_edit(this, store_->set_favorite(localId, !note->favorite, now_seconds()))) return false;
    emit noteChanged(localId);
    emit notesChanged();
    noteSaved();
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
    noteSaved();
    return true;
}
