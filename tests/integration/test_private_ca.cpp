// T106: a user-supplied custom-ca.pem extends the trust anchors; verification stays on.
// Needs the `openssl` CLI.
#include <fstream>
#include <iterator>
#include <string>

#include "pbnotes/ca_selector.h"
#include "pbnotes/curl_transport.h"
#include "test_support.h"
#include "tls_test_server.h"

using pbnotes::CurlTransport;
using pbnotes::ErrorClass;
using pbnotes::Request;
using pbtest::TlsTestServer;

namespace {
std::string bundled_ca() { return std::string(PBNOTES_SOURCE_DIR) + "/third_party/cacert/cacert.pem"; }
TlsTestServer::Handler ok_handler() {
    return [](const std::string&) { return TlsTestServer::http_response(200, "{}"); };
}
Request make_request(const TlsTestServer& s) {
    Request req;
    req.url = s.url("/x");
    req.username = "alice";
    req.password = "app-secret";
    return req;
}
std::string slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
// Builds the effective bundle (bundled public CAs + custom-ca.pem) for a given custom file.
std::string effective_with(const pbtest::TmpDir& d, const std::string& custom_source, const char* tag) {
    d.write(std::string("custom-") + tag + ".pem", slurp(custom_source));
    auto eff = pbnotes::build_effective_ca(bundled_ca(), d.file(std::string("custom-") + tag + ".pem"),
                                           d.file(std::string("effective-") + tag + ".pem"));
    PB_CHECK(eff.custom_status == pbnotes::CustomCaStatus::Loaded);
    return eff.path;
}
}  // namespace

PB_TEST(private_ca_is_refused_without_custom_file_and_accepted_with_it) {
    pbtest::TmpDir d;
    pbtest::TestPki pki(d, "privateca");
    if (!pki.available()) PB_SKIP("openssl CLI not available");
    auto leaf = pki.issue("server", "DNS:localhost,IP:127.0.0.1");
    PB_CHECK(leaf.ok);
    TlsTestServer server(leaf.cert, leaf.key, ok_handler());

    CurlTransport without(bundled_ca());
    auto refused = without.perform(make_request(server));
    PB_CHECK(refused.error == ErrorClass::Tls);
    PB_CHECK(!server.saw_authorization());

    CurlTransport with(effective_with(d, pki.ca_cert(), "a"));
    auto accepted = with.perform(make_request(server));
    PB_CHECK(accepted.error == ErrorClass::None);
    PB_CHECK_EQ(accepted.status, 200L);
}

PB_TEST(a_different_custom_ca_does_not_help) {
    pbtest::TmpDir d;
    pbtest::TestPki pki(d, "privateca");
    pbtest::TestPki other(d, "otherca");
    if (!pki.available() || !other.available()) PB_SKIP("openssl CLI not available");
    auto leaf = pki.issue("server", "DNS:localhost,IP:127.0.0.1");
    PB_CHECK(leaf.ok);
    TlsTestServer server(leaf.cert, leaf.key, ok_handler());

    CurlTransport transport(effective_with(d, other.ca_cert(), "b"));
    auto resp = transport.perform(make_request(server));
    PB_CHECK(resp.error == ErrorClass::Tls);
    PB_CHECK(!server.saw_authorization());
}

PB_TEST(wrong_hostname_is_still_refused_with_the_custom_ca_loaded) {
    pbtest::TmpDir d;
    pbtest::TestPki pki(d, "privateca");
    if (!pki.available()) PB_SKIP("openssl CLI not available");
    auto leaf = pki.issue("wronghost", "DNS:wrong.example.com");
    PB_CHECK(leaf.ok);
    TlsTestServer server(leaf.cert, leaf.key, ok_handler());

    CurlTransport transport(effective_with(d, pki.ca_cert(), "c"));
    auto resp = transport.perform(make_request(server));
    PB_CHECK(resp.error == ErrorClass::Tls);  // verification stays on in every case
    PB_CHECK(!server.saw_authorization());
}

PB_TEST(self_signed_server_certificate_placed_in_custom_ca_is_accepted) {
    pbtest::TmpDir d;
    pbtest::TestPki pki(d);
    if (!pki.available()) PB_SKIP("openssl CLI not available");
    auto leaf = pki.self_signed("selfsigned", "DNS:localhost,IP:127.0.0.1");
    PB_CHECK(leaf.ok);
    TlsTestServer server(leaf.cert, leaf.key, ok_handler());

    CurlTransport without(bundled_ca());
    PB_CHECK(without.perform(make_request(server)).error == ErrorClass::Tls);

    CurlTransport with(effective_with(d, leaf.cert, "d"));
    auto resp = with.perform(make_request(server));
    PB_CHECK(resp.error == ErrorClass::None);
    PB_CHECK_EQ(resp.status, 200L);
}

PB_TEST_MAIN()
