// US5 (spec 002 FR-025, research R-6): the real CurlTransport sends a multipart upload with the file's bytes
// and name intact, and keeps the TLS and redirect rules for it. Needs the `openssl` CLI.
#include <algorithm>
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
};

// Bytes that are hard on a multipart encoder: NUL, CR LF, a fake boundary, high bytes.
std::string tricky_bytes() {
    std::string s = "start\r\n--not-the-boundary\r\n";
    s.push_back('\0');
    s += "\xFF\xFE middle \r\n";
    s += std::string(70000, 'z');
    return s + "end";
}

Request upload_request(const TlsTestServer& server, const std::string& file) {
    Request req;
    req.method = "POST";
    req.url = server.url("/attachment/12");
    req.username = "alice";
    req.password = "app-secret";
    req.upload_file = file;
    req.upload_field = "file";
    req.upload_file_name = "photo one.png";
    return req;
}

}  // namespace

PB_TEST(an_upload_arrives_with_its_bytes_and_file_name_intact) {
    Env e;
    if (!e.pki.available()) PB_SKIP("openssl CLI not available");
    PB_CHECK(e.leaf.ok);
    const std::string data = tricky_bytes();
    e.dir.write("photo.png", data);
    TlsTestServer server(e.leaf.cert, e.leaf.key, [](const std::string&) {
        return TlsTestServer::http_response(200, "{\"filename\":\".attachments.12/photo one.png\"}");
    });
    CurlTransport transport(e.pki.ca_cert());
    auto resp = transport.perform(upload_request(server, e.dir.file("photo.png")));
    PB_CHECK(resp.error == ErrorClass::None);
    PB_CHECK_EQ(resp.status, 200L);
    PB_CHECK(resp.body.find("photo one.png") != std::string::npos);

    const std::string raw = server.requests().at(0);
    PB_CHECK(raw.compare(0, 5, "POST ") == 0);
    PB_CHECK(raw.find("Content-Type: multipart/form-data; boundary=") != std::string::npos);
    PB_CHECK(raw.find("Content-Disposition: form-data; name=\"file\"; filename=\"photo one.png\"") != std::string::npos);
    PB_CHECK(raw.find(data) != std::string::npos);          // the exact bytes
    PB_CHECK(raw.find("name=\"file\"") == raw.rfind("name=\"file\""));   // one part only
}

PB_TEST(an_upload_sends_the_credentials_to_the_server_it_was_addressed_to) {
    Env e;
    if (!e.pki.available()) PB_SKIP("openssl CLI not available");
    PB_CHECK(e.leaf.ok);
    e.dir.write("a.bin", "DATA");
    TlsTestServer server(e.leaf.cert, e.leaf.key, [](const std::string&) { return TlsTestServer::http_response(200, "{}"); });
    CurlTransport transport(e.pki.ca_cert());
    transport.perform(upload_request(server, e.dir.file("a.bin")));
    PB_CHECK(server.saw_authorization());
}

PB_TEST(a_certificate_failure_sends_neither_the_file_nor_the_credentials) {
    Env e;
    if (!e.pki.available()) PB_SKIP("openssl CLI not available");
    auto stranger = e.pki.self_signed("stranger", "DNS:localhost,IP:127.0.0.1");
    PB_CHECK(stranger.ok);
    e.dir.write("a.bin", "SECRET-FILE-CONTENT");
    TlsTestServer server(stranger.cert, stranger.key, [](const std::string&) { return TlsTestServer::http_response(200, "{}"); });
    CurlTransport transport(e.pki.ca_cert());
    auto resp = transport.perform(upload_request(server, e.dir.file("a.bin")));
    PB_CHECK(resp.error == ErrorClass::Tls);
    PB_CHECK(!server.saw_authorization());
    for (const auto& r : server.requests()) PB_CHECK(r.find("SECRET-FILE-CONTENT") == std::string::npos);
}

PB_TEST(a_redirect_to_another_host_does_not_forward_the_credentials) {
    Env e;
    if (!e.pki.available()) PB_SKIP("openssl CLI not available");
    PB_CHECK(e.leaf.ok);
    e.dir.write("a.bin", "DATA");
    TlsTestServer serverB(e.leaf.cert, e.leaf.key, [](const std::string&) { return TlsTestServer::http_response(200, "from-B"); });
    const std::string target = serverB.url("/target");
    TlsTestServer serverA(e.leaf.cert, e.leaf.key, [&](const std::string&) {
        return TlsTestServer::http_response(307, "", "Location: " + target + "\r\n");
    });
    CurlTransport transport(e.pki.ca_cert());
    transport.perform(upload_request(serverA, e.dir.file("a.bin")));
    PB_CHECK(serverA.saw_authorization());
    PB_CHECK(!serverB.saw_authorization());
}

PB_TEST(a_redirect_to_plain_http_is_refused_for_uploads) {
    Env e;
    if (!e.pki.available()) PB_SKIP("openssl CLI not available");
    PB_CHECK(e.leaf.ok);
    e.dir.write("a.bin", "DATA");
    TlsTestServer serverB(e.leaf.cert, e.leaf.key, [](const std::string&) { return TlsTestServer::http_response(200, "from-B"); });
    const std::string insecure = "http://localhost:" + std::to_string(serverB.port()) + "/target";
    TlsTestServer serverA(e.leaf.cert, e.leaf.key, [&](const std::string&) {
        return TlsTestServer::http_response(307, "", "Location: " + insecure + "\r\n");
    });
    CurlTransport transport(e.pki.ca_cert());
    auto resp = transport.perform(upload_request(serverA, e.dir.file("a.bin")));
    PB_CHECK(resp.error != ErrorClass::None);
    PB_CHECK(serverB.requests().empty());
}

PB_TEST(a_missing_upload_file_is_an_error_and_sends_nothing) {
    Env e;
    if (!e.pki.available()) PB_SKIP("openssl CLI not available");
    PB_CHECK(e.leaf.ok);
    TlsTestServer server(e.leaf.cert, e.leaf.key, [](const std::string&) { return TlsTestServer::http_response(200, "{}"); });
    CurlTransport transport(e.pki.ca_cert());
    auto resp = transport.perform(upload_request(server, e.dir.file("does-not-exist.bin")));
    PB_CHECK(resp.error != ErrorClass::None);
    PB_CHECK(server.requests().empty());
}

PB_TEST_MAIN()
