// About box (spec 003): contract test of the manifest format against the REAL licenses/ tree of the
// repository (constitution IV). Also proves that every text the package will carry exists.
#include <sys/stat.h>

#include <fstream>
#include <string>

#include "pbnotes/about.h"
#include "test_support.h"

using namespace pbnotes::about;

namespace {

const std::string kRoot = PBNOTES_SOURCE_DIR;

long file_size(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 ? static_cast<long>(st.st_size) : -1;
}

}  // namespace

PB_TEST(real_manifest_parses_without_bad_blocks) {
    Manifest m = load_manifest(kRoot);
    PB_CHECK(m.readable);
    PB_CHECK(m.app.has_value());
    PB_CHECK_EQ(m.bad_blocks, 0);
    PB_CHECK_EQ(m.components.size(), size_t(10));
}

PB_TEST(app_block_names_the_author_and_the_license_file) {
    Manifest m = load_manifest(kRoot);
    PB_CHECK(m.app.has_value());
    PB_CHECK(!m.app->name.empty());
    PB_CHECK(!m.app->holder.empty());
    PB_CHECK(!m.app->copyright.empty());
    PB_CHECK(!m.app->license.empty());
    PB_CHECK_EQ(m.app->text.size(), size_t(1));
}

PB_TEST(every_named_text_exists_and_is_within_the_size_cap) {
    Manifest m = load_manifest(kRoot);
    std::vector<const Entry*> all;
    if (m.app) all.push_back(&*m.app);
    for (const Entry& e : m.components) all.push_back(&e);
    for (const Entry* e : all) {
        for (const std::string& t : e->text) {
            long size = file_size(kRoot + "/" + t);
            if (size <= 0) PB_FAIL_AT("text missing or empty: " + t);
            if (size > 256 * 1024) PB_FAIL_AT("text over 256 KiB: " + t);
        }
    }
}

PB_TEST(every_component_has_an_origin_a_purpose_and_an_address) {
    Manifest m = load_manifest(kRoot);
    int bundled = 0, system = 0;
    for (const Entry& e : m.components) {
        PB_CHECK(!e.purpose.empty());
        PB_CHECK(e.url.compare(0, 8, "https://") == 0);
        (e.origin == Origin::Bundled ? bundled : system)++;
    }
    PB_CHECK_EQ(bundled, 4);
    PB_CHECK_EQ(system, 6);
}

PB_TEST_MAIN()
