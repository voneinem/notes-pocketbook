#pragma once
// A small stateful fake of the Nextcloud Notes API v1 for integration tests: it keeps notes,
// validates If-Match / If-None-Match like the real server is expected to, and can be edited
// "on the server" between sync runs. Plug it into FakeTransport::set_handler.

#include <ctime>
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
    std::vector<std::string> log;     // "METHOD /path" in arrival order

    pbnotes::Response handle(const pbnotes::Request& req) {
        const std::string marker = "/index.php/apps/notes/api/v1/notes";
        const size_t at = req.url.find(marker);
        if (req.url.find("/ocs/v2.php/cloud/capabilities") != std::string::npos) {
            log.push_back("GET /capabilities");
            return ok(R"({"ocs":{"data":{"capabilities":{"notes":{"api_version":["0.2","1.0","1.3"]}}}}})");
        }
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
    static std::string note_json(const Note& n) {
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
        return ok(body, {{"ETag", collection_etag()}, {"Last-Modified", http_date(clock)}, {"X-Notes-API-Versions", "1.0, 1.3"}});
    }
    void apply_body(Note& n, const pbnotes::Request& req) {
        auto doc = pbnotes::json::parse(req.body);
        if (!doc) return;
        if (auto v = doc->get_string("content")) n.content = *v;
        if (auto v = doc->get_string("category")) n.category = *v;
        if (auto v = doc->get_bool("favorite")) n.favorite = *v;
        n.etag = "s" + std::to_string(++counter_);
        n.modified = ++clock;
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
