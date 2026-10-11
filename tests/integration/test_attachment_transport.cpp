// US4 (spec 002 FR-019, research R-6): the real CurlTransport streams a download into a file, stops at the
// size cap, answers HEAD without a body, and keeps the TLS and redirect rules for these requests.
// Needs the `openssl` CLI.
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>

#include "pbnotes/curl_transport.h"
#include "test_support.h"
#include "tls_test_server.h"

using pbnotes::CurlTransport;
using pbnotes::ErrorClass;
using pbnotes::Request;
using pbtest::TlsTestServer;

namespace {

struct Env {
    pbtest::TmpDir dir;
    pbtest::TestPki pki{dir};
    pbtest::TestPki::Leaf leaf;
    Env() { leaf = pki.available() ? pki.issue("good", "DNS:localhost,IP:127.0.0.1") : pbtest::TestPki::Leaf(); }
    bool ok() const { return pki.available() && leaf.ok; }
};

// A response with a chunked body (no Content-Length): `total` bytes in chunks of 100.
std::string chunked_response(size_t total) {
    std::string out = "HTTP/1.1 200 X\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n";
    size_t sent = 0;
    while (sent < total) {
        const size_t n = std::min<size_t>(100, total - sent);
        char head[16];
        std::snprintf(head, sizeof head, "%zx\r\n", n);
        out += head + std::string(n, 'z') + "\r\n";
        sent += n;
    }
    return out + "0\r\n\r\n";
}

Request download_request(const TlsTestServer& server, const std::string& file, long long max_bytes) {
    Request req;
    req.url = server.url("/attachment");
    req.username = "alice";
    req.password = "app-secret";
    req.save_body_to = file;
    req.max_body_bytes = max_bytes;
    return req;
}

bool exists(const std::string& path) { return std::ifstream(path).good(); }

}  // namespace

PB_TEST(a_download_is_streamed_into_the_file_and_not_into_memory) {
    Env e;
    if (!e.pki.available()) PB_SKIP("openssl CLI not available");
    PB_CHECK(e.leaf.ok);
    const std::string body = std::string(300000, 'a') + "END";
    TlsTestServer server(e.leaf.cert, e.leaf.key, [&](const std::string&) { return TlsTestServer::http_response(200, body); });
    CurlTransport transport(e.pki.ca_cert());
    auto resp = transport.perform(download_request(server, e.dir.file("a.part"), 0));
    PB_CHECK(resp.error == ErrorClass::None);
    PB_CHECK_EQ(resp.status, 200L);
    PB_CHECK(resp.body.empty());
    PB_CHECK(!resp.body_too_large);
    PB_CHECK_EQ(e.dir.read("a.part"), body);
}

PB_TEST(a_content_length_over_the_cap_stops_the_transfer_and_leaves_no_file) {
    Env e;
    if (!e.pki.available()) PB_SKIP("openssl CLI not available");
    PB_CHECK(e.leaf.ok);
    TlsTestServer server(e.leaf.cert, e.leaf.key, [](const std::string&) { return TlsTestServer::http_response(200, std::string(5000, 'x')); });
    CurlTransport transport(e.pki.ca_cert());
    auto resp = transport.perform(download_request(server, e.dir.file("a.part"), 1000));
    PB_CHECK(resp.body_too_large);
    PB_CHECK_EQ(resp.status, 200L);   // the server's own status
    PB_CHECK(!exists(e.dir.file("a.part")));
}

PB_TEST(a_chunked_body_over_the_cap_is_stopped_by_the_running_total) {
    Env e;
    if (!e.pki.available()) PB_SKIP("openssl CLI not available");
    PB_CHECK(e.leaf.ok);
    TlsTestServer server(e.leaf.cert, e.leaf.key, [](const std::string&) { return chunked_response(5000); });
    CurlTransport transport(e.pki.ca_cert());
    auto resp = transport.perform(download_request(server, e.dir.file("a.part"), 1000));
    PB_CHECK(resp.body_too_large);
    PB_CHECK(!exists(e.dir.file("a.part")));
}

PB_TEST(a_body_exactly_at_the_cap_is_accepted) {
    Env e;
    if (!e.pki.available()) PB_SKIP("openssl CLI not available");
    PB_CHECK(e.leaf.ok);
    TlsTestServer server(e.leaf.cert, e.leaf.key, [](const std::string&) { return TlsTestServer::http_response(200, std::string(1000, 'x')); });
    CurlTransport transport(e.pki.ca_cert());
    auto resp = transport.perform(download_request(server, e.dir.file("a.part"), 1000));
    PB_CHECK(!resp.body_too_large);
    PB_CHECK_EQ(e.dir.read("a.part").size(), static_cast<size_t>(1000));
}

PB_TEST(an_error_status_does_not_create_a_file) {
    Env e;
    if (!e.pki.available()) PB_SKIP("openssl CLI not available");
    PB_CHECK(e.leaf.ok);
    TlsTestServer server(e.leaf.cert, e.leaf.key, [](const std::string&) { return TlsTestServer::http_response(404, "{\"message\":\"nope\"}"); });
    CurlTransport transport(e.pki.ca_cert());
    auto resp = transport.perform(download_request(server, e.dir.file("a.part"), 1000));
    PB_CHECK_EQ(resp.status, 404L);
    PB_CHECK(!exists(e.dir.file("a.part")));
    PB_CHECK(resp.body.find("nope") != std::string::npos);   // the (small) error body stays readable
}

PB_TEST(head_returns_the_headers_without_a_body) {
    Env e;
    if (!e.pki.available()) PB_SKIP("openssl CLI not available");
    PB_CHECK(e.leaf.ok);
    TlsTestServer server(e.leaf.cert, e.leaf.key, [](const std::string&) {
        // a HEAD answer announces the length but carries no body
        return std::string("HTTP/1.1 200 X\r\nContent-Length: 123456\r\nConnection: close\r\n\r\n");
    });
    CurlTransport transport(e.pki.ca_cert());
    Request req;
    req.method = "HEAD";
    req.url = server.url("/attachment");
    req.username = "alice";
    req.password = "app-secret";
    auto resp = transport.perform(req);
    PB_CHECK(resp.error == ErrorClass::None);
    PB_CHECK_EQ(resp.status, 200L);
    PB_CHECK_EQ(pbnotes::header_value(resp.headers, "Content-Length"), std::string("123456"));
    PB_CHECK(resp.body.empty());
    PB_CHECK(server.requests().at(0).compare(0, 5, "HEAD ") == 0);
}

PB_TEST(a_certificate_failure_sends_nothing_and_creates_no_file) {
    Env e;
    if (!e.pki.available()) PB_SKIP("openssl CLI not available");
    auto stranger = e.pki.self_signed("stranger", "DNS:localhost,IP:127.0.0.1");
    PB_CHECK(stranger.ok);
    TlsTestServer server(stranger.cert, stranger.key, [](const std::string&) { return TlsTestServer::http_response(200, "secret"); });
    CurlTransport transport(e.pki.ca_cert());
    auto resp = transport.perform(download_request(server, e.dir.file("a.part"), 1000));
    PB_CHECK(resp.error == ErrorClass::Tls);
    PB_CHECK(!server.saw_authorization());
    PB_CHECK(!exists(e.dir.file("a.part")));
}

PB_TEST(a_redirect_to_another_host_does_not_forward_credentials_for_downloads) {
    Env e;
    if (!e.pki.available()) PB_SKIP("openssl CLI not available");
    PB_CHECK(e.leaf.ok);
    TlsTestServer serverB(e.leaf.cert, e.leaf.key, [](const std::string&) { return TlsTestServer::http_response(200, "from-B"); });
    const std::string target = serverB.url("/target");
    TlsTestServer serverA(e.leaf.cert, e.leaf.key, [&](const std::string&) {
        return TlsTestServer::http_response(302, "", "Location: " + target + "\r\n");
    });
    CurlTransport transport(e.pki.ca_cert());
    auto resp = transport.perform(download_request(serverA, e.dir.file("a.part"), 1000));
    PB_CHECK(resp.error == ErrorClass::None);
    PB_CHECK_EQ(e.dir.read("a.part"), std::string("from-B"));
    PB_CHECK(serverA.saw_authorization());
    PB_CHECK(!serverB.saw_authorization());
}

PB_TEST(a_redirect_to_plain_http_is_refused_for_downloads) {
    Env e;
    if (!e.pki.available()) PB_SKIP("openssl CLI not available");
    PB_CHECK(e.leaf.ok);
    TlsTestServer serverB(e.leaf.cert, e.leaf.key, [](const std::string&) { return TlsTestServer::http_response(200, "from-B"); });
    const std::string insecure = "http://localhost:" + std::to_string(serverB.port()) + "/target";
    TlsTestServer serverA(e.leaf.cert, e.leaf.key, [&](const std::string&) {
        return TlsTestServer::http_response(302, "", "Location: " + insecure + "\r\n");
    });
    CurlTransport transport(e.pki.ca_cert());
    auto resp = transport.perform(download_request(serverA, e.dir.file("a.part"), 1000));
    PB_CHECK(resp.error != ErrorClass::None);
    PB_CHECK(serverB.requests().empty());
    PB_CHECK(!exists(e.dir.file("a.part")));
}

PB_TEST_MAIN()
