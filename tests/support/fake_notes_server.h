#pragma once
// A small stateful fake of the Nextcloud Notes API v1 for integration tests: it keeps notes,
// validates If-Match / If-None-Match like the real server is expected to, and can be edited
// "on the server" between sync runs. Plug it into FakeTransport::set_handler.

#include <cstdio>
#include <ctime>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "pbnotes/http_transport.h"
#include "pbnotes/json.h"

namespace pbtest {

class FakeNotesServer {
public:
    struct Note {
        int64_t id = 0;
        std::string etag, content, category;
        std::string title;  // explicit server title (the real server uses the file name); empty = derived
        bool favorite = false;
        int64_t modified = 0;
    };

    std::map<int64_t, Note> notes;
    int64_t next_id = 1;
    int64_t clock = 1700000000;       // "server time", advanced on every change
    bool honor_if_match_on_put = true;
    bool honor_if_match_on_delete = false;  // the real server's behavior is a VERIFY item
    bool maintenance = false;
    bool escape_non_ascii = false;    // answer with \uXXXX escapes (surrogate pairs) instead of raw UTF-8
    std::vector<std::string> log;     // "METHOD /path" in arrival order
    std::vector<std::string> write_bodies;   // the JSON body of every POST/PUT of a note, in arrival order
    // Optional: what the real server may do to an explicit title (make it unique, replace characters).
    std::function<std::string(const std::string&)> adjust_title;
    // Notes API versions the server announces (the X-Notes-API-Versions header and the capabilities answer).
    std::string api_versions_header = "1.0, 1.3";

    // Attachments (spec 002): files by (note id, decoded path). GET/HEAD on the v1.4 attachment route.
    std::map<std::pair<int64_t, std::string>, std::string> attachments;
    int attachment_requests = 0;   // every request that reached the attachment route
    struct Upload {
        int64_t note_id = 0;
        std::string filename;      // the name the server stored it under, ".attachments.<note id>/<name>"
        std::string bytes;
    };
    std::vector<Upload> uploads;   // every file the server accepted, in order
    long upload_fail_status = 0;   // != 0: uploads are answered with this status (for example 413)
    int upload_network_failures = 0;   // the next n uploads fail with a transport error before reaching the server
    bool lose_next_upload_response = false;   // the next upload is stored, but its answer never arrives
    bool head_has_length = true;   // HEAD answers with Content-Length
    void add_attachment(int64_t note_id, const std::string& path, const std::string& bytes) {
        attachments[{note_id, path}] = bytes;
    }

    pbnotes::Response handle(const pbnotes::Request& req) {
        const std::string marker = "/index.php/apps/notes/api/v1/notes";
        const size_t at = req.url.find(marker);
        if (req.url.find("/ocs/v2.php/cloud/capabilities") != std::string::npos) {
            log.push_back("GET /capabilities");
            return ok("{\"ocs\":{\"data\":{\"capabilities\":{\"notes\":{\"api_version\":" + versions_json() + "}}}}}");
        }
        if (req.url.find("/index.php/apps/notes/api/v1.4/attachment/") != std::string::npos) return attachment(req);
        if (at == std::string::npos) return status(404);
        std::string rest = req.url.substr(at + marker.size());
        std::string query;
        const size_t q = rest.find('?');
        if (q != std::string::npos) {
            query = rest.substr(q + 1);
            rest = rest.substr(0, q);
        }
        log.push_back(req.method + " " + (rest.empty() ? "/notes" : "/notes" + rest) + (query.empty() ? "" : "?" + query));
        if (maintenance) return status(503);

        if (rest.empty()) {
            if (req.method == "GET") return list(req, query);
            if (req.method == "POST") return create(req);
            return status(405);
        }
        const int64_t id = std::atoll(rest.c_str() + 1);
        auto it = notes.find(id);
        if (req.method == "GET") {
            if (it == notes.end()) return status(404);
            if (matches(pbnotes::header_value(req.headers, "If-None-Match"), it->second.etag)) return status(304);
            return ok(note_json(it->second));
        }
        if (req.method == "PUT") {
            if (it == notes.end()) return status(404);
            const std::string im = pbnotes::header_value(req.headers, "If-Match");
            if (honor_if_match_on_put && !im.empty() && !matches(im, it->second.etag)) return status(412);
            return update(it->second, req);
        }
        if (req.method == "DELETE") {
            if (it == notes.end()) return status(404);
            const std::string im = pbnotes::header_value(req.headers, "If-Match");
            if (honor_if_match_on_delete && !im.empty() && !matches(im, it->second.etag)) return status(412);
            notes.erase(it);
            bump();
            return ok("{}");
        }
        return status(405);
    }

    // Simulates a rename made elsewhere (web UI, another device).
    void rename_on_server(int64_t id, const std::string& title) {
        Note& n = notes.at(id);
        n.title = title;
        n.etag = "s" + std::to_string(++counter_);
        n.modified = ++clock;
        bump();
    }
    // Simulates an edit made elsewhere (web UI, another device).
    void edit_on_server(int64_t id, const std::string& content) {
        Note& n = notes.at(id);
        n.content = content;
        n.etag = "s" + std::to_string(++counter_);
        n.modified = ++clock;
        bump();
    }
    int64_t add_on_server(const std::string& content, const std::string& category = "", bool favorite = false) {
        Note n;
        n.id = next_id++;
        n.content = content;
        n.category = category;
        n.favorite = favorite;
        n.etag = "s" + std::to_string(++counter_);
        n.modified = ++clock;
        notes[n.id] = n;
        bump();
        return n.id;
    }
    int count_with_content(const std::string& content) const {
        int c = 0;
        for (const auto& kv : notes)
            if (kv.second.content == content) ++c;
        return c;
    }
    int requests_matching(const std::string& prefix) const {
        int c = 0;
        for (const auto& l : log)
            if (l.compare(0, prefix.size(), prefix) == 0) ++c;
        return c;
    }

private:
    int counter_ = 0;
    int version_ = 1;

    void bump() { ++version_; }
    static bool matches(const std::string& header, const std::string& etag) {
        return header == etag || header == "\"" + etag + "\"";
    }
    static pbnotes::Response ok(const std::string& body, pbnotes::Headers h = {}) {
        pbnotes::Response r;
        r.status = 200;
        r.body = body;
        r.headers = std::move(h);
        return r;
    }
    static pbnotes::Response status(long code) {
        pbnotes::Response r;
        r.status = code;
        return r;
    }
    static std::string title_of(const std::string& content) {
        size_t b = content.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) return "New note";
        size_t e = content.find('\n', b);
        return content.substr(b, e == std::string::npos ? std::string::npos : e - b);
    }
    // Rewrites every non-ASCII character of a JSON text as \uXXXX (a surrogate pair above U+FFFF).
    static std::string escape_unicode(const std::string& json) {
        std::string out;
        char buf[16];
        for (size_t i = 0; i < json.size();) {
            const unsigned char c = static_cast<unsigned char>(json[i]);
            if (c < 0x80) { out.push_back(static_cast<char>(c)); ++i; continue; }
            const size_t n = (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : 4;
            uint32_t cp = n == 2 ? (c & 0x1F) : n == 3 ? (c & 0x0F) : (c & 0x07);
            for (size_t k = 1; k < n && i + k < json.size(); ++k) cp = (cp << 6) | (static_cast<unsigned char>(json[i + k]) & 0x3F);
            i += n;
            if (cp >= 0x10000) {
                cp -= 0x10000;
                std::snprintf(buf, sizeof buf, "\\u%04X\\u%04X", 0xD800 + (cp >> 10), 0xDC00 + (cp & 0x3FF));
            } else {
                std::snprintf(buf, sizeof buf, "\\u%04X", cp);
            }
            out += buf;
        }
        return out;
    }
    std::string note_json(const Note& n) const {
        const std::string json = note_json_raw(n);
        return escape_non_ascii ? escape_unicode(json) : json;
    }
    static std::string note_json_raw(const Note& n) {
        return "{\"id\":" + std::to_string(n.id) + ",\"etag\":" + pbnotes::json::quote(n.etag) +
               ",\"readonly\":false,\"title\":" + pbnotes::json::quote(n.title.empty() ? title_of(n.content) : n.title) +
               ",\"content\":" + pbnotes::json::quote(n.content) + ",\"category\":" + pbnotes::json::quote(n.category) +
               ",\"favorite\":" + (n.favorite ? "true" : "false") + ",\"modified\":" + std::to_string(n.modified) + "}";
    }
    static std::string http_date(int64_t t) {
        std::time_t tt = static_cast<std::time_t>(t);
        std::tm tm{};
        gmtime_r(&tt, &tm);
        char buf[64];
        std::strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S GMT", &tm);
        return buf;
    }
    std::string collection_etag() const { return "\"coll-" + std::to_string(version_) + "\""; }

    pbnotes::Response list(const pbnotes::Request& req, const std::string& query) {
        const std::string inm = pbnotes::header_value(req.headers, "If-None-Match");
        if (!inm.empty() && inm == collection_etag()) return status(304);
        int64_t prune = 0;
        const size_t p = query.find("pruneBefore=");
        if (p != std::string::npos) prune = std::atoll(query.c_str() + p + 12);
        std::string body = "[";
        bool first = true;
        for (const auto& kv : notes) {
            if (!first) body += ",";
            first = false;
            if (prune > 0 && kv.second.modified < prune) body += "{\"id\":" + std::to_string(kv.first) + "}";
            else body += note_json(kv.second);
        }
        body += "]";
        return ok(body, {{"ETag", collection_etag()}, {"Last-Modified", http_date(clock)}, {"X-Notes-API-Versions", api_versions_header}});
    }
    void apply_body(Note& n, const pbnotes::Request& req) {
        auto doc = pbnotes::json::parse(req.body);
        if (!doc) return;
        write_bodies.push_back(req.body);
        if (auto v = doc->get_string("title")) n.title = adjust_title ? adjust_title(*v) : *v;
        if (auto v = doc->get_string("content")) n.content = *v;
        if (auto v = doc->get_string("category")) n.category = *v;
        if (auto v = doc->get_bool("favorite")) n.favorite = *v;
        n.etag = "s" + std::to_string(++counter_);
        n.modified = ++clock;
    }
    std::string versions_json() const {
        std::string out = "[\"0.2\"";
        size_t pos = 0;
        while (pos < api_versions_header.size()) {
            size_t end = api_versions_header.find(',', pos);
            if (end == std::string::npos) end = api_versions_header.size();
            std::string tok = api_versions_header.substr(pos, end - pos);
            const size_t b = tok.find_first_not_of(' ');
            if (b != std::string::npos) out += ",\"" + tok.substr(b) + "\"";
            pos = end + 1;
        }
        return out + "]";
    }
    static std::string percent_decode(const std::string& s) {
        std::string out;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '%' && i + 2 <= s.size() - 1) {
                out.push_back(static_cast<char>(std::stoi(s.substr(i + 1, 2), nullptr, 16)));
                i += 2;
            } else {
                out.push_back(s[i]);
            }
        }
        return out;
    }
    pbnotes::Response attachment(const pbnotes::Request& req) {
        ++attachment_requests;
        const std::string marker = "/api/v1.4/attachment/";
        std::string rest = req.url.substr(req.url.find(marker) + marker.size());
        std::string query;
        const size_t q = rest.find('?');
        if (q != std::string::npos) {
            query = rest.substr(q + 1);
            rest = rest.substr(0, q);
        }
        log.push_back(req.method + " /attachment/" + rest);
        if (maintenance) return status(503);
        if (req.method == "POST") return upload(req, std::atoll(rest.c_str()));
        const size_t p = query.find("path=");
        const std::string path = p == std::string::npos ? "" : percent_decode(query.substr(p + 5));
        auto it = attachments.find({std::atoll(rest.c_str()), path});
        if (it == attachments.end()) return ok_status(404, "{\"message\":\"File not found\"}");
        const std::string length = std::to_string(it->second.size());
        if (req.method == "HEAD") {
            pbnotes::Headers h;
            if (head_has_length) h.emplace_back("Content-Length", length);
            return ok("", h);
        }
        if (req.method == "GET") return ok(it->second, {{"Content-Length", length}});
        return status(405);
    }
    // POST .../attachment/<note id>: multipart upload of one file; the server keeps the name, or adds " (2)",
    // " (3)", ... before the extension when it is taken.
    pbnotes::Response upload(const pbnotes::Request& req, int64_t note_id) {
        if (upload_network_failures > 0) {
            --upload_network_failures;
            pbnotes::Response r;
            r.error = pbnotes::ErrorClass::Network;
            r.error_detail = "injected failure";
            return r;
        }
        if (notes.find(note_id) == notes.end()) return ok_status(404, "{\"message\":\"Note not found\"}");
        if (upload_fail_status != 0) return ok_status(upload_fail_status, "{\"message\":\"rejected\"}");
        std::ifstream f(req.upload_file, std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        std::string name = req.upload_file_name;
        std::string stored = name;
        for (int n = 2; attachments.count({note_id, ".attachments." + std::to_string(note_id) + "/" + stored}); ++n) {
            const size_t dot = name.rfind('.');
            const std::string suffix = " (" + std::to_string(n) + ")";
            stored = dot == std::string::npos || dot == 0 ? name + suffix : name.substr(0, dot) + suffix + name.substr(dot);
        }
        const std::string path = ".attachments." + std::to_string(note_id) + "/" + stored;
        attachments[{note_id, path}] = bytes;
        uploads.push_back({note_id, path, bytes});
        if (lose_next_upload_response) {
            lose_next_upload_response = false;
            pbnotes::Response r;
            r.error = pbnotes::ErrorClass::Network;
            r.error_detail = "the answer was lost";
            return r;
        }
        return ok("{\"filename\":" + pbnotes::json::quote(path) + "}");
    }
    static pbnotes::Response ok_status(long code, const std::string& body) {
        pbnotes::Response r;
        r.status = code;
        r.body = body;
        return r;
    }
    pbnotes::Response create(const pbnotes::Request& req) {
        Note n;
        n.id = next_id++;
        apply_body(n, req);
        notes[n.id] = n;
        bump();
        return ok(note_json(n));
    }
    pbnotes::Response update(Note& n, const pbnotes::Request& req) {
        apply_body(n, req);
        bump();
        return ok(note_json(n));
    }
};

}  // namespace pbtest
