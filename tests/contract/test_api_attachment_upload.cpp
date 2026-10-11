// US5 (spec 002 FR-025, contracts/notes-attachments-api.md): the client side of the attachment upload against
// the scripted transport.
#include <fstream>
#include <iterator>
#include <string>

#include "fake_transport.h"
#include "pbnotes/api_client.h"
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

struct Env {
    pbtest::TmpDir dir;
    std::string file;
    Env() {
        dir.write("photo.png", "PNGDATA");
        file = dir.file("photo.png");
    }
};

}  // namespace

PB_TEST(an_upload_is_a_multipart_post_with_the_single_field_file_and_nothing_else) {
    Env e;
    FakeTransport t;
    t.expect("POST", "attachment/12", FakeTransport::ok(fixture("attachment_upload_ok.unverified.json")));
    ApiClient api(t, creds());
    auto r = api.upload_attachment(12, e.file, "My Photo.png");
    PB_CHECK(r.error == ErrorClass::None);
    PB_CHECK_EQ(t.requests().size(), static_cast<size_t>(1));
    const auto& req = t.requests()[0];
    PB_CHECK_EQ(req.method, std::string("POST"));
    PB_CHECK_EQ(req.url, std::string("https://cloud.example.com/index.php/apps/notes/api/v1.4/attachment/12"));
    PB_CHECK_EQ(req.upload_file, e.file);
    PB_CHECK_EQ(req.upload_field, std::string("file"));
    PB_CHECK_EQ(req.upload_file_name, std::string("My Photo.png"));
    PB_CHECK(req.body.empty());
    PB_CHECK(req.save_body_to.empty());
    PB_CHECK_EQ(req.username, std::string("alice"));
    PB_CHECK(!pbnotes::has_header(req.headers, "If-Match"));
    PB_CHECK(t.mismatches().empty());
}

PB_TEST(success_returns_the_validated_remote_path) {
    Env e;
    FakeTransport t;
    t.expect("POST", "attachment/12", FakeTransport::ok(fixture("attachment_upload_ok.unverified.json")));
    ApiClient api(t, creds());
    auto r = api.upload_attachment(12, e.file, "photo.png");
    PB_CHECK_EQ(r.remote_path, std::string(".attachments.12/photo.png"));
    PB_CHECK(!r.too_large && !r.rejected && !r.unsupported && !r.not_found);
    PB_CHECK_EQ(r.status, 200L);
}

PB_TEST(a_name_chosen_by_the_server_is_accepted_as_returned) {
    Env e;
    FakeTransport t;
    t.expect("POST", "attachment/12", FakeTransport::ok(fixture("attachment_upload_renamed.unverified.json")));
    ApiClient api(t, creds());
    auto r = api.upload_attachment(12, e.file, "photo.png");
    PB_CHECK_EQ(r.remote_path, std::string(".attachments.12/photo (2).png"));
}

PB_TEST(an_invalid_answer_is_rejected) {
    Env e;
    const std::string too_long = "{\"filename\":\"" + std::string(1025, 'a') + "\"}";
    for (const std::string& body : {std::string("{\"filename\":\"/etc/passwd\"}"), std::string("{\"filename\":\"a\\u0001b.png\"}"),
                                    std::string("{\"filename\":\"dir/\"}"), std::string("{\"filename\":\"https://x.example/a.png\"}"),
                                    std::string("{\"filename\":\"\"}"), std::string("{\"name\":\"a.png\"}"),
                                    std::string("{\"filename\":5}"), std::string("not json"), std::string(""), too_long}) {
        FakeTransport t;
        t.expect("POST", "attachment/12", FakeTransport::ok(body));
        ApiClient api(t, creds());
        auto r = api.upload_attachment(12, e.file, "photo.png");
        PB_CHECK(r.rejected);
        PB_CHECK(r.remote_path.empty());
    }
}

PB_TEST(too_large_and_old_server_and_missing_note_have_their_own_flags) {
    Env e;
    {
        FakeTransport t;
        t.expect("POST", "attachment/12", FakeTransport::status(413, fixture("attachment_upload_413.unverified.json")));
        ApiClient api(t, creds());
        auto r = api.upload_attachment(12, e.file, "photo.png");
        PB_CHECK(r.too_large);
        PB_CHECK(r.error == ErrorClass::None);
    }
    {
        FakeTransport t;
        t.expect("POST", "attachment/12", FakeTransport::status(400, fixture("attachment_400_old_server.unverified.json")));
        ApiClient api(t, creds());
        PB_CHECK(api.upload_attachment(12, e.file, "photo.png").unsupported);
    }
    {
        FakeTransport t;
        t.expect("POST", "attachment/12", FakeTransport::status(404, fixture("attachment_404.unverified.json")));
        ApiClient api(t, creds());
        auto r = api.upload_attachment(12, e.file, "photo.png");
        PB_CHECK(r.not_found);   // the note is gone on the server
    }
    {
        FakeTransport t;
        t.expect("POST", "attachment/12", FakeTransport::status(500));
        ApiClient api(t, creds());
        auto r = api.upload_attachment(12, e.file, "photo.png");
        PB_CHECK(r.rejected);
    }
}

PB_TEST(credentials_and_maintenance_are_classified_like_for_notes) {
    Env e;
    for (long code : {401L, 403L}) {
        FakeTransport t;
        t.expect("POST", "attachment/12", FakeTransport::status(code));
        ApiClient api(t, creds());
        PB_CHECK(api.upload_attachment(12, e.file, "photo.png").error == ErrorClass::Auth);
    }
    FakeTransport t;
    t.expect("POST", "attachment/12", FakeTransport::status(503));
    ApiClient api(t, creds());
    PB_CHECK(api.upload_attachment(12, e.file, "photo.png").error == ErrorClass::Maintenance);
    FakeTransport t2;
    t2.fail_request(0, ErrorClass::Network);
    ApiClient api2(t2, creds());
    PB_CHECK(api2.upload_attachment(12, e.file, "photo.png").error == ErrorClass::Network);
}

PB_TEST(a_missing_or_oversized_file_makes_no_request) {
    Env e;
    FakeTransport t;
    ApiClient api(t, creds());
    auto missing = api.upload_attachment(12, e.dir.file("nope.png"), "nope.png");
    PB_CHECK(missing.unreadable);
    PB_CHECK(missing.error == ErrorClass::None);
    const std::string big = e.dir.file("big.bin");
    {
        std::ofstream f(big, std::ios::binary);
        f.seekp(pbnotes::kMaxAttachmentBytes);
        f.put('x');
    }
    auto too_big = api.upload_attachment(12, big, "big.bin");
    PB_CHECK(too_big.too_large);
    PB_CHECK_EQ(t.requests().size(), static_cast<size_t>(0));
    auto dir = api.upload_attachment(12, e.dir.path(), "dir");
    PB_CHECK(dir.unreadable);
    PB_CHECK_EQ(t.requests().size(), static_cast<size_t>(0));
}

PB_TEST_MAIN()
