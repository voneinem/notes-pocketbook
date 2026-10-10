// T104: credentials are never forwarded to another host/port on a redirect, and a redirect to a
// plain http:// URL is refused. Needs the `openssl` CLI.
#include <string>

#include "pbnotes/curl_transport.h"
#include "test_support.h"
#include "tls_test_server.h"

using pbnotes::CurlTransport;
using pbnotes::ErrorClass;
using pbnotes::Request;
using pbtest::TlsTestServer;

PB_TEST(redirect_to_another_server_does_not_forward_credentials) {
    pbtest::TmpDir d;
    pbtest::TestPki pki(d);
    if (!pki.available()) PB_SKIP("openssl CLI not available");
    auto leaf = pki.issue("good", "DNS:localhost,IP:127.0.0.1");
    PB_CHECK(leaf.ok);

    TlsTestServer serverB(leaf.cert, leaf.key, [](const std::string&) { return TlsTestServer::http_response(200, "from-B"); });
    const std::string target = serverB.url("/target");
    TlsTestServer serverA(leaf.cert, leaf.key, [&](const std::string&) {
        return TlsTestServer::http_response(302, "", "Location: " + target + "\r\n");
    });
    PB_CHECK(serverA.running() && serverB.running());

    CurlTransport transport(pki.ca_cert());
    Request req;
    req.url = serverA.url("/start");
    req.username = "alice";
    req.password = "app-secret";
    auto resp = transport.perform(req);

    PB_CHECK(resp.error == ErrorClass::None);
    PB_CHECK_EQ(resp.status, 200L);
    PB_CHECK_EQ(resp.body, std::string("from-B"));
    PB_CHECK(serverA.saw_authorization());   // the original host gets the credentials
    PB_CHECK(!serverB.saw_authorization());  // the redirect target does not
    PB_CHECK_EQ(serverB.requests().size(), static_cast<size_t>(1));
}

PB_TEST(redirect_to_plain_http_is_refused) {
    pbtest::TmpDir d;
    pbtest::TestPki pki(d);
    if (!pki.available()) PB_SKIP("openssl CLI not available");
    auto leaf = pki.issue("good", "DNS:localhost,IP:127.0.0.1");
    PB_CHECK(leaf.ok);

    TlsTestServer serverB(leaf.cert, leaf.key, [](const std::string&) { return TlsTestServer::http_response(200, "from-B"); });
    const std::string insecure = "http://localhost:" + std::to_string(serverB.port()) + "/target";
    TlsTestServer serverA(leaf.cert, leaf.key, [&](const std::string&) {
        return TlsTestServer::http_response(302, "", "Location: " + insecure + "\r\n");
    });
    PB_CHECK(serverA.running() && serverB.running());

    CurlTransport transport(pki.ca_cert());
    Request req;
    req.url = serverA.url("/start");
    req.username = "alice";
    req.password = "app-secret";
    auto resp = transport.perform(req);

    PB_CHECK(resp.error != ErrorClass::None);
    PB_CHECK(serverB.requests().empty());
    PB_CHECK_EQ(serverB.failed_handshakes(), 0);  // the client never even connected
    PB_CHECK(!serverB.saw_authorization());
}

PB_TEST_MAIN()
