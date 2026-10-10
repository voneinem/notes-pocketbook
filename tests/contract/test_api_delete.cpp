// T102: the DELETE sequence required by constitution Principle VI — a conditional GET with the
// stored ETag first, DELETE only if the note is unchanged; the If-Match variant (used only if the
// server is verified to return 412 for a stale If-Match on DELETE) sends the header and no GET.
#include <fstream>
#include <iterator>
#include <string>

#include "fake_transport.h"
#include "pbnotes/api_client.h"
#include "test_support.h"

using pbnotes::ApiClient;
using pbnotes::DeleteOutcome;
using pbnotes::ErrorClass;
using pbtest::FakeTransport;

namespace {
std::string fixture(const std::string& name) {
    std::ifstream f(std::string(PBNOTES_FIXTURE_DIR) + "/" + name, std::ios::binary);
    if (!f) PB_FAIL_AT("missing fixture " + name);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
ApiClient::Credentials creds() { return {"https://cloud.example.com", "alice", "app-password"}; }
}  // namespace

PB_TEST(unchanged_note_is_checked_first_and_then_deleted) {
    FakeTransport t;
    t.expect("GET", "/notes/5", FakeTransport::status(304));
    t.expect("DELETE", "/notes/5", FakeTransport::ok("{}"));
    ApiClient api(t, creds());
    auto r = api.delete_note(5, "stored");
    PB_CHECK(r.error == ErrorClass::None);
    PB_CHECK(r.outcome == DeleteOutcome::Deleted);
    PB_CHECK_EQ(t.requests().size(), static_cast<size_t>(2));
    PB_CHECK_EQ(t.requests()[0].method, std::string("GET"));
    PB_CHECK_EQ(pbnotes::header_value(t.requests()[0].headers, "If-None-Match"), std::string("\"stored\""));
    PB_CHECK_EQ(t.requests()[1].method, std::string("DELETE"));
    PB_CHECK(t.mismatches().empty());
}

PB_TEST(a_note_changed_on_the_server_is_not_deleted) {
    FakeTransport t;
    t.expect("GET", "/notes/5", FakeTransport::ok(fixture("note_updated.unverified.json")));
    ApiClient api(t, creds());
    auto r = api.delete_note(5, "stored");
    PB_CHECK(r.outcome == DeleteOutcome::ChangedOnServer);
    PB_CHECK(r.server_note.has_value());
    PB_CHECK_EQ(r.server_note->etag, std::string("u2000002"));
    PB_CHECK_EQ(t.requests().size(), static_cast<size_t>(1));  // NO DELETE was sent
}

PB_TEST(a_note_that_is_already_gone_is_not_deleted_again) {
    FakeTransport t;
    t.expect("GET", "/notes/5", FakeTransport::status(404));
    ApiClient api(t, creds());
    auto r = api.delete_note(5, "stored");
    PB_CHECK(r.error == ErrorClass::None);
    PB_CHECK(r.outcome == DeleteOutcome::AlreadyGone);
    PB_CHECK_EQ(t.requests().size(), static_cast<size_t>(1));
}

PB_TEST(delete_404_after_the_check_counts_as_gone) {
    FakeTransport t;
    t.expect("GET", "/notes/5", FakeTransport::status(304));
    t.expect("DELETE", "/notes/5", FakeTransport::status(404));
    ApiClient api(t, creds());
    PB_CHECK(api.delete_note(5, "stored").outcome == DeleteOutcome::AlreadyGone);
}

PB_TEST(errors_stop_the_sequence_before_any_delete) {
    {
        FakeTransport t;
        t.expect("GET", "/notes/5", FakeTransport::status(401));
        ApiClient api(t, creds());
        auto r = api.delete_note(5, "stored");
        PB_CHECK(r.error == ErrorClass::Auth);
        PB_CHECK_EQ(t.requests().size(), static_cast<size_t>(1));
    }
    {
        FakeTransport t;
        pbnotes::Response net;
        net.error = ErrorClass::Network;
        t.expect("GET", "/notes/5", net);
        ApiClient api(t, creds());
        PB_CHECK(api.delete_note(5, "stored").error == ErrorClass::Network);
        PB_CHECK_EQ(t.requests().size(), static_cast<size_t>(1));
    }
    {
        FakeTransport t;
        t.expect("GET", "/notes/5", FakeTransport::status(304));
        t.expect("DELETE", "/notes/5", FakeTransport::status(503));
        ApiClient api(t, creds());
        auto r = api.delete_note(5, "stored");
        PB_CHECK(r.error == ErrorClass::Maintenance);
        PB_CHECK(r.outcome == DeleteOutcome::Failed);
    }
}

PB_TEST(if_match_variant_sends_the_header_and_no_get) {
    FakeTransport t;
    t.expect("DELETE", "/notes/5", FakeTransport::ok("{}"));
    ApiClient api(t, creds());
    api.set_delete_uses_if_match(true);
    auto r = api.delete_note(5, "stored");
    PB_CHECK(r.outcome == DeleteOutcome::Deleted);
    PB_CHECK_EQ(t.requests().size(), static_cast<size_t>(1));
    PB_CHECK_EQ(t.requests()[0].method, std::string("DELETE"));
    PB_CHECK_EQ(pbnotes::header_value(t.requests()[0].headers, "If-Match"), std::string("\"stored\""));
}

PB_TEST(if_match_variant_treats_412_as_a_conflict_and_404_as_gone) {
    {
        FakeTransport t;
        t.expect("DELETE", "/notes/5", FakeTransport::status(412));
        ApiClient api(t, creds());
        api.set_delete_uses_if_match(true);
        auto r = api.delete_note(5, "stored");
        PB_CHECK(r.outcome == DeleteOutcome::ChangedOnServer);
        PB_CHECK(!r.server_note.has_value());  // not fetched in this variant
        PB_CHECK(r.error == ErrorClass::None);
    }
    {
        FakeTransport t;
        t.expect("DELETE", "/notes/5", FakeTransport::status(404));
        ApiClient api(t, creds());
        api.set_delete_uses_if_match(true);
        PB_CHECK(api.delete_note(5, "stored").outcome == DeleteOutcome::AlreadyGone);
    }
}

PB_TEST_MAIN()
