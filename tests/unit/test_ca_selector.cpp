// T013: select_ca_file picks the first usable device CA file, else the bundled one.
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "pbnotes/ca_selector.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::CaSource;
using pbnotes::select_ca_file;

namespace {
const char* kPem = "-----BEGIN CERTIFICATE-----\nMIIBfake\n-----END CERTIFICATE-----\n";
}

PB_TEST(first_usable_device_candidate_wins) {
    pbtest::TmpDir d;
    d.write("a.pem", kPem);
    d.write("b.pem", kPem);
    auto sel = select_ca_file({d.file("a.pem"), d.file("b.pem")}, "/bundled.pem");
    PB_CHECK_EQ(sel.path, d.file("a.pem"));
    PB_CHECK(sel.source == CaSource::Device);
}

PB_TEST(empty_and_non_pem_candidates_are_skipped) {
    pbtest::TmpDir d;
    d.write("empty.pem", "");
    d.write("text.txt", "this is not a certificate\n");
    d.write("good.pem", std::string("# header\n") + kPem);
    auto sel = select_ca_file({d.file("empty.pem"), d.file("text.txt"), d.file("good.pem")}, "/bundled.pem");
    PB_CHECK_EQ(sel.path, d.file("good.pem"));
    PB_CHECK(sel.source == CaSource::Device);
}

PB_TEST(missing_and_directory_candidates_are_skipped) {
    pbtest::TmpDir d;
    d.write("good.pem", kPem);
    auto sel = select_ca_file({d.file("does-not-exist.pem"), d.path(), d.file("good.pem")}, "/bundled.pem");
    PB_CHECK_EQ(sel.path, d.file("good.pem"));
}

PB_TEST(unreadable_candidate_is_skipped) {
    if (geteuid() == 0) PB_SKIP("root can read everything");
    pbtest::TmpDir d;
    d.write("secret.pem", kPem);
    ::chmod(d.file("secret.pem").c_str(), 0000);
    d.write("good.pem", kPem);
    auto sel = select_ca_file({d.file("secret.pem"), d.file("good.pem")}, "/bundled.pem");
    PB_CHECK_EQ(sel.path, d.file("good.pem"));
}

PB_TEST(falls_back_to_bundled_and_never_returns_no_ca) {
    pbtest::TmpDir d;
    d.write("empty.pem", "");
    auto sel = select_ca_file({d.file("empty.pem"), d.file("none.pem")}, "/opt/app/bundled.pem");
    PB_CHECK_EQ(sel.path, std::string("/opt/app/bundled.pem"));
    PB_CHECK(sel.source == CaSource::Bundled);
    auto none = select_ca_file({}, "/opt/app/bundled.pem");
    PB_CHECK_EQ(none.path, std::string("/opt/app/bundled.pem"));
    PB_CHECK(none.source == CaSource::Bundled);
}

PB_TEST(usable_ca_file_check) {
    pbtest::TmpDir d;
    d.write("ok.pem", kPem);
    d.write("bad.pem", "nothing");
    PB_CHECK(pbnotes::is_usable_ca_file(d.file("ok.pem")));
    PB_CHECK(!pbnotes::is_usable_ca_file(d.file("bad.pem")));
    PB_CHECK(!pbnotes::is_usable_ca_file(d.file("missing.pem")));
}

PB_TEST_MAIN()
