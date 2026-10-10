// The defensive JSON reader used for all server responses: whole-document success or failure.
#include <string>

#include "pbnotes/json.h"
#include "test_support.h"

using pbnotes::json::parse;
using pbnotes::json::quote;

PB_TEST(parses_objects_arrays_and_scalars) {
    auto v = parse(R"({"a":1,"b":[true,false,null,"x"],"c":{"d":-2.5e1},"e":"text"})");
    PB_CHECK(v.has_value());
    PB_CHECK_EQ(v->get_int("a").value_or(-1), static_cast<int64_t>(1));
    PB_CHECK(v->find("b")->is_array());
    PB_CHECK_EQ(v->find("b")->array.size(), static_cast<size_t>(4));
    PB_CHECK(v->find("b")->array[2].is_null());
    PB_CHECK_EQ(v->find("c")->find("d")->number, -25.0);
    PB_CHECK_EQ(v->get_string("e").value_or(""), std::string("text"));
}

PB_TEST(typed_getters_reject_wrong_types) {
    auto v = parse(R"({"n":1.5,"s":"1","b":1})");
    PB_CHECK(v.has_value());
    PB_CHECK(!v->get_int("n").has_value());  // not an integer
    PB_CHECK(!v->get_int("s").has_value());  // a string
    PB_CHECK(!v->get_bool("b").has_value());
    PB_CHECK(!v->get_string("missing").has_value());
}

PB_TEST(decodes_escapes_and_unicode) {
    auto v = parse(R"({"s":"line\nTab\t\"q\" \u00e4 \u65e5 \ud83d\ude42"})");
    PB_CHECK(v.has_value());
    PB_CHECK_EQ(v->get_string("s").value_or(""), std::string("line\nTab\t\"q\" ä 日 🙂"));
}

PB_TEST(malformed_documents_fail_as_a_whole) {
    for (const char* bad : {"", "{", "[1,2", "{\"a\":}", "{\"a\" 1}", "{a:1}", "[1,]", "tru", "01", "1.", "\"abc",
                            "\"bad\\q\"", "\"\\ud800\"", "\"\\udc00x\"", "{\"a\":1} extra", "\"ctl\x01\""}) {
        std::string err;
        if (parse(bad, &err).has_value()) PB_FAIL_AT(std::string("accepted: ") + bad);
        PB_CHECK(!err.empty());
    }
}

PB_TEST(excessive_nesting_is_rejected) {
    std::string deep(200, '[');
    deep += std::string(200, ']');
    PB_CHECK(!parse(deep).has_value());
}

PB_TEST(quote_escapes_everything_needed) {
    PB_CHECK_EQ(quote("a\"b\\c\nd\te"), std::string("\"a\\\"b\\\\c\\nd\\te\""));
    PB_CHECK_EQ(quote(std::string("x\x01y")), std::string("\"x\\u0001y\""));
    PB_CHECK_EQ(quote("ä日"), std::string("\"ä日\""));
    auto round = parse(quote("tricky \"\\\n\t\x1f ünï"));
    PB_CHECK(round.has_value() && round->is_string());
    PB_CHECK_EQ(round->string, std::string("tricky \"\\\n\t\x1f ünï"));
}

PB_TEST_MAIN()
