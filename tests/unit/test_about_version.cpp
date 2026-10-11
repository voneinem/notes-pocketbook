// About box (spec 003 FR-003): which version string the About screen shows.
#include "pbnotes/about.h"
#include "test_support.h"

using pbnotes::about::display_version;

PB_TEST(empty_and_fallback_versions_show_as_unknown) {
    PB_CHECK_EQ(display_version(""), std::string("unknown"));
    PB_CHECK_EQ(display_version("0.0.0"), std::string("unknown"));
}

PB_TEST(real_versions_are_shown_unchanged) {
    PB_CHECK_EQ(display_version("0ae4a86"), std::string("0ae4a86"));
    PB_CHECK_EQ(display_version("0.2.0"), std::string("0.2.0"));
    PB_CHECK_EQ(display_version("0.2.0-3-gabc123-dirty"), std::string("0.2.0-3-gabc123-dirty"));
    PB_CHECK_EQ(display_version("0.0.1"), std::string("0.0.1"));
}

PB_TEST_MAIN()
