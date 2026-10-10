// T025: URL validation/normalization and the connection test's classification.
#include <fstream>
#include <iterator>
#include <string>

#include "fake_transport.h"
#include "pbnotes/connection_test.h"
#include "test_support.h"

using pbnotes::ConnectionTester;
using pbnotes::normalize_server_url;
using pbtest::FakeTransport;

namespace {
std::string fixture(const std::string& name) {
    std::ifstream f(std::string(PBNOTES_FIXTURE_DIR) + "/" + name, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
}  // namespace

PB_TEST(https_url_is_normalized) {
    auto r = normalize_server_url("  https://Cloud.Example.com/  ");
    PB_CHECK(r.ok);
    PB_CHECK_EQ(r.normalized, std::string("https://cloud.example.com"));
}

PB_TEST(sub_path_and_port_are_kept) {
    PB_CHECK_EQ(normalize_server_url("https://host.example.com/nextcloud/").normalized, std::string("https://host.example.com/nextcloud"));
    PB_CHECK_EQ(normalize_server_url("https://host.example.com:8443").normalized, std::string("https://host.example.com:8443"));
}

PB_TEST(http_and_other_schemes_are_rejected_with_an_explanation) {
    for (const char* u : {"http://host.example.com", "ftp://host.example.com", "host.example.com", "//host.example.com"}) {
        auto r = normalize_server_url(u);
        PB_CHECK(!r.ok);
        PB_CHECK_EQ(r.error_key, std::string("url_not_https"));
    }
}

PB_TEST(empty_and_malformed_urls_are_rejected) {
    PB_CHECK_EQ(normalize_server_url("").error_key, std::string("url_empty"));
    PB_CHECK_EQ(normalize_server_url("   ").error_key, std::string("url_empty"));
    for (const char* u : {"https://", "https:///path", "https://host with space", "https://user:pw@host.example.com",
                          "https://host.example.com?x=1", "https://host.example.com/#frag"}) {
        auto r = normalize_server_url(u);
        PB_CHECK(!r.ok);
        PB_CHECK_EQ(r.error_key, std::string("url_invalid"));
    }
}

PB_TEST(missing_username_or_password_is_rejected_without_any_request) {
    FakeTransport t;
    ConnectionTester tester(t, pbnotes::CaSource::Bundled);
    auto r1 = tester.test("https://cloud.example.com", "", "pw");
    auto r2 = tester.test("https://cloud.example.com", "alice", "");
    PB_CHECK(!r1.ok && !r2.ok);
    PB_CHECK_EQ(r1.message_key, std::string("credentials_missing"));
    PB_CHECK_EQ(r2.message_key, std::string("credentials_missing"));
    PB_CHECK(t.requests().empty());
}

PB_TEST(http_url_never_reaches_the_network) {
    FakeTransport t;
    ConnectionTester tester(t, pbnotes::CaSource::Bundled);
    auto r = tester.test("http://cloud.example.com", "alice", "pw");
    PB_CHECK(!r.ok);
    PB_CHECK_EQ(r.message_key, std::string("url_not_https"));
    PB_CHECK(t.requests().empty());
}

PB_TEST(success_reports_the_normalized_url_and_ca_source) {
    FakeTransport t;
    t.expect("GET", "capabilities", FakeTransport::ok(fixture("capabilities_ok.unverified.json")));
    ConnectionTester tester(t, pbnotes::CaSource::Device);
    auto r = tester.test("https://Cloud.Example.com/", "alice", "pw");
    PB_CHECK(r.ok);
    PB_CHECK_EQ(r.message_key, std::string("ok"));
    PB_CHECK_EQ(r.normalized_url, std::string("https://cloud.example.com"));
    PB_CHECK(r.ca_source == pbnotes::CaSource::Device);
}

PB_TEST(failures_get_distinct_messages) {
    struct Case {
        pbnotes::Response response;
        const char* key;
        pbnotes::ErrorClass cls;
    };
    pbnotes::Response net;
    net.error = pbnotes::ErrorClass::Network;
    net.error_detail = "curl 6: Could not resolve host";
    pbnotes::Response tls;
    tls.error = pbnotes::ErrorClass::Tls;
    const Case cases[] = {
        {FakeTransport::status(401), "auth_rejected", pbnotes::ErrorClass::Auth},
        {net, "server_unreachable", pbnotes::ErrorClass::Network},
        {tls, "certificate_unverifiable", pbnotes::ErrorClass::Tls},
        {FakeTransport::ok(fixture("capabilities_only_v02.unverified.json")), "server_unsupported", pbnotes::ErrorClass::Unsupported},
        {FakeTransport::status(503), "server_maintenance", pbnotes::ErrorClass::Maintenance},
        {FakeTransport::ok("garbage"), "server_error", pbnotes::ErrorClass::Server},
    };
    for (const auto& c : cases) {
        FakeTransport t;
        t.expect("GET", "capabilities", c.response);
        ConnectionTester tester(t, pbnotes::CaSource::Bundled);
        auto r = tester.test("https://cloud.example.com", "alice", "pw");
        PB_CHECK(!r.ok);
        PB_CHECK_EQ(r.message_key, std::string(c.key));
        PB_CHECK(r.error_class == c.cls);
        if (c.cls == pbnotes::ErrorClass::Network) PB_CHECK_EQ(r.detail, std::string("curl 6: Could not resolve host"));
    }
}

PB_TEST_MAIN()
