// T103: expired and wrong-hostname certificates (both signed by a CA the client trusts) are
// refused, and no Authorization header reaches the server. Needs the `openssl` CLI.
#include <string>

#include "pbnotes/curl_transport.h"
#include "test_support.h"
#include "tls_test_server.h"

using pbnotes::CurlTransport;
using pbnotes::ErrorClass;
using pbnotes::Request;
using pbtest::TlsTestServer;

namespace {
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
}  // namespace

PB_TEST(control_valid_certificate_for_localhost_is_accepted) {
    pbtest::TmpDir d;
    pbtest::TestPki pki(d);
    if (!pki.available()) PB_SKIP("openssl CLI not available");
    auto leaf = pki.issue("good", "DNS:localhost,IP:127.0.0.1");
    PB_CHECK(leaf.ok);
    TlsTestServer server(leaf.cert, leaf.key, ok_handler());
    CurlTransport transport(pki.ca_cert());
    auto resp = transport.perform(make_request(server));
    PB_CHECK(resp.error == ErrorClass::None);  // proves the harness itself is sound
    PB_CHECK_EQ(resp.status, 200L);
}

PB_TEST(expired_certificate_is_refused_and_no_credentials_are_sent) {
    pbtest::TmpDir d;
    pbtest::TestPki pki(d);
    if (!pki.available()) PB_SKIP("openssl CLI not available");
    auto leaf = pki.issue("expired", "DNS:localhost,IP:127.0.0.1", "20200101000000Z", "20200102000000Z");
    PB_CHECK(leaf.ok);
    TlsTestServer server(leaf.cert, leaf.key, ok_handler());
    CurlTransport transport(pki.ca_cert());
    auto resp = transport.perform(make_request(server));
    PB_CHECK(resp.error == ErrorClass::Tls);
    PB_CHECK(!server.saw_authorization());
    PB_CHECK(server.requests().empty());
}

PB_TEST(wrong_hostname_is_refused_and_no_credentials_are_sent) {
    pbtest::TmpDir d;
    pbtest::TestPki pki(d);
    if (!pki.available()) PB_SKIP("openssl CLI not available");
    auto leaf = pki.issue("wronghost", "DNS:wrong.example.com");
    PB_CHECK(leaf.ok);
    TlsTestServer server(leaf.cert, leaf.key, ok_handler());
    CurlTransport transport(pki.ca_cert());
    auto resp = transport.perform(make_request(server));  // connects to "localhost"
    PB_CHECK(resp.error == ErrorClass::Tls);
    PB_CHECK(!server.saw_authorization());
    PB_CHECK(server.requests().empty());
}

PB_TEST_MAIN()
