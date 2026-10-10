// T026: CurlTransport verifies certificates and never sends credentials to an unverified peer.
// Needs the `openssl` CLI (skipped otherwise).
#include <string>

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
    return [](const std::string&) { return TlsTestServer::http_response(200, "{\"ok\":true}"); };
}
}  // namespace

PB_TEST(self_signed_certificate_is_refused_and_no_credentials_are_sent) {
    pbtest::TmpDir d;
    pbtest::TestPki pki(d);
    if (!pki.available()) PB_SKIP("openssl CLI not available");
    auto leaf = pki.self_signed("selfsigned", "DNS:localhost,IP:127.0.0.1");
    PB_CHECK(leaf.ok);
    TlsTestServer server(leaf.cert, leaf.key, ok_handler());
    PB_CHECK(server.running());

    CurlTransport transport(bundled_ca());  // the public CAs do not know this certificate
    Request req;
    req.url = server.url("/index.php/apps/notes/api/v1/notes");
    req.username = "alice";
    req.password = "app-secret";
    auto resp = transport.perform(req);

    PB_CHECK(resp.error == ErrorClass::Tls);
    PB_CHECK_EQ(resp.status, 0L);
    PB_CHECK(server.requests().empty());  // the handshake failed, so no request was ever sent
    PB_CHECK(!server.saw_authorization());
}

PB_TEST(trusted_certificate_succeeds_and_credentials_are_sent_after_verification) {
    pbtest::TmpDir d;
    pbtest::TestPki pki(d);
    if (!pki.available()) PB_SKIP("openssl CLI not available");
    auto leaf = pki.self_signed("selfsigned", "DNS:localhost,IP:127.0.0.1");
    PB_CHECK(leaf.ok);
    TlsTestServer server(leaf.cert, leaf.key, ok_handler());
    PB_CHECK(server.running());

    CurlTransport transport(leaf.cert);  // the test CA is supplied as the trust anchor
    Request req;
    req.url = server.url("/x");
    req.username = "alice";
    req.password = "app-secret";
    auto resp = transport.perform(req);

    PB_CHECK(resp.error == ErrorClass::None);
    PB_CHECK_EQ(resp.status, 200L);
    PB_CHECK_EQ(resp.body, std::string("{\"ok\":true}"));
    PB_CHECK(server.saw_authorization());
    // "alice:app-secret" as HTTP Basic credentials, and never in the URL
    PB_CHECK(server.requests().at(0).find("Basic YWxpY2U6YXBwLXNlY3JldA==") != std::string::npos);
    PB_CHECK(server.requests().at(0).find("GET /x ") != std::string::npos);
}

PB_TEST(plain_http_urls_are_refused_before_any_connection) {
    pbtest::TmpDir d;
    pbtest::TestPki pki(d);
    if (!pki.available()) PB_SKIP("openssl CLI not available");
    auto leaf = pki.self_signed("selfsigned", "DNS:localhost,IP:127.0.0.1");
    PB_CHECK(leaf.ok);
    TlsTestServer server(leaf.cert, leaf.key, ok_handler());
    PB_CHECK(server.running());

    CurlTransport transport(leaf.cert);
    Request req;
    req.url = "http://localhost:" + std::to_string(server.port()) + "/x";
    req.username = "alice";
    req.password = "app-secret";
    auto resp = transport.perform(req);

    PB_CHECK(resp.error != ErrorClass::None);
    PB_CHECK(!resp.error_detail.empty());
    PB_CHECK(server.requests().empty());
    PB_CHECK_EQ(server.failed_handshakes(), 0);  // not even a connection attempt
}

PB_TEST_MAIN()
