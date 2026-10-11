// US4 (spec 002 FR-019..FR-024, contracts/notes-attachments-api.md): the client side of the attachment
// download and size probe, against the scripted transport.
#include <fstream>
#include <iterator>
#include <string>

#include "fake_transport.h"
#include "pbnotes/api_client.h"
#include "pbnotes/json.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::ApiClient;
using pbnotes::ErrorClass;
using pbtest::FakeTransport;

namespace {

std::string fixture(const std::string& name) {
    std::ifstream f(std::string(PBNOTES_FIXTURE_DIR) + "/" + name, std::ios::binary);
    if (!f) PB_FAIL_AT("missing fixture " + name);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
ApiClient::Credentials creds() { return {"https://cloud.example.com", "alice", "app-password"}; }
const char* kBase = "https://cloud.example.com/index.php/apps/notes/api/v1.4/attachment/12?path=";
constexpr int64_t kLimit = 10 * 1024 * 1024;

pbnotes::Headers head_headers() {
    auto doc = pbnotes::json::parse(fixture("attachment_head_ok.unverified.json"));
    pbnotes::Headers h;
    for (const auto& kv : doc->find("headers")->object) h.emplace_back(kv.first, kv.second.string);
    return h;
}

}  // namespace

PB_TEST(supports_attachments_needs_api_version_one_point_four) {
    PB_CHECK(pbnotes::supports_attachments("1.0, 1.3, 1.4"));
    PB_CHECK(pbnotes::supports_attachments("1.4"));
    PB_CHECK(pbnotes::supports_attachments("0.2,1.0,1.4"));
    PB_CHECK(!pbnotes::supports_attachments("1.0, 1.3"));
    PB_CHECK(!pbnotes::supports_attachments(""));
    PB_CHECK(!pbnotes::supports_attachments("1.40, 11.4, 1.4.1"));   // whole tokens only
}

PB_TEST(a_download_is_a_get_with_the_encoded_path_and_credentials_and_no_if_match) {
    pbtest::TmpDir d;
    FakeTransport t;
    t.expect("GET", "attachment/12", FakeTransport::ok("PNGDATA"));
    ApiClient api(t, creds());
    auto r = api.download_attachment(12, ".attachments.12/photo one.png", d.file("a.part"), kLimit);
    PB_CHECK(r.error == ErrorClass::None);
    PB_CHECK_EQ(t.requests().size(), static_cast<size_t>(1));
    const auto& req = t.requests()[0];
    PB_CHECK_EQ(req.method, std::string("GET"));
    PB_CHECK_EQ(req.url, std::string(kBase) + ".attachments.12%2Fphoto%20one.png");
    PB_CHECK_EQ(req.username, std::string("alice"));
    PB_CHECK_EQ(req.password, std::string("app-password"));
    PB_CHECK(!pbnotes::has_header(req.headers, "If-Match"));
    PB_CHECK(!pbnotes::has_header(req.headers, "If-None-Match"));
    PB_CHECK_EQ(req.save_body_to, d.file("a.part"));
    PB_CHECK_EQ(req.max_body_bytes, kLimit);
    PB_CHECK(t.mismatches().empty());
}

PB_TEST(special_characters_in_the_path_are_percent_encoded_in_the_query) {
    pbtest::TmpDir d;
    FakeTransport t;
    t.expect("GET", "attachment/12", FakeTransport::ok("x"));
    ApiClient api(t, creds());
    api.download_attachment(12, "a&b#c+d=e.png", d.file("a.part"), kLimit);
    PB_CHECK_EQ(t.requests()[0].url, std::string(kBase) + "a%26b%23c%2Bd%3De.png");
}

PB_TEST(a_successful_download_stores_the_body_at_the_part_path_and_reports_its_size) {
    pbtest::TmpDir d;
    FakeTransport t;
    t.expect("GET", "attachment/12", FakeTransport::ok("PNGDATA!"));
    ApiClient api(t, creds());
    auto r = api.download_attachment(12, "a.png", d.file("a.part"), kLimit);
    PB_CHECK(r.error == ErrorClass::None);
    PB_CHECK_EQ(r.status, 200L);
    PB_CHECK(!r.not_found && !r.too_large && !r.unsupported && !r.rejected);
    PB_CHECK_EQ(r.size.value_or(-1), static_cast<int64_t>(8));
    PB_CHECK_EQ(d.read("a.part"), std::string("PNGDATA!"));
}

PB_TEST(a_missing_file_is_not_found_and_leaves_no_file) {
    pbtest::TmpDir d;
    FakeTransport t;
    t.expect("GET", "attachment/12", FakeTransport::status(404, fixture("attachment_404.unverified.json")));
    ApiClient api(t, creds());
    auto r = api.download_attachment(12, "a.png", d.file("a.part"), kLimit);
    PB_CHECK(r.not_found);
    PB_CHECK(r.error == ErrorClass::None);   // an answer from the server, not a failure of the connection
    PB_CHECK(!d.exists("a.part"));
}

PB_TEST(an_old_server_answering_400_is_unsupported) {
    pbtest::TmpDir d;
    FakeTransport t;
    t.expect("GET", "attachment/12", FakeTransport::status(400, fixture("attachment_400_old_server.unverified.json")));
    ApiClient api(t, creds());
    auto r = api.download_attachment(12, "a.png", d.file("a.part"), kLimit);
    PB_CHECK(r.unsupported);
    PB_CHECK(!d.exists("a.part"));
}

PB_TEST(rejected_credentials_and_maintenance_are_classified_like_for_notes) {
    pbtest::TmpDir d;
    for (long code : {401L, 403L}) {
        FakeTransport t;
        t.expect("GET", "attachment/12", FakeTransport::status(code));
        ApiClient api(t, creds());
        PB_CHECK(api.download_attachment(12, "a.png", d.file("a.part"), kLimit).error == ErrorClass::Auth);
    }
    FakeTransport t;
    t.expect("GET", "attachment/12", FakeTransport::status(503));
    ApiClient api(t, creds());
    PB_CHECK(api.download_attachment(12, "a.png", d.file("a.part"), kLimit).error == ErrorClass::Maintenance);
}

PB_TEST(any_other_failure_status_is_rejected) {
    pbtest::TmpDir d;
    FakeTransport t;
    t.expect("GET", "attachment/12", FakeTransport::status(500));
    ApiClient api(t, creds());
    auto r = api.download_attachment(12, "a.png", d.file("a.part"), kLimit);
    PB_CHECK(r.rejected);
    PB_CHECK(r.error == ErrorClass::Server);
}

PB_TEST(a_transport_failure_is_passed_through) {
    pbtest::TmpDir d;
    FakeTransport t;
    t.fail_request(0, ErrorClass::Network);
    ApiClient api(t, creds());
    auto r = api.download_attachment(12, "a.png", d.file("a.part"), kLimit);
    PB_CHECK(r.error == ErrorClass::Network);
    PB_CHECK_EQ(r.status, 0L);
}

PB_TEST(a_content_length_over_the_limit_stops_before_the_body_and_stores_nothing) {
    pbtest::TmpDir d;
    FakeTransport t;
    t.expect("GET", "attachment/12", FakeTransport::ok("small", {{"Content-Length", std::to_string(kLimit + 1)}}));
    ApiClient api(t, creds());
    auto r = api.download_attachment(12, "a.png", d.file("a.part"), kLimit);
    PB_CHECK(r.too_large);
    PB_CHECK(r.error == ErrorClass::None);
    PB_CHECK(!d.exists("a.part"));
}

PB_TEST(a_body_cut_by_the_transport_at_the_limit_is_too_large_and_stores_nothing) {
    pbtest::TmpDir d;
    FakeTransport t;
    pbnotes::Response cut = FakeTransport::ok("");
    cut.body_too_large = true;
    t.expect("GET", "attachment/12", cut);
    ApiClient api(t, creds());
    auto r = api.download_attachment(12, "a.png", d.file("a.part"), kLimit);
    PB_CHECK(r.too_large);
    PB_CHECK(!d.exists("a.part"));
}

PB_TEST(a_download_exactly_at_the_limit_is_accepted) {
    pbtest::TmpDir d;
    FakeTransport t;
    t.expect("GET", "attachment/12", FakeTransport::ok(std::string(64, 'x'), {{"Content-Length", "64"}}));
    ApiClient api(t, creds());
    auto r = api.download_attachment(12, "a.png", d.file("a.part"), 64);
    PB_CHECK(!r.too_large);
    PB_CHECK_EQ(r.size.value_or(-1), static_cast<int64_t>(64));
}

PB_TEST(a_write_failure_on_the_device_is_storage_full) {
    pbtest::TmpDir d;
    FakeTransport t;
    pbnotes::Response full = FakeTransport::ok("");
    full.storage_full = true;
    t.expect("GET", "attachment/12", full);
    ApiClient api(t, creds());
    auto r = api.download_attachment(12, "a.png", d.file("a.part"), kLimit);
    PB_CHECK(r.storage_full);
    PB_CHECK(!d.exists("a.part"));
}

PB_TEST(a_probe_is_a_head_with_a_short_timeout_and_returns_the_length) {
    FakeTransport t;
    t.expect("HEAD", "attachment/12", FakeTransport::ok("", head_headers()));
    ApiClient api(t, creds());
    auto r = api.probe_attachment(12, ".attachments.12/photo one.png");
    PB_CHECK(r.error == ErrorClass::None);
    PB_CHECK_EQ(r.size.value_or(-1), static_cast<int64_t>(123456));
    const auto& req = t.requests()[0];
    PB_CHECK_EQ(req.method, std::string("HEAD"));
    PB_CHECK_EQ(req.url, std::string(kBase) + ".attachments.12%2Fphoto%20one.png");
    PB_CHECK_EQ(req.total_timeout_s, 3L);
    PB_CHECK(req.connect_timeout_s <= 3);
    PB_CHECK(!pbnotes::has_header(req.headers, "If-Match"));
}

PB_TEST(a_probe_without_a_length_gives_no_size_and_no_error) {
    FakeTransport t;
    t.expect("HEAD", "attachment/12", FakeTransport::ok(""));
    ApiClient api(t, creds());
    auto r = api.probe_attachment(12, "a.png");
    PB_CHECK(r.error == ErrorClass::None);
    PB_CHECK(!r.size.has_value());
    PB_CHECK(!r.not_found);
}

PB_TEST(a_probe_of_a_missing_file_says_not_found) {
    FakeTransport t;
    t.expect("HEAD", "attachment/12", FakeTransport::status(404));
    ApiClient api(t, creds());
    auto r = api.probe_attachment(12, "a.png");
    PB_CHECK(r.not_found);
}

PB_TEST(a_probe_failure_is_not_fatal_but_classified) {
    FakeTransport t;
    t.expect("HEAD", "attachment/12", FakeTransport::status(401));
    ApiClient api(t, creds());
    PB_CHECK(api.probe_attachment(12, "a.png").error == ErrorClass::Auth);
    FakeTransport t2;
    t2.fail_request(0, ErrorClass::Network);
    ApiClient api2(t2, creds());
    auto r = api2.probe_attachment(12, "a.png");
    PB_CHECK(r.error == ErrorClass::Network);
    PB_CHECK(!r.size.has_value());
}

PB_TEST(a_garbage_content_length_is_ignored) {
    FakeTransport t;
    t.expect("HEAD", "attachment/12", FakeTransport::ok("", {{"Content-Length", "12x"}}));
    ApiClient api(t, creds());
    PB_CHECK(!api.probe_attachment(12, "a.png").size.has_value());
    FakeTransport t2;
    t2.expect("HEAD", "attachment/12", FakeTransport::ok("", {{"Content-Length", "-5"}}));
    ApiClient api2(t2, creds());
    PB_CHECK(!api2.probe_attachment(12, "a.png").size.has_value());
}

PB_TEST_MAIN()
