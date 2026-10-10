// T105: build_effective_ca = base CA file + a valid custom-ca.pem; invalid custom files are ignored
// (verification never off); the output is written atomically; inputs are never modified.
#include <sys/stat.h>
#include <unistd.h>

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "pbnotes/ca_selector.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::build_effective_ca;
using pbnotes::CustomCaStatus;

namespace {

std::string slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// The first `n` real certificates from the bundled Mozilla CA file (valid PEM/DER structure).
std::string some_certs(int n) {
    const std::string all = slurp(std::string(PBNOTES_SOURCE_DIR) + "/third_party/cacert/cacert.pem");
    std::string out;
    size_t pos = 0;
    for (int i = 0; i < n; ++i) {
        size_t b = all.find("-----BEGIN CERTIFICATE-----", pos);
        size_t e = all.find("-----END CERTIFICATE-----", b);
        if (b == std::string::npos || e == std::string::npos) PB_FAIL_AT("bundled CA file has too few certificates");
        e += std::string("-----END CERTIFICATE-----").size();
        out += all.substr(b, e - b) + "\n";
        pos = e;
    }
    return out;
}

const std::string kBase = "-----BEGIN CERTIFICATE-----\nQkFTRQ==\n-----END CERTIFICATE-----\n";  // content is irrelevant for the base

}  // namespace

PB_TEST(no_custom_file_means_the_base_is_used_unchanged) {
    pbtest::TmpDir d;
    d.write("base.pem", some_certs(1));
    auto r = build_effective_ca(d.file("base.pem"), d.file("custom-ca.pem"), d.file("effective.pem"));
    PB_CHECK(r.custom_status == CustomCaStatus::None);
    PB_CHECK_EQ(r.path, d.file("base.pem"));
    PB_CHECK_EQ(r.custom_cert_count, 0);
    PB_CHECK(!d.exists("effective.pem"));
}

PB_TEST(valid_custom_file_is_appended_to_the_base) {
    pbtest::TmpDir d;
    const std::string base = some_certs(1);
    const std::string custom = some_certs(2);
    d.write("base.pem", base);
    d.write("custom-ca.pem", custom);
    auto r = build_effective_ca(d.file("base.pem"), d.file("custom-ca.pem"), d.file("effective.pem"));
    PB_CHECK(r.custom_status == CustomCaStatus::Loaded);
    PB_CHECK_EQ(r.path, d.file("effective.pem"));
    PB_CHECK_EQ(r.custom_cert_count, 2);
    const std::string eff = d.read("effective.pem");
    PB_CHECK(eff.compare(0, base.size(), base) == 0);               // base first
    PB_CHECK(eff.find(custom) != std::string::npos);                // then the custom certificates
    PB_CHECK_EQ(d.read("base.pem"), base);                          // inputs untouched
    PB_CHECK_EQ(d.read("custom-ca.pem"), custom);
}

PB_TEST(invalid_custom_files_are_ignored_and_the_base_is_used) {
    pbtest::TmpDir d;
    d.write("base.pem", some_certs(1));
    const std::string big = some_certs(1) + std::string(1024 * 1024 + 1, '#');
    const struct {
        const char* name;
        std::string content;
    } bad[] = {
        {"empty", ""},
        {"text", "this is not a certificate\n"},
        {"too_big", big},
        {"bad_base64", "-----BEGIN CERTIFICATE-----\n@@@@ not base64 @@@@\n-----END CERTIFICATE-----\n"},
        {"not_der", "-----BEGIN CERTIFICATE-----\nQUJDRA==\n-----END CERTIFICATE-----\n"},  // "ABCD", no DER SEQUENCE
        {"unterminated", "-----BEGIN CERTIFICATE-----\nMIIB\n"},
    };
    for (const auto& b : bad) {
        d.write("custom-ca.pem", b.content);
        auto r = build_effective_ca(d.file("base.pem"), d.file("custom-ca.pem"), d.file("effective.pem"));
        if (r.custom_status != CustomCaStatus::Invalid) PB_FAIL_AT(std::string("not rejected: ") + b.name);
        PB_CHECK_EQ(r.path, d.file("base.pem"));
        PB_CHECK_EQ(r.custom_cert_count, 0);
    }
}

PB_TEST(unreadable_custom_file_is_invalid) {
    if (geteuid() == 0) PB_SKIP("root can read everything");
    pbtest::TmpDir d;
    d.write("base.pem", some_certs(1));
    d.write("custom-ca.pem", some_certs(1));
    ::chmod(d.file("custom-ca.pem").c_str(), 0000);
    auto r = build_effective_ca(d.file("base.pem"), d.file("custom-ca.pem"), d.file("effective.pem"));
    PB_CHECK(r.custom_status == CustomCaStatus::Invalid);
    PB_CHECK_EQ(r.path, d.file("base.pem"));
}

PB_TEST(effective_file_follows_changes_of_either_input) {
    pbtest::TmpDir d;
    d.write("base.pem", some_certs(1));
    d.write("custom-ca.pem", some_certs(1));
    auto r1 = build_effective_ca(d.file("base.pem"), d.file("custom-ca.pem"), d.file("effective.pem"));
    PB_CHECK_EQ(r1.custom_cert_count, 1);
    d.write("custom-ca.pem", some_certs(3));
    auto r2 = build_effective_ca(d.file("base.pem"), d.file("custom-ca.pem"), d.file("effective.pem"));
    PB_CHECK_EQ(r2.custom_cert_count, 3);
    d.write("base.pem", some_certs(2));
    build_effective_ca(d.file("base.pem"), d.file("custom-ca.pem"), d.file("effective.pem"));
    PB_CHECK(d.read("effective.pem").compare(0, some_certs(2).size(), some_certs(2)) == 0);
}

PB_TEST(output_is_written_atomically_without_leftovers) {
    pbtest::TmpDir d;
    d.write("base.pem", some_certs(1));
    d.write("custom-ca.pem", some_certs(1));
    build_effective_ca(d.file("base.pem"), d.file("custom-ca.pem"), d.file("effective.pem"));
    PB_CHECK(d.exists("effective.pem"));
    PB_CHECK(!d.exists("effective.pem.tmp"));
}

PB_TEST_MAIN()
