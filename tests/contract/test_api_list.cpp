// T044: list_notes / get_note against FakeTransport and fixtures.
#include <fstream>
#include <iterator>
#include <string>

#include "fake_transport.h"
#include "pbnotes/api_client.h"
#include "test_support.h"

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
const char* kNotes = "https://cloud.example.com/index.php/apps/notes/api/v1/notes";
}  // namespace

PB_TEST(first_listing_has_no_prune_parameter_and_no_validator) {
    FakeTransport t;
    t.expect("GET", "/notes", FakeTransport::ok(fixture("notes_list_full.unverified.json")));
    ApiClient api(t, creds());
    auto r = api.list_notes(std::nullopt, 0);
    PB_CHECK(r.error == ErrorClass::None);
    const auto& req = t.requests().at(0);
    PB_CHECK_EQ(req.method, std::string("GET"));
    PB_CHECK_EQ(req.url, std::string(kNotes));
    PB_CHECK(!pbnotes::has_header(req.headers, "If-None-Match"));
    PB_CHECK_EQ(req.username, std::string("alice"));
}

PB_TEST(the_server_title_is_kept_only_as_a_fallback_field) {
    FakeTransport t;
    t.expect("GET", "/notes", FakeTransport::ok(
        R"([{"id":9,"etag":"e9","readonly":false,"title":"File name","content":"","category":"","favorite":false,"modified":5}])"));
    ApiClient api(t, creds());
    auto r = api.list_notes(std::nullopt, 0);
    PB_CHECK_EQ(r.notes.size(), static_cast<size_t>(1));
    PB_CHECK_EQ(r.notes[0].server_title, std::string("File name"));
}

PB_TEST(listing_parses_every_field) {
    FakeTransport t;
    t.expect("GET", "/notes", FakeTransport::ok(fixture("notes_list_full.unverified.json")));
    ApiClient api(t, creds());
    auto r = api.list_notes(std::nullopt, 0);
    PB_CHECK_EQ(r.notes.size(), static_cast<size_t>(3));
    const auto& n = r.notes[0];
    PB_CHECK_EQ(n.id, static_cast<int64_t>(1));
    PB_CHECK_EQ(n.etag, std::string("e1000001"));
    PB_CHECK_EQ(n.content, std::string("Shopping list\n- [ ] milk\n- [x] bread\n"));
    PB_CHECK_EQ(n.category, std::string("Home"));
    PB_CHECK(n.favorite);
    PB_CHECK_EQ(n.modified, static_cast<int64_t>(1700000100));
    PB_CHECK(!n.pruned);
    PB_CHECK_EQ(r.notes[2].category, std::string(""));
}

PB_TEST(incremental_listing_sends_prune_before_and_if_none_match) {
    FakeTransport t;
    t.expect("GET", "/notes", FakeTransport::ok(fixture("notes_list_pruned.unverified.json")));
    ApiClient api(t, creds());
    api.list_notes(std::string("abc123"), 1700000000);
    const auto& req = t.requests().at(0);
    PB_CHECK_EQ(req.url, std::string(kNotes) + "?pruneBefore=1700000000");
    PB_CHECK_EQ(pbnotes::header_value(req.headers, "If-None-Match"), std::string("\"abc123\""));  // quotes added
    // an already quoted validator is sent unchanged
    FakeTransport t2;
    t2.expect("GET", "/notes", FakeTransport::ok("[]"));
    ApiClient api2(t2, creds());
    api2.list_notes(std::string("\"q\""), 5);
    PB_CHECK_EQ(pbnotes::header_value(t2.requests().at(0).headers, "If-None-Match"), std::string("\"q\""));
}

PB_TEST(pruned_entries_carry_only_an_id) {
    FakeTransport t;
    t.expect("GET", "/notes", FakeTransport::ok(fixture("notes_list_pruned.unverified.json")));
    ApiClient api(t, creds());
    auto r = api.list_notes(std::string("x"), 1);
    PB_CHECK_EQ(r.notes.size(), static_cast<size_t>(3));
    PB_CHECK(r.notes[0].pruned);
    PB_CHECK_EQ(r.notes[0].id, static_cast<int64_t>(2));
    PB_CHECK(!r.notes[1].pruned);
    PB_CHECK_EQ(r.notes[1].etag, std::string("e3000002"));
}

PB_TEST(listing_reads_the_response_headers) {
    FakeTransport t;
    t.expect("GET", "/notes",
             FakeTransport::ok("[]", {{"ETag", "\"coll-1\""},
                                      {"Last-Modified", "Tue, 14 Nov 2023 22:13:20 GMT"},
                                      {"X-Notes-API-Versions", "1.0, 1.3"}}));
    ApiClient api(t, creds());
    auto r = api.list_notes(std::nullopt, 0);
    PB_CHECK_EQ(r.etag.value_or(""), std::string("\"coll-1\""));
    PB_CHECK_EQ(r.last_modified, static_cast<int64_t>(1700000000));
    PB_CHECK_EQ(r.api_versions, std::string("1.0, 1.3"));
    PB_CHECK(!r.not_modified);
}

PB_TEST(missing_or_unparsable_headers_give_neutral_values) {
    FakeTransport t;
    t.expect("GET", "/notes", FakeTransport::ok("[]", {{"Last-Modified", "garbage"}}));
    ApiClient api(t, creds());
    auto r = api.list_notes(std::nullopt, 0);
    PB_CHECK(!r.etag.has_value());
    PB_CHECK_EQ(r.last_modified, static_cast<int64_t>(0));
    PB_CHECK_EQ(r.api_versions, std::string(""));
}

PB_TEST(not_modified_means_nothing_changed) {
    FakeTransport t;
    t.expect("GET", "/notes", FakeTransport::status(304));
    ApiClient api(t, creds());
    auto r = api.list_notes(std::string("abc"), 1);
    PB_CHECK(r.error == ErrorClass::None);
    PB_CHECK(r.not_modified);
    PB_CHECK(r.notes.empty());
}

PB_TEST(error_statuses_are_classified) {
    struct Case {
        long status;
        ErrorClass cls;
    };
    for (Case c : {Case{401, ErrorClass::Auth}, Case{403, ErrorClass::Auth}, Case{503, ErrorClass::Maintenance},
                   Case{500, ErrorClass::Server}, Case{404, ErrorClass::Server}}) {
        FakeTransport t;
        t.expect("GET", "/notes", FakeTransport::status(c.status));
        ApiClient api(t, creds());
        PB_CHECK(api.list_notes(std::nullopt, 0).error == c.cls);
    }
    FakeTransport t;
    pbnotes::Response net;
    net.error = ErrorClass::Network;
    t.expect("GET", "/notes", net);
    ApiClient api(t, creds());
    PB_CHECK(api.list_notes(std::nullopt, 0).error == ErrorClass::Network);
}

PB_TEST(invalid_json_or_wrong_types_fail_the_whole_listing) {
    for (const char* body : {"not json", "{\"id\":1}", "[1,2]",
                             R"([{"id":1,"etag":"e","title":"t","content":5,"category":"","favorite":false,"modified":1}])",
                             R"([{"id":"1","etag":"e","title":"t","content":"c","category":"","favorite":false,"modified":1}])",
                             R"([{"id":1,"title":"t","content":"c","category":"","favorite":false,"modified":1}])"}) {
        FakeTransport t;
        t.expect("GET", "/notes", FakeTransport::ok(body));
        ApiClient api(t, creds());
        auto r = api.list_notes(std::nullopt, 0);
        if (r.error != ErrorClass::Server) PB_FAIL_AT(std::string("accepted: ") + body);
        PB_CHECK(r.notes.empty());  // never a partial result
    }
}

PB_TEST(get_note_returns_one_note) {
    FakeTransport t;
    t.expect("GET", "/notes/4", FakeTransport::ok(fixture("note_single.unverified.json")));
    ApiClient api(t, creds());
    auto r = api.get_note(4);
    PB_CHECK(r.error == ErrorClass::None);
    PB_CHECK(r.note.has_value());
    PB_CHECK_EQ(r.note->id, static_cast<int64_t>(4));
    PB_CHECK_EQ(r.note->content, std::string("New on server\nCreated elsewhere.\n"));  // the server title is not kept
    PB_CHECK_EQ(t.requests().at(0).url, std::string(kNotes) + "/4");
}

PB_TEST(get_note_handles_conditional_gone_and_errors) {
    {
        FakeTransport t;
        t.expect("GET", "/notes/7", FakeTransport::status(304));
        ApiClient api(t, creds());
        auto r = api.get_note(7, std::string("tag"));
        PB_CHECK(r.not_modified && !r.not_found && r.error == ErrorClass::None);
        PB_CHECK_EQ(pbnotes::header_value(t.requests().at(0).headers, "If-None-Match"), std::string("\"tag\""));
    }
    {
        FakeTransport t;
        t.expect("GET", "/notes/7", FakeTransport::status(404));
        ApiClient api(t, creds());
        auto r = api.get_note(7);
        PB_CHECK(r.not_found && r.error == ErrorClass::None && !r.note.has_value());
    }
    {
        FakeTransport t;
        t.expect("GET", "/notes/7", FakeTransport::status(401));
        ApiClient api(t, creds());
        PB_CHECK(api.get_note(7).error == ErrorClass::Auth);
    }
    {
        FakeTransport t;
        t.expect("GET", "/notes/7", FakeTransport::ok("{\"id\":7}"));  // not a complete note
        ApiClient api(t, creds());
        PB_CHECK(api.get_note(7).error == ErrorClass::Server);
    }
}

PB_TEST_MAIN()
