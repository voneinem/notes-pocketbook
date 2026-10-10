// Smoke test: proves the host build links notes_core and the test runner works.
#include <cstring>

#include "pbnotes/version.h"
#include "test_support.h"

extern "C" const char* sqlite3_libversion(void);

PB_TEST(version_is_not_empty) {
    PB_CHECK(std::strlen(pbnotes::version()) > 0);
}

PB_TEST(sqlite_is_linked) {
    // The vendored SQLite is linked into notes_core; the version macro comes from its header.
    PB_CHECK(std::strlen(sqlite3_libversion()) > 0);
}

PB_TEST_MAIN()
