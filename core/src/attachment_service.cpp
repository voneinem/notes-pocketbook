#include "pbnotes/attachment_service.h"

#include <dirent.h>
#include <sys/stat.h>

#include <cerrno>
#include <cstdio>
#include <unistd.h>

#include <ctime>
#include <set>

#include "pbnotes/api_client.h"
#include "pbnotes/attachment_ref.h"
#include "pbnotes/log.h"

namespace pbnotes {

AttachmentService::AttachmentService(NoteStore& store, HttpTransport& transport, std::string attachments_dir, Clock clock)
    : store_(store), transport_(transport), dir_(std::move(attachments_dir)), clock_(std::move(clock)) {
    if (!clock_) clock_ = [] { return static_cast<int64_t>(std::time(nullptr)); };
    ::mkdir(dir_.c_str(), 0755);
}

namespace {

AttachmentService::DownloadResult failed(NoteStore& store, int64_t row_id, const std::string& key,
                                         ErrorClass transport_error = ErrorClass::None) {
    store.set_attachment_error(row_id, 0, "", key);
    AttachmentService::DownloadResult r;
    r.error_key = key;
    r.transport_error = transport_error;
    return r;
}

}  // namespace

AttachmentService::DownloadResult AttachmentService::download(int64_t note_local_id, const std::string& remote_path) {
    DownloadResult result;
    auto account = store_.get_account();
    auto note = store_.get(note_local_id);
    auto row = store_.ensure_attachment(note_local_id, remote_path);
    if (!account || !note || !row) {
        result.error_key = "not_found";
        return result;
    }
    if (!supports_attachments(account->api_versions)) return failed(store_, row->id, "unsupported_server");
    if (!note->remote_id) return failed(store_, row->id, "not_found");  // a note the server does not know has no attachments there

    const std::string part = path_of(std::to_string(row->id) + ".part");
    ApiClient api(transport_, {account->server_url, account->username, account->password});
    const AttachmentFetchResult fetched = api.download_attachment(*note->remote_id, remote_path, part, kMaxAttachmentBytes);
    if (fetched.error != ErrorClass::None) {
        std::remove(part.c_str());
        const bool credentials = fetched.error == ErrorClass::Auth;
        log::event("attachment.download", {log::text("result", credentials ? "rejected" : "network")});
        return failed(store_, row->id, credentials ? "rejected" : "network", fetched.error);
    }
    const char* failure = fetched.too_large ? "too_large"
                          : fetched.storage_full ? "storage_full"
                          : fetched.not_found ? "not_found"
                          : fetched.unsupported ? "unsupported_server"
                          : fetched.rejected || !fetched.size ? "rejected"
                                                              : nullptr;
    if (failure) {
        std::remove(part.c_str());
        log::event("attachment.download", {log::text("result", failure)});
        return failed(store_, row->id, failure);
    }

    const std::string ext = safe_extension(remote_path);
    const std::string final_name = std::to_string(row->id) + (ext.empty() ? "" : "." + ext);
    if (std::rename(part.c_str(), path_of(final_name).c_str()) != 0) {
        const bool full = errno == ENOSPC || errno == EDQUOT;
        std::remove(part.c_str());
        return failed(store_, row->id, full ? "storage_full" : "rejected");
    }
    if (!store_.record_downloaded(note_local_id, remote_path, *fetched.size, final_name)) {
        std::remove(path_of(final_name).c_str());
        return failed(store_, row->id, "rejected");
    }
    log::event("attachment.download", {log::text("result", "ok"), log::count("bytes", *fetched.size)});
    result.ok = true;
    return result;
}

AddResult AttachmentService::add(const std::string& source_path) {
    AddResult result;
    auto fail = [&](const char* key) {
        result.ok = false;
        result.error_key = key;
        return result;
    };
    struct stat st;
    if (source_path.empty() || ::stat(source_path.c_str(), &st) != 0 || !S_ISREG(st.st_mode) ||
        ::access(source_path.c_str(), R_OK) != 0)
        return fail("unreadable");
    if (st.st_size > kMaxAttachmentBytes) return fail("too_large");
    const std::string name = attachment_file_name(source_path);
    if (name.empty()) return fail("unreadable");

    auto row = store_.stage_attachment(name, clock_());
    if (!row) return fail("storage_full");
    const std::string part = path_of(std::to_string(row->id) + ".part");
    const std::string ext = safe_extension(name);
    const std::string final_name = std::to_string(row->id) + (ext.empty() ? "" : "." + ext);
    auto cleanup = [&]() {
        std::remove(part.c_str());
        std::remove(path_of(final_name).c_str());
        store_.drop_staged(row->id);
    };

    FILE* in = std::fopen(source_path.c_str(), "rb");  // read only: the original is never touched
    if (!in) {
        cleanup();
        return fail("unreadable");
    }
    FILE* out = std::fopen(part.c_str(), "wb");
    if (!out) {
        const bool full = errno == ENOSPC || errno == EDQUOT;
        std::fclose(in);
        cleanup();
        return fail(full ? "storage_full" : "unreadable");
    }
    const char* failure = nullptr;
    int64_t total = 0;
    char buffer[64 * 1024];
    for (;;) {
        const size_t n = std::fread(buffer, 1, sizeof buffer, in);
        if (n == 0) {
            if (std::ferror(in)) failure = "unreadable";
            break;
        }
        total += static_cast<int64_t>(n);
        if (total > kMaxAttachmentBytes) { failure = "too_large"; break; }  // the file grew while it was read
        if (fail_copy_after_ >= 0 && total > fail_copy_after_) { failure = "storage_full"; break; }
        if (std::fwrite(buffer, 1, n, out) != n) {
            failure = errno == ENOSPC || errno == EDQUOT ? "storage_full" : "unreadable";
            break;
        }
    }
    std::fclose(in);
    if (std::fclose(out) != 0 && !failure) failure = "storage_full";  // the final flush failed
    if (failure) {
        cleanup();
        return fail(failure);
    }
    if (std::rename(part.c_str(), path_of(final_name).c_str()) != 0 || !store_.set_staged_file(row->id, total, final_name)) {
        const bool full = errno == ENOSPC || errno == EDQUOT;
        cleanup();
        return fail(full ? "storage_full" : "unreadable");
    }
    log::event("attachment.add", {log::text("result", "ok"), log::count("bytes", total)});
    result.ok = true;
    result.id = row->id;
    result.display_name = row->display_name;
    result.size = total;
    result.token = make_token(row->id, row->display_name);
    return result;
}

std::optional<int64_t> AttachmentService::probe_size(int64_t note_local_id, const std::string& remote_path) {
    if (auto row = store_.attachment_for(note_local_id, remote_path))
        if (row->size) return row->size;
    auto account = store_.get_account();
    auto note = store_.get(note_local_id);
    if (!account || !note || !note->remote_id || !supports_attachments(account->api_versions)) return std::nullopt;
    ApiClient api(transport_, {account->server_url, account->username, account->password});
    const AttachmentProbeResult probe = api.probe_attachment(*note->remote_id, remote_path);
    if (probe.not_found) {
        store_.set_attachment_error(0, note_local_id, remote_path, "not_found");
        return std::nullopt;
    }
    if (probe.error != ErrorClass::None || !probe.size) return std::nullopt;
    if (!store_.record_size(note_local_id, remote_path, *probe.size)) return std::nullopt;
    return probe.size;
}

namespace {

// "<digits>" or "<digits>.<ext>" with ext of 1-8 lower-case letters or digits: the names this service creates.
bool is_stored_name(const std::string& name) {
    size_t i = 0;
    while (i < name.size() && name[i] >= '0' && name[i] <= '9') ++i;
    if (i == 0) return false;
    if (i == name.size()) return true;
    if (name[i] != '.' || name.size() - i - 1 < 1 || name.size() - i - 1 > 8) return false;
    for (size_t k = i + 1; k < name.size(); ++k)
        if (!((name[k] >= 'a' && name[k] <= 'z') || (name[k] >= '0' && name[k] <= '9'))) return false;
    return true;
}

}  // namespace

void AttachmentService::purge(NoteStore& store, const std::string& directory, int64_t min_age_seconds) {
    const std::vector<std::string> named = store.referenced_local_files();
    const std::set<std::string> keep(named.begin(), named.end());
    DIR* d = opendir(directory.c_str());
    if (!d) return;
    int removed = 0;
    while (dirent* e = readdir(d)) {
        const std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        struct stat st;
        const std::string full = directory + "/" + name;
        if (::lstat(full.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;  // only plain files, never links
        if (min_age_seconds > 0 && std::time(nullptr) - st.st_mtime < min_age_seconds) continue;
        const bool part = name.size() >= 5 && name.compare(name.size() - 5, 5, ".part") == 0;
        if (!part && !is_stored_name(name)) continue;  // not one of ours: never delete what we did not create
        if (part || !keep.count(name)) {
            if (std::remove(full.c_str()) == 0) ++removed;
        }
    }
    closedir(d);
    if (removed > 0) log::event("attachment.purge", {log::count("removed", removed)});
}

}  // namespace pbnotes
