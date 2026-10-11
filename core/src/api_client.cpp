#include "pbnotes/api_client.h"

#include <sys/stat.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pbnotes/attachment_ref.h"
#include "pbnotes/json.h"
#include "pbnotes/version.h"

namespace pbnotes {

bool Capabilities::has_api_v1() const {
    for (const auto& v : notes_api_versions)
        if (v == "1" || v.compare(0, 2, "1.") == 0) return true;
    return false;
}

std::string etag_header_value(const std::string& etag) {
    if (etag.size() >= 2 && etag.front() == '"' && etag.back() == '"') return etag;
    if (etag.compare(0, 3, "W/\"") == 0) return etag;
    return "\"" + etag + "\"";
}

int64_t parse_http_date(const std::string& value) {
    static const char* const kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int day = 0, year = 0, hh = 0, mm = 0, ss = 0;
    char month[4] = {0};
    if (std::sscanf(value.c_str(), "%*3s, %d %3s %d %d:%d:%d GMT", &day, month, &year, &hh, &mm, &ss) != 6) return 0;
    int mon = -1;
    for (int i = 0; i < 12; ++i)
        if (std::strcmp(month, kMonths[i]) == 0) mon = i + 1;
    if (mon < 0 || day < 1 || day > 31 || hh > 23 || mm > 59 || ss > 60 || year < 1970) return 0;
    // days since the epoch (proleptic Gregorian, locale independent)
    const int y = year - (mon <= 2 ? 1 : 0);
    const int era = y / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = static_cast<unsigned>((153 * (mon + (mon > 2 ? -3 : 9)) + 2) / 5 + day - 1);
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const int64_t days = static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
    return days * 86400 + hh * 3600 + mm * 60 + ss;
}

ErrorClass classify_response(const Response& r) {
    if (r.status == 0) return r.error == ErrorClass::None ? ErrorClass::Network : r.error;
    if (r.status == 401 || r.status == 403) return ErrorClass::Auth;
    if (r.status == 503) return ErrorClass::Maintenance;
    if (r.status >= 200 && r.status < 300) return ErrorClass::None;
    return ErrorClass::Server;
}

ApiClient::ApiClient(HttpTransport& transport, Credentials credentials)
    : transport_(transport), creds_(std::move(credentials)) {}

Request ApiClient::make_request(const std::string& method, const std::string& url) const {
    Request req;
    req.method = method;
    req.url = url;
    req.username = creds_.username;
    req.password = creds_.password;
    req.headers.emplace_back("Accept", "application/json");
    req.headers.emplace_back("User-Agent", std::string("pbnotes/") + version());
    return req;
}

CapabilitiesResult ApiClient::fetch_capabilities() {
    CapabilitiesResult result;
    Request req = make_request("GET", creds_.server_url + "/ocs/v2.php/cloud/capabilities?format=json");
    req.headers.emplace_back("OCS-APIRequest", "true");
    const Response resp = transport_.perform(req);
    result.status = resp.status;
    result.error = classify_response(resp);
    if (result.error != ErrorClass::None) {
        result.detail = resp.error_detail;
        return result;
    }

    std::string parse_error;
    auto doc = json::parse(resp.body, &parse_error);
    if (!doc || !doc->is_object()) {
        result.error = ErrorClass::Server;
        result.detail = "invalid capabilities response";
        return result;
    }
    const json::Value* ocs = doc->find("ocs");
    const json::Value* data = ocs ? ocs->find("data") : nullptr;
    const json::Value* caps = data ? data->find("capabilities") : nullptr;
    if (!caps || !caps->is_object()) {
        result.error = ErrorClass::Server;
        result.detail = "capabilities missing";
        return result;
    }
    const json::Value* notes = caps->find("notes");
    if (!notes) {  // the Notes app is not installed or not enabled
        result.error = ErrorClass::Unsupported;
        result.detail = "notes app not available";
        return result;
    }
    const json::Value* versions = notes->find("api_version");
    if (!notes->is_object() || !versions || !versions->is_array()) {
        result.error = ErrorClass::Server;
        result.detail = "api_version has an unexpected type";
        return result;
    }
    std::vector<std::string> list;
    for (const auto& v : versions->array) {
        if (!v.is_string()) {
            result.error = ErrorClass::Server;
            result.detail = "api_version has an unexpected type";
            return result;
        }
        list.push_back(v.string);
    }
    result.capabilities.notes_api_versions = std::move(list);
    if (!result.capabilities.has_api_v1()) {
        result.error = ErrorClass::Unsupported;
        result.detail = "notes api v1 not offered";
    }
    return result;
}

namespace {

// Parses one note object. Pruned entries (only an id) are allowed when `allow_pruned` is set.
bool parse_api_note(const json::Value& v, bool allow_pruned, ApiNote* out) {
    if (!v.is_object()) return false;
    auto id = v.get_int("id");
    if (!id) return false;
    out->id = *id;
    if (allow_pruned && v.object.size() == 1) {
        out->pruned = true;
        return true;
    }
    auto etag = v.get_string("etag");
    auto content = v.get_string("content");
    auto category = v.get_string("category");
    auto favorite = v.get_bool("favorite");
    auto modified = v.get_int("modified");
    if (!etag || !content || !category || !favorite || !modified) return false;
    if (v.find("title") && !v.find("title")->is_string()) return false;
    out->etag = *etag;
    out->content = *content;
    if (auto title = v.get_string("title")) out->server_title = *title;
    out->category = *category;
    out->favorite = *favorite;
    out->modified = *modified;
    out->readonly = v.get_bool("readonly").value_or(false);
    out->pruned = false;
    return true;
}

}  // namespace

ListResult ApiClient::list_notes(const std::optional<std::string>& collection_etag, int64_t prune_before) {
    ListResult result;
    std::string url = creds_.server_url + "/index.php/apps/notes/api/v1/notes";
    if (prune_before > 0) url += "?pruneBefore=" + std::to_string(prune_before);
    Request req = make_request("GET", url);
    if (collection_etag && !collection_etag->empty()) req.headers.emplace_back("If-None-Match", etag_header_value(*collection_etag));
    const Response resp = transport_.perform(req);
    result.status = resp.status;
    if (resp.status == 304) {
        result.not_modified = true;
        return result;
    }
    result.error = classify_response(resp);
    if (result.error != ErrorClass::None) {
        result.detail = resp.error_detail;
        return result;
    }

    auto doc = json::parse(resp.body);
    if (!doc || !doc->is_array()) {
        result.error = ErrorClass::Server;
        result.detail = "invalid notes listing";
        return result;
    }
    std::vector<ApiNote> notes;
    for (const auto& item : doc->array) {
        ApiNote n;
        if (!parse_api_note(item, /*allow_pruned=*/true, &n)) {
            result.error = ErrorClass::Server;
            result.detail = "unexpected note format";
            return result;
        }
        notes.push_back(std::move(n));
    }
    result.notes = std::move(notes);
    if (has_header(resp.headers, "ETag")) result.etag = header_value(resp.headers, "ETag");
    result.last_modified = parse_http_date(header_value(resp.headers, "Last-Modified"));
    result.api_versions = header_value(resp.headers, "X-Notes-API-Versions");
    return result;
}

NoteResult ApiClient::get_note(int64_t id, const std::optional<std::string>& if_none_match) {
    NoteResult result;
    Request req = make_request("GET", creds_.server_url + "/index.php/apps/notes/api/v1/notes/" + std::to_string(id));
    if (if_none_match && !if_none_match->empty()) req.headers.emplace_back("If-None-Match", etag_header_value(*if_none_match));
    const Response resp = transport_.perform(req);
    result.status = resp.status;
    if (resp.status == 304) {
        result.not_modified = true;
        return result;
    }
    if (resp.status == 404) {
        result.not_found = true;
        return result;
    }
    result.error = classify_response(resp);
    if (result.error != ErrorClass::None) {
        result.detail = resp.error_detail;
        return result;
    }
    auto doc = json::parse(resp.body);
    ApiNote note;
    if (!doc || !parse_api_note(*doc, /*allow_pruned=*/false, &note)) {
        result.error = ErrorClass::Server;
        result.detail = "unexpected note format";
        return result;
    }
    result.note = std::move(note);
    return result;
}

std::string draft_json(const NoteDraft& d) {
    return "{\"title\":" + json::quote(d.title) + ",\"content\":" + json::quote(d.content) + ",\"category\":" + json::quote(d.category) +
           ",\"favorite\":" + (d.favorite ? "true" : "false") + ",\"modified\":" + std::to_string(d.modified) + "}";
}

WriteResult ApiClient::write_note(const std::string& method, const std::string& url, const NoteDraft& draft,
                                  const std::string& if_match_etag) {
    WriteResult result;
    Request req = make_request(method, url);
    req.headers.emplace_back("Content-Type", "application/json");
    if (!if_match_etag.empty()) req.headers.emplace_back("If-Match", etag_header_value(if_match_etag));
    req.body = draft_json(draft);
    const Response resp = transport_.perform(req);
    result.status = resp.status;
    if (resp.status == 412) {
        result.precondition_failed = true;
        return result;
    }
    if (resp.status == 404) {
        result.not_found = true;
        return result;
    }
    result.error = classify_response(resp);
    if (result.error != ErrorClass::None) {
        result.detail = resp.error_detail;
        return result;
    }
    auto doc = json::parse(resp.body);
    ApiNote note;
    if (!doc || !parse_api_note(*doc, /*allow_pruned=*/false, &note)) {
        result.error = ErrorClass::Server;
        result.detail = "unexpected note format";
        return result;
    }
    result.note = std::move(note);
    return result;
}

WriteResult ApiClient::create_note(const NoteDraft& draft) {
    return write_note("POST", creds_.server_url + "/index.php/apps/notes/api/v1/notes", draft, std::string());
}

WriteResult ApiClient::update_note(int64_t id, const NoteDraft& draft, const std::string& if_match_etag) {
    return write_note("PUT", creds_.server_url + "/index.php/apps/notes/api/v1/notes/" + std::to_string(id), draft,
                      if_match_etag);
}

DeleteResult ApiClient::delete_note(int64_t id, const std::string& stored_etag) {
    DeleteResult result;
    const std::string url = creds_.server_url + "/index.php/apps/notes/api/v1/notes/" + std::to_string(id);

    if (!delete_uses_if_match_) {
        // Check first: a note that changed on the server is a conflict and must not be deleted.
        const NoteResult check = get_note(id, stored_etag);
        result.status = check.status;
        if (check.error != ErrorClass::None) {
            result.error = check.error;
            result.detail = check.detail;
            return result;
        }
        if (check.not_found) {
            result.outcome = DeleteOutcome::AlreadyGone;
            return result;
        }
        if (!check.not_modified) {
            result.outcome = DeleteOutcome::ChangedOnServer;
            result.server_note = check.note;
            return result;
        }
    }

    Request req = make_request("DELETE", url);
    if (delete_uses_if_match_) req.headers.emplace_back("If-Match", etag_header_value(stored_etag));
    const Response resp = transport_.perform(req);
    result.status = resp.status;
    if (resp.status == 404) {
        result.outcome = DeleteOutcome::AlreadyGone;
        return result;
    }
    if (resp.status == 412) {
        result.outcome = DeleteOutcome::ChangedOnServer;
        return result;
    }
    result.error = classify_response(resp);
    if (result.error != ErrorClass::None) {
        result.detail = resp.error_detail;
        return result;
    }
    result.outcome = DeleteOutcome::Deleted;
    return result;
}

// ---- attachments -------------------------------------------------------------------------------------

bool supports_attachments(const std::string& api_versions) {
    size_t pos = 0;
    while (pos < api_versions.size()) {
        size_t end = api_versions.find(',', pos);
        if (end == std::string::npos) end = api_versions.size();
        std::string tok = api_versions.substr(pos, end - pos);
        const size_t b = tok.find_first_not_of(" \t");
        const size_t e = tok.find_last_not_of(" \t");
        tok = b == std::string::npos ? std::string() : tok.substr(b, e - b + 1);
        if (tok == "1.4") return true;
        pos = end + 1;
    }
    return false;
}

namespace {

// Percent-encodes everything except the unreserved characters (a query value, like encodeURIComponent).
std::string percent_encode_component(const std::string& s) {
    static const char kHex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[c >> 4]);
            out.push_back(kHex[c & 0xF]);
        }
    }
    return out;
}

// A Content-Length value: digits only; nullopt otherwise.
std::optional<int64_t> parse_length(const std::string& v) {
    if (v.empty() || v.size() > 15) return std::nullopt;
    int64_t n = 0;
    for (char c : v) {
        if (c < '0' || c > '9') return std::nullopt;
        n = n * 10 + (c - '0');
    }
    return n;
}

}  // namespace

std::string ApiClient::attachment_url(int64_t note_remote_id, const std::string& remote_path) const {
    return creds_.server_url + "/index.php/apps/notes/api/v1.4/attachment/" + std::to_string(note_remote_id) +
           "?path=" + percent_encode_component(remote_path);
}

AttachmentFetchResult ApiClient::download_attachment(int64_t note_remote_id, const std::string& remote_path,
                                                     const std::string& dest_part_path, int64_t max_bytes) {
    AttachmentFetchResult result;
    Request req = make_request("GET", attachment_url(note_remote_id, remote_path));
    for (auto& h : req.headers)
        if (h.first == "Accept") h.second = "*/*";  // a file, not JSON
    req.save_body_to = dest_part_path;
    req.max_body_bytes = max_bytes;
    req.total_timeout_s = 600;  // up to 10 MiB over a slow connection
    const Response resp = transport_.perform(req);
    result.status = resp.status;
    result.detail = resp.error_detail;
    if (resp.body_too_large) {
        result.too_large = true;
        return result;
    }
    if (resp.storage_full) {
        result.storage_full = true;
        return result;
    }
    result.error = classify_response(resp);
    if (resp.status == 0 || result.error == ErrorClass::Auth || result.error == ErrorClass::Maintenance) return result;
    if (resp.status == 404) {
        result.error = ErrorClass::None;
        result.not_found = true;
    } else if (resp.status == 400) {
        result.error = ErrorClass::None;
        result.unsupported = true;
    } else if (resp.status < 200 || resp.status >= 300) {
        result.rejected = true;  // error stays ErrorClass::Server
    } else {
        struct stat st;
        if (::stat(dest_part_path.c_str(), &st) == 0) result.size = static_cast<int64_t>(st.st_size);
        else result.error = ErrorClass::Server;  // a 2xx without a stored file: do not pretend success
    }
    return result;
}

AttachmentUploadResult ApiClient::upload_attachment(int64_t note_remote_id, const std::string& file_path,
                                                    const std::string& display_name) {
    AttachmentUploadResult result;
    struct stat st;
    if (::stat(file_path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
        result.unreadable = true;
        return result;
    }
    if (st.st_size > kMaxAttachmentBytes) {
        result.too_large = true;
        return result;
    }
    Request req = make_request("POST", creds_.server_url + "/index.php/apps/notes/api/v1.4/attachment/" + std::to_string(note_remote_id));
    req.upload_file = file_path;
    req.upload_field = "file";
    req.upload_file_name = display_name;
    req.total_timeout_s = 600;
    const Response resp = transport_.perform(req);
    result.status = resp.status;
    result.detail = resp.error_detail;
    result.error = classify_response(resp);
    if (resp.status == 0) {
        if (resp.error_detail == "the upload file cannot be read") {  // the transport could not read it either
            result.error = ErrorClass::None;
            result.unreadable = true;
        }
        return result;
    }
    if (result.error == ErrorClass::Auth || result.error == ErrorClass::Maintenance) return result;
    if (resp.status == 413) { result.error = ErrorClass::None; result.too_large = true; return result; }
    if (resp.status == 400) { result.error = ErrorClass::None; result.unsupported = true; return result; }
    if (resp.status == 404) { result.error = ErrorClass::None; result.not_found = true; return result; }
    if (resp.status < 200 || resp.status >= 300) {
        result.rejected = true;  // error stays ErrorClass::Server
        return result;
    }
    auto doc = json::parse(resp.body);
    auto filename = doc && doc->is_object() ? doc->get_string("filename") : std::nullopt;
    auto path = filename ? validate_attachment_path(*filename) : std::nullopt;
    if (!path) {
        result.rejected = true;
        result.error = ErrorClass::Server;
        result.detail = "the server's answer is not a valid file path";
        return result;
    }
    result.remote_path = *path;
    return result;
}

AttachmentProbeResult ApiClient::probe_attachment(int64_t note_remote_id, const std::string& remote_path) {
    AttachmentProbeResult result;
    Request req = make_request("HEAD", attachment_url(note_remote_id, remote_path));
    req.connect_timeout_s = 3;
    req.total_timeout_s = 3;
    const Response resp = transport_.perform(req);
    result.status = resp.status;
    const ErrorClass cls = classify_response(resp);
    if (resp.status == 0 || cls == ErrorClass::Auth || cls == ErrorClass::Maintenance) {
        result.error = cls;
        return result;
    }
    if (resp.status == 404) {
        result.not_found = true;
        return result;
    }
    if (resp.status >= 200 && resp.status < 300) result.size = parse_length(header_value(resp.headers, "Content-Length"));
    return result;  // any other status: the size is simply not known
}

}  // namespace pbnotes
