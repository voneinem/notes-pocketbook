// T067: create_note (POST) and update_note (PUT with If-Match) against FakeTransport.
// The DELETE sequence is covered by test_api_delete.cpp (T102).
#include <fstream>
#include <iterator>
#include <string>

#include "fake_transport.h"
#include "pbnotes/api_client.h"
#include "pbnotes/json.h"
#include "test_support.h"

using pbnotes::ApiClient;
using pbnotes::ErrorClass;
using pbnotes::NoteDraft;
using pbtest::FakeTransport;

namespace {
std::string fixture(const std::string& name) {
    std::ifstream f(std::string(PBNOTES_FIXTURE_DIR) + "/" + name, std::ios::binary);
    if (!f) PB_FAIL_AT("missing fixture " + name);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
ApiClient::Credentials creds() { return {"https://cloud.example.com", "alice", "app-password"}; }
NoteDraft draft() {
    NoteDraft d;
    d.title = "Created offline";
    d.content = "Created offline\nwith \"quotes\" and ümläuts\n";
    d.category = "Work";
    d.favorite = true;
    d.modified = 1700001000;
    return d;
}
}  // namespace

PB_TEST(create_posts_title_content_category_favorite_and_modified) {
    FakeTransport t;
    t.expect("POST", "/notes", FakeTransport::ok(fixture("note_created.unverified.json")));
    ApiClient api(t, creds());
    auto r = api.create_note(draft());
    PB_CHECK(r.error == ErrorClass::None);
    const auto& req = t.requests().at(0);
    PB_CHECK_EQ(req.method, std::string("POST"));
    PB_CHECK_EQ(req.url, std::string("https://cloud.example.com/index.php/apps/notes/api/v1/notes"));
    PB_CHECK_EQ(pbnotes::header_value(req.headers, "Content-Type"), std::string("application/json"));
    auto body = pbnotes::json::parse(req.body);
    PB_CHECK(body.has_value());
    PB_CHECK_EQ(body->object.size(), static_cast<size_t>(5));  // exactly these five members (spec 004: the title is sent)
    PB_CHECK_EQ(body->get_string("title").value_or(""), std::string("Created offline"));
    PB_CHECK_EQ(body->get_string("content").value_or(""), draft().content);
    PB_CHECK_EQ(body->get_string("category").value_or(""), std::string("Work"));
    PB_CHECK(body->get_bool("favorite").value_or(false));
    PB_CHECK_EQ(body->get_int("modified").value_or(0), static_cast<int64_t>(1700001000));
    PB_CHECK(!pbnotes::has_header(req.headers, "If-Match"));
}

PB_TEST(create_response_gives_the_server_identity_and_validator) {
    FakeTransport t;
    t.expect("POST", "/notes", FakeTransport::ok(fixture("note_created.unverified.json")));
    ApiClient api(t, creds());
    auto r = api.create_note(draft());
    PB_CHECK(r.note.has_value());
    PB_CHECK_EQ(r.note->id, static_cast<int64_t>(101));
    PB_CHECK_EQ(r.note->etag, std::string("c1000001"));
    PB_CHECK_EQ(r.note->modified, static_cast<int64_t>(1700001000));
}

PB_TEST(update_sends_the_stored_etag_in_if_match) {
    FakeTransport t;
    t.expect("PUT", "/notes/5", FakeTransport::ok(fixture("note_updated.unverified.json")));
    ApiClient api(t, creds());
    auto r = api.update_note(5, draft(), "stored-etag");
    PB_CHECK(r.error == ErrorClass::None);
    const auto& req = t.requests().at(0);
    PB_CHECK_EQ(req.method, std::string("PUT"));
    PB_CHECK_EQ(req.url, std::string("https://cloud.example.com/index.php/apps/notes/api/v1/notes/5"));
    PB_CHECK_EQ(pbnotes::header_value(req.headers, "If-Match"), std::string("\"stored-etag\""));
    PB_CHECK_EQ(pbnotes::json::parse(req.body)->get_string("title").value_or("?"), std::string("Created offline"));
    PB_CHECK_EQ(r.note->etag, std::string("u2000002"));
}

PB_TEST(the_title_is_escaped_like_any_other_text) {
    FakeTransport t;
    t.expect("POST", "/notes", FakeTransport::ok(fixture("note_created.unverified.json")));
    t.expect("PUT", "/notes/5", FakeTransport::ok(fixture("note_updated.unverified.json")));
    ApiClient api(t, creds());
    NoteDraft d = draft();
    d.title = "Say \"hi\" \\ back/slash \xF0\x9F\x98\x80 \xC3\xBCber";
    api.create_note(d);
    api.update_note(5, d, "e");
    for (const auto& req : t.requests()) {
        auto body = pbnotes::json::parse(req.body);
        PB_CHECK(body.has_value());
        PB_CHECK_EQ(body->get_string("title").value_or("?"), d.title);
    }
}

PB_TEST(an_update_that_changes_only_category_or_favorite_still_sends_the_title) {
    FakeTransport t;
    t.expect("PUT", "/notes/5", FakeTransport::ok(fixture("note_updated.unverified.json")));
    ApiClient api(t, creds());
    NoteDraft d = draft();
    d.category = "Elsewhere";
    d.favorite = false;
    api.update_note(5, d, "e");
    auto body = pbnotes::json::parse(t.requests().at(0).body);
    PB_CHECK_EQ(body->get_string("title").value_or("?"), std::string("Created offline"));
    PB_CHECK_EQ(body->object.size(), static_cast<size_t>(5));
}

PB_TEST(a_title_the_server_adjusted_comes_back_as_the_servers_title) {
    FakeTransport t;
    t.expect("POST", "/notes", FakeTransport::ok(fixture("note_created_adjusted_title.unverified.json")));
    ApiClient api(t, creds());
    auto r = api.create_note(draft());
    PB_CHECK(r.note.has_value());
    PB_CHECK_EQ(r.note->server_title, std::string("Created offline (2)"));
    PB_CHECK_EQ(r.note->id, static_cast<int64_t>(102));
}

PB_TEST(update_flags_precondition_failed_and_gone) {
    {
        FakeTransport t;
        t.expect("PUT", "/notes/5", FakeTransport::status(412));
        ApiClient api(t, creds());
        auto r = api.update_note(5, draft(), "old");
        PB_CHECK(r.precondition_failed && !r.not_found);
        PB_CHECK(r.error == ErrorClass::None);  // a conflict is not a transport/server failure
        PB_CHECK(!r.note.has_value());
    }
    {
        FakeTransport t;
        t.expect("PUT", "/notes/5", FakeTransport::status(404));
        ApiClient api(t, creds());
        auto r = api.update_note(5, draft(), "old");
        PB_CHECK(r.not_found && !r.precondition_failed);
        PB_CHECK(r.error == ErrorClass::None);
    }
}

PB_TEST(write_errors_are_classified) {
    struct Case {
        long status;
        ErrorClass cls;
    };
    for (Case c : {Case{401, ErrorClass::Auth}, Case{503, ErrorClass::Maintenance}, Case{500, ErrorClass::Server},
                   Case{422, ErrorClass::Server}}) {
        FakeTransport t;
        t.expect("POST", "/notes", FakeTransport::status(c.status));
        ApiClient api(t, creds());
        PB_CHECK(api.create_note(draft()).error == c.cls);
    }
    FakeTransport t;
    pbnotes::Response net;
    net.error = ErrorClass::Network;
    t.expect("PUT", "/notes/5", net);
    ApiClient api(t, creds());
    PB_CHECK(api.update_note(5, draft(), "e").error == ErrorClass::Network);
}

PB_TEST(an_unparsable_write_response_is_a_server_error) {
    FakeTransport t;
    t.expect("POST", "/notes", FakeTransport::ok("<html>"));
    ApiClient api(t, creds());
    auto r = api.create_note(draft());
    PB_CHECK(r.error == ErrorClass::Server);
    PB_CHECK(!r.note.has_value());
}

PB_TEST_MAIN()
