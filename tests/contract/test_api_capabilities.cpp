// T024: ApiClient::fetch_capabilities against FakeTransport and fixtures.
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
}  // namespace

PB_TEST(capabilities_success_lists_notes_api_versions) {
    FakeTransport t;
    t.expect("GET", "/ocs/v2.php/cloud/capabilities", FakeTransport::ok(fixture("capabilities_ok.unverified.json")));
    ApiClient api(t, creds());
    auto r = api.fetch_capabilities();
    PB_CHECK(r.error == ErrorClass::None);
    PB_CHECK(r.capabilities.has_api_v1());
    PB_CHECK_EQ(r.capabilities.notes_api_versions.size(), static_cast<size_t>(5));
    PB_CHECK_EQ(r.capabilities.notes_api_versions.front(), std::string("0.2"));
}

PB_TEST(capabilities_request_is_authenticated_and_asks_for_json) {
    FakeTransport t;
    t.expect("GET", "capabilities", FakeTransport::ok(fixture("capabilities_ok.unverified.json")));
    ApiClient api(t, creds());
    api.fetch_capabilities();
    PB_CHECK_EQ(t.requests().size(), static_cast<size_t>(1));
    const auto& req = t.requests()[0];
    PB_CHECK_EQ(req.url, std::string("https://cloud.example.com/ocs/v2.php/cloud/capabilities?format=json"));
    PB_CHECK_EQ(req.username, std::string("alice"));
    PB_CHECK_EQ(req.password, std::string("app-password"));
    PB_CHECK_EQ(pbnotes::header_value(req.headers, "OCS-APIRequest"), std::string("true"));
    PB_CHECK_EQ(pbnotes::header_value(req.headers, "accept"), std::string("application/json"));
    // credentials are never part of the URL
    PB_CHECK(req.url.find("alice") == std::string::npos && req.url.find("app-password") == std::string::npos);
}

PB_TEST(auth_failures_are_classified) {
    for (long code : {401L, 403L}) {
        FakeTransport t;
        t.expect("GET", "capabilities", FakeTransport::status(code));
        ApiClient api(t, creds());
        auto r = api.fetch_capabilities();
        PB_CHECK(r.error == ErrorClass::Auth);
        PB_CHECK_EQ(r.status, code);
    }
}

PB_TEST(missing_notes_section_means_unsupported) {
    FakeTransport t;
    t.expect("GET", "capabilities", FakeTransport::ok(fixture("capabilities_no_notes.unverified.json")));
    ApiClient api(t, creds());
    auto r = api.fetch_capabilities();
    PB_CHECK(r.error == ErrorClass::Unsupported);
}

PB_TEST(server_without_api_v1_means_unsupported) {
    FakeTransport t;
    t.expect("GET", "capabilities", FakeTransport::ok(fixture("capabilities_only_v02.unverified.json")));
    ApiClient api(t, creds());
    auto r = api.fetch_capabilities();
    PB_CHECK(r.error == ErrorClass::Unsupported);
    PB_CHECK(!r.capabilities.has_api_v1());
}

PB_TEST(maintenance_mode_is_classified) {
    FakeTransport t;
    t.expect("GET", "capabilities", FakeTransport::status(503));
    ApiClient api(t, creds());
    PB_CHECK(api.fetch_capabilities().error == ErrorClass::Maintenance);
}

PB_TEST(transport_errors_are_passed_through) {
    for (ErrorClass e : {ErrorClass::Network, ErrorClass::Tls}) {
        FakeTransport t;
        pbnotes::Response r;
        r.error = e;
        t.expect("GET", "capabilities", r);
        ApiClient api(t, creds());
        PB_CHECK(api.fetch_capabilities().error == e);
    }
}

PB_TEST(invalid_json_is_a_server_error) {
    FakeTransport t;
    t.expect("GET", "capabilities", FakeTransport::ok("<html>not json</html>"));
    ApiClient api(t, creds());
    PB_CHECK(api.fetch_capabilities().error == ErrorClass::Server);
}

PB_TEST(wrong_json_types_are_rejected_without_partial_result) {
    FakeTransport t;
    t.expect("GET", "capabilities",
             FakeTransport::ok(R"({"ocs":{"data":{"capabilities":{"notes":{"api_version":"1.0"}}}}})"));
    ApiClient api(t, creds());
    auto r = api.fetch_capabilities();
    PB_CHECK(r.error == ErrorClass::Server);
    PB_CHECK(r.capabilities.notes_api_versions.empty());
}

PB_TEST_MAIN()
