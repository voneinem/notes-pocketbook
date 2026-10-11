#include "pbnotes/about.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>

#include "pbnotes/log.h"

namespace pbnotes {
namespace about {

namespace {

constexpr size_t kMaxManifestBytes = 64 * 1024;
constexpr size_t kMaxTextBytes = 256 * 1024;
constexpr size_t kMaxParagraphs = 5000;
constexpr size_t kMaxBlocks = 64;
constexpr size_t kMaxIdLen = 40, kMaxNameLen = 80, kMaxPurposeLen = 120, kMaxLicenseLen = 120,
                 kMaxUrlLen = 200, kMaxPathLen = 200, kMaxFreeLen = 200;

std::string trim(std::string_view s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return std::string(s.substr(b, e - b));
}

std::vector<std::string> split_list(const std::string& value) {
    std::vector<std::string> out;
    size_t pos = 0;
    while (pos <= value.size()) {
        size_t comma = value.find(',', pos);
        if (comma == std::string::npos) comma = value.size();
        std::string item = trim(std::string_view(value).substr(pos, comma - pos));
        if (!item.empty()) out.push_back(item);
        pos = comma + 1;
    }
    return out;
}

bool valid_id(const std::string& id) {
    if (id.empty() || id.size() > kMaxIdLen) return false;
    for (char c : id) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
    }
    return true;
}

// Relative path below the licenses root: no leading '/', no ".." segment, no backslash, no NUL.
bool valid_text_path(const std::string& p) {
    if (p.empty() || p.size() > kMaxPathLen || p[0] == '/') return false;
    if (p.find('\\') != std::string::npos || p.find('\0') != std::string::npos) return false;
    size_t pos = 0;
    while (pos <= p.size()) {
        size_t slash = p.find('/', pos);
        if (slash == std::string::npos) slash = p.size();
        if (p.compare(pos, slash - pos, "..") == 0) return false;
        pos = slash + 1;
    }
    return true;
}

bool valid_key(std::string_view k) {
    if (k.empty() || !(k[0] >= 'a' && k[0] <= 'z')) return false;
    for (char c : k) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

// Builds an entry from the key/value lines of one block; false = bad block.
bool build_entry(const std::map<std::string, std::string>& kv, Entry& e, bool& is_app) {
    auto get = [&](const char* key) {
        auto it = kv.find(key);
        return it == kv.end() ? std::string() : it->second;
    };
    const std::string kind = get("kind");
    if (kind == "app") is_app = true;
    else if (kind == "component") is_app = false;
    else return false;

    e.id = get("id");
    e.name = get("name");
    e.license = get("license");
    e.holder = get("holder");
    if (!valid_id(e.id)) return false;
    if (e.name.empty() || e.name.size() > kMaxNameLen) return false;
    if (e.license.empty() || e.license.size() > kMaxLicenseLen) return false;
    if (e.holder.empty() || e.holder.size() > kMaxFreeLen) return false;

    for (const std::string& t : split_list(get("text"))) {
        if (!valid_text_path(t)) return false;
        e.text.push_back(t);
    }
    // a raw NUL anywhere in the list value is caught by the path check only if it survives trimming
    if (get("text").find('\0') != std::string::npos) return false;
    e.source = split_list(get("source"));
    e.cmake = split_list(get("cmake"));

    if (is_app) {
        e.copyright = get("copyright");
        if (e.copyright.empty() || e.copyright.size() > kMaxFreeLen) return false;
        return true;
    }
    e.purpose = get("purpose");
    e.url = get("url");
    if (e.purpose.empty() || e.purpose.size() > kMaxPurposeLen) return false;
    if (e.url.size() <= 8 || e.url.size() > kMaxUrlLen || e.url.compare(0, 8, "https://") != 0) return false;
    const std::string origin = get("origin");
    if (origin == "bundled") e.origin = Origin::Bundled;
    else if (origin == "system") e.origin = Origin::System;
    else return false;
    return true;
}

}  // namespace

Manifest parse_manifest(std::string_view content) {
    Manifest m;
    if (content.size() > kMaxManifestBytes) return m;  // readable stays false
    m.readable = true;
    if (content.size() >= 3 && content.compare(0, 3, "\xEF\xBB\xBF") == 0) content.remove_prefix(3);

    std::map<std::string, std::string> kv;
    bool block_open = false, block_bad = false;
    size_t blocks = 0;
    std::set<std::string> ids;

    auto close_block = [&]() {
        if (!block_open) return;
        block_open = false;
        ++blocks;
        if (blocks > kMaxBlocks) { kv.clear(); block_bad = false; return; }
        Entry e;
        bool is_app = false;
        bool ok = !block_bad && build_entry(kv, e, is_app);
        if (ok && !ids.insert(e.id).second) ok = false;     // duplicate id
        if (ok && is_app && m.app) ok = false;               // second app block
        if (ok) {
            if (is_app) m.app = std::move(e);
            else m.components.push_back(std::move(e));
        } else {
            ++m.bad_blocks;
        }
        kv.clear();
        block_bad = false;
    };

    size_t pos = 0;
    while (pos < content.size()) {
        size_t nl = content.find('\n', pos);
        if (nl == std::string_view::npos) nl = content.size();
        std::string_view raw = content.substr(pos, nl - pos);
        pos = nl + 1;
        if (!raw.empty() && raw.back() == '\r') raw.remove_suffix(1);

        if (trim(raw).empty() && raw.find('\0') == std::string_view::npos) { close_block(); continue; }
        if (!raw.empty() && raw[0] == '#') continue;

        block_open = true;
        size_t colon = raw.find(':');
        if (colon == std::string_view::npos || !valid_key(raw.substr(0, colon))) { block_bad = true; continue; }
        kv[std::string(raw.substr(0, colon))] = trim(raw.substr(colon + 1));
    }
    close_block();
    return m;
}

Manifest load_manifest(const std::string& licenses_root) {
    std::ifstream f(licenses_root + "/licenses/manifest.txt", std::ios::binary);
    if (!f) return {};
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    Manifest m = parse_manifest(data);
    if (m.bad_blocks > 0) log::event("about.bad_blocks", {log::count("blocks", m.bad_blocks)});
    return m;
}

std::vector<std::string> split_paragraphs(std::string_view text) {
    std::vector<std::string> out;
    std::string current;
    auto flush = [&]() {
        if (!current.empty()) out.push_back(std::move(current));
        current.clear();
    };
    size_t pos = 0;
    while (pos < text.size() && out.size() < kMaxParagraphs) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string_view::npos) nl = text.size();
        std::string_view line = text.substr(pos, nl - pos);
        pos = nl + 1;
        size_t end = line.size();
        while (end > 0 && (line[end - 1] == '\r' || line[end - 1] == ' ' || line[end - 1] == '\t')) --end;
        line = line.substr(0, end);
        if (line.empty() || trim(line).empty()) { flush(); continue; }
        if (!current.empty()) current += '\n';
        current += line;
    }
    if (out.size() < kMaxParagraphs) flush();
    return out;
}

LicenseText load_text(const std::string& licenses_root, const Entry& entry) {
    namespace fs = std::filesystem;
    LicenseText result;
    std::error_code ec;
    const fs::path root = fs::canonical(licenses_root, ec);
    int unreadable = 0;
    const bool several = entry.text.size() > 1;
    for (const std::string& rel : entry.text) {
        bool read = false;
        std::string data;
        if (!ec && valid_text_path(rel)) {
            std::error_code e2;
            const fs::path file = fs::canonical(root / rel, e2);
            const std::string r = root.string(), f = file.string();
            const bool inside = !e2 && f.size() > r.size() && f.compare(0, r.size(), r) == 0 &&
                                (f[r.size()] == '/' || r.back() == '/');
            if (inside && fs::is_regular_file(file, e2)) {
                std::ifstream in(file, std::ios::binary);
                data.resize(kMaxTextBytes + 1);
                in.read(&data[0], static_cast<std::streamsize>(data.size()));
                data.resize(static_cast<size_t>(in.gcount()));
                if (data.size() > kMaxTextBytes) {
                    data.resize(kMaxTextBytes);
                    size_t cut = data.rfind('\n');
                    data.resize(cut == std::string::npos ? 0 : cut + 1);
                    result.truncated = true;
                }
                read = true;
            }
        }
        std::vector<std::string> paras = read ? split_paragraphs(data) : std::vector<std::string>();
        if (paras.empty()) { ++unreadable; continue; }
        if (several) {
            std::string base = fs::path(rel).stem().string();
            result.paragraphs.push_back(base);
        }
        result.paragraphs.insert(result.paragraphs.end(), paras.begin(), paras.end());
        result.available = true;
    }
    if (result.truncated) result.paragraphs.push_back("Text shortened.");
    if (unreadable > 0) log::event("about.text_unavailable", {log::count("files", unreadable)});
    return result;
}

std::string display_version(std::string_view version) {
    if (version.empty() || version == "0.0.0") return "unknown";
    return std::string(version);
}

}  // namespace about
}  // namespace pbnotes
