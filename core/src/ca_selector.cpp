#include "pbnotes/ca_selector.h"

#include <sys/stat.h>

#include <cstdio>
#include <fstream>
#include <iterator>

namespace pbnotes {

bool is_usable_ca_file(const std::string& path) {
    struct stat st;
    if (::stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0) return false;
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    // CA bundles are a few hundred KB; refuse absurdly large files.
    if (st.st_size > 8 * 1024 * 1024) return false;
    const std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return data.find("-----BEGIN CERTIFICATE-----") != std::string::npos;
}

CaSelection select_ca_file(const std::vector<std::string>& device_candidates, const std::string& bundled_path) {
    for (const auto& candidate : device_candidates) {
        if (is_usable_ca_file(candidate)) return {candidate, CaSource::Device};
    }
    return {bundled_path, CaSource::Bundled};
}

namespace {

constexpr long long kMaxCustomCaBytes = 1024 * 1024;
const char kBegin[] = "-----BEGIN CERTIFICATE-----";
const char kEnd[] = "-----END CERTIFICATE-----";

bool read_file(const std::string& path, std::string* out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out->assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

// Strict base64 check: returns the first decoded byte and the decoded length via the out params.
bool decode_base64_head(const std::string& text, int* first_byte, size_t* decoded_len) {
    std::string clean;
    for (char c : text)
        if (c != '\n' && c != '\r' && c != ' ' && c != '\t') clean.push_back(c);
    if (clean.empty() || clean.size() % 4 != 0) return false;
    auto value = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    size_t pad = 0;
    for (size_t i = 0; i < clean.size(); ++i) {
        if (clean[i] == '=') {
            if (i < clean.size() - 2) return false;  // padding only at the end
            ++pad;
        } else if (pad > 0 || value(clean[i]) < 0) {
            return false;
        }
    }
    if (pad > 2) return false;
    const int a = value(clean[0]), b = value(clean[1]);
    *first_byte = (a << 2) | (b >> 4);
    *decoded_len = clean.size() / 4 * 3 - pad;
    return *decoded_len > 0;
}

// Counts well-formed certificates; returns -1 if the file is malformed or has none.
int count_valid_certificates(const std::string& data) {
    int count = 0;
    size_t pos = 0;
    while (true) {
        const size_t b = data.find(kBegin, pos);
        if (b == std::string::npos) break;
        const size_t body = b + sizeof(kBegin) - 1;
        const size_t e = data.find(kEnd, body);
        if (e == std::string::npos) return -1;  // unterminated block
        int first = 0;
        size_t len = 0;
        if (!decode_base64_head(data.substr(body, e - body), &first, &len) || first != 0x30 || len < 16) return -1;
        ++count;
        pos = e + sizeof(kEnd) - 1;
    }
    return count > 0 ? count : -1;
}

}  // namespace

EffectiveCa build_effective_ca(const std::string& base_ca_path, const std::string& custom_ca_path,
                               const std::string& out_path) {
    EffectiveCa result;
    result.path = base_ca_path;

    struct stat st;
    if (::stat(custom_ca_path.c_str(), &st) != 0) return result;  // no custom file

    result.custom_status = CustomCaStatus::Invalid;
    if (!S_ISREG(st.st_mode) || st.st_size <= 0 || st.st_size > kMaxCustomCaBytes) return result;
    std::string custom;
    if (!read_file(custom_ca_path, &custom)) return result;
    const int count = count_valid_certificates(custom);
    if (count < 0) return result;

    std::string base;
    if (!read_file(base_ca_path, &base)) {  // the base is unusable; do not hide that behind the custom file
        result.custom_status = CustomCaStatus::None;
        return result;
    }

    std::string combined = base;
    if (!combined.empty() && combined.back() != '\n') combined.push_back('\n');
    combined += custom;

    const std::string tmp = out_path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        f << combined;
        f.flush();
        if (!f) {
            std::remove(tmp.c_str());
            result.custom_status = CustomCaStatus::Invalid;
            return result;
        }
    }
    if (std::rename(tmp.c_str(), out_path.c_str()) != 0) {  // atomic replace on the same filesystem
        std::remove(tmp.c_str());
        result.custom_status = CustomCaStatus::Invalid;
        return result;
    }
    result.path = out_path;
    result.custom_status = CustomCaStatus::Loaded;
    result.custom_cert_count = count;
    return result;
}



}  // namespace pbnotes
