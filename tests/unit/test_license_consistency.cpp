// About box (spec 003 FR-012, FR-013): the in-app list of third-party components cannot silently drift from NOTICE,
// third_party/, the CMake dependencies and the license files. One test per rule proves it fires on in-memory data;
// the last tests run all rules on the REAL repository.
#include <sys/stat.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

#include "license_check.h"
#include "pbnotes/about.h"
#include "test_support.h"

using namespace licensecheck;

namespace {

const std::string kDashes(80, '-');

const char* kManifest =
    "id: pbnotes\nkind: app\nname: Notes Pocketbook\nlicense: MIT License\nholder: Jane Author\n"
    "copyright: Copyright (c) 2026 Jane Author\ntext: LICENSE\n\n"
    "id: sqlite\nkind: component\nname: SQLite\npurpose: Stores notes\nlicense: Public domain\nholder: The authors\n"
    "url: https://www.sqlite.org/\norigin: bundled\nsource: third_party/sqlite\n\n"
    "id: qt\nkind: component\nname: Qt 6.8\npurpose: Draws\nlicense: LGPL-3.0\nholder: Qt\n"
    "url: https://www.qt.io/\norigin: system\ntext: licenses/LGPL-3.0.txt\ncmake: Qt6\n";

// A repository in which every rule holds.
Inputs good() {
    Inputs in;
    in.manifest = pbnotes::about::parse_manifest(kManifest);
    in.notice = "Intro text\n\n" + kDashes + "\nSQLite (public domain)\nbody\n\n" + kDashes + "\nQt 6.8 (LGPL-3.0)\nbody\n";
    in.third_party_dirs = {"sqlite"};
    in.cmake_names = {"Qt6"};
    in.size_of = [](const std::string& p) -> long {
        if (p == "LICENSE" || p == "licenses/LGPL-3.0.txt") return 1000;
        if (p == "third_party/sqlite") return 0;
        return -1;
    };
    in.license_file = "MIT License\n\nCopyright (c) 2026 Jane Author\n\nPermission is hereby granted...\n";
    return in;
}

bool has_finding(const std::vector<std::string>& findings, const std::string& rule, const std::string& what) {
    return std::any_of(findings.begin(), findings.end(), [&](const std::string& f) {
        return f.compare(0, rule.size(), rule) == 0 && f.find(what) != std::string::npos;
    });
}

std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

}  // namespace

PB_TEST(a_consistent_repository_has_no_findings) {
    PB_CHECK(check_consistency(good()).empty());
}

PB_TEST(rule_1_component_without_notice_section) {
    Inputs in = good();
    in.manifest.components[1].name = "Qt 6.9";  // renamed in the manifest only
    auto f = check_consistency(in);
    PB_CHECK(has_finding(f, "rule 1:", "Qt 6.9"));
    PB_CHECK(has_finding(f, "rule 1:", "Qt 6.8 (LGPL-3.0)"));  // the NOTICE section is now unclaimed too
}

PB_TEST(rule_1_notice_section_without_component) {
    Inputs in = good();
    in.notice += "\n" + kDashes + "\nZlib (zlib license)\nbody\n";
    PB_CHECK(has_finding(check_consistency(in), "rule 1:", "Zlib"));
}

PB_TEST(rule_1_the_introduction_before_the_first_rule_line_is_not_a_section) {
    Inputs in = good();
    PB_CHECK_EQ(notice_headings(in.notice).size(), size_t(2));
}

PB_TEST(rule_2_unclaimed_third_party_directory) {
    Inputs in = good();
    in.third_party_dirs.push_back("dummy");
    in.size_of = [](const std::string& p) -> long { return p == "third_party/sqlite" || p == "third_party/dummy" ? 0 : (p == "LICENSE" || p == "licenses/LGPL-3.0.txt" ? 10 : -1); };
    PB_CHECK(has_finding(check_consistency(in), "rule 2:", "third_party/dummy"));
}

PB_TEST(rule_3_unclaimed_cmake_dependency) {
    Inputs in = good();
    in.cmake_names.push_back("ZLIB");
    PB_CHECK(has_finding(check_consistency(in), "rule 3:", "ZLIB"));
}

PB_TEST(rule_3_scraping_reads_find_package_and_linked_libraries) {
    const std::string cmake =
        "find_package(CURL REQUIRED)\n"
        "find_package(Qt6 QUIET COMPONENTS Core Gui)\n"
        "target_link_libraries(app PRIVATE pbnotes_bridge ${LIBS} Qt6::Core pthread inkview m)\n"
        "target_link_libraries(notes_core PUBLIC pbnotes_sqlite CURL::libcurl)\n";
    auto names = scrape_cmake_names(cmake);
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    PB_CHECK_EQ(names.size(), size_t(3));
    PB_CHECK_EQ(names[0], std::string("CURL"));
    PB_CHECK_EQ(names[1], std::string("Qt6"));
    PB_CHECK_EQ(names[2], std::string("inkview"));
}

PB_TEST(rule_4_missing_empty_and_oversized_text_files) {
    Inputs in = good();
    in.size_of = [](const std::string& p) -> long {
        if (p == "LICENSE") return 1000;
        if (p == "third_party/sqlite") return 0;
        return -1;  // licenses/LGPL-3.0.txt is missing
    };
    PB_CHECK(has_finding(check_consistency(in), "rule 4:", "licenses/LGPL-3.0.txt"));
    in.size_of = [](const std::string& p) -> long { return p == "LICENSE" ? 1000 : p == "third_party/sqlite" ? 0 : 0; };
    PB_CHECK(has_finding(check_consistency(in), "rule 4:", "empty"));
    in.size_of = [](const std::string& p) -> long { return p == "LICENSE" ? 1000 : p == "third_party/sqlite" ? 0 : 256 * 1024 + 1; };
    PB_CHECK(has_finding(check_consistency(in), "rule 4:", "256 KiB"));
}

PB_TEST(rule_4_missing_source_path) {
    Inputs in = good();
    in.size_of = [](const std::string& p) -> long { return p == "LICENSE" || p == "licenses/LGPL-3.0.txt" ? 10 : -1; };
    PB_CHECK(has_finding(check_consistency(in), "rule 4:", "third_party/sqlite"));
}

PB_TEST(rule_5_copyright_line_differs_from_license) {
    Inputs in = good();
    in.manifest.app->copyright = "Copyright (c) 2027 Jane Author";  // year changed in the manifest only
    PB_CHECK(has_finding(check_consistency(in), "rule 5:", "differs"));
    Inputs none = good();
    none.license_file = "MIT License\n";
    PB_CHECK(has_finding(check_consistency(none), "rule 5:", "no Copyright line"));
}

PB_TEST(rule_6_bad_blocks_missing_app_and_unreadable_manifest) {
    Inputs in = good();
    in.manifest.bad_blocks = 2;
    PB_CHECK(has_finding(check_consistency(in), "rule 6:", "2 bad block"));
    Inputs no_app = good();
    no_app.manifest.app.reset();
    PB_CHECK(has_finding(check_consistency(no_app), "rule 6:", "no valid app block"));
    Inputs unreadable = good();
    unreadable.manifest.readable = false;
    PB_CHECK(has_finding(check_consistency(unreadable), "rule 6:", "not readable"));
}

// ---- the real repository ------------------------------------------------------------------------------

PB_TEST(the_real_repository_is_consistent) {
    namespace fs = std::filesystem;
    const std::string root = PBNOTES_SOURCE_DIR;
    Inputs in;
    in.manifest = pbnotes::about::load_manifest(root);
    in.notice = read_file(root + "/NOTICE");
    in.license_file = read_file(root + "/LICENSE");
    for (const auto& e : fs::directory_iterator(root + "/third_party")) {
        if (e.is_directory()) in.third_party_dirs.push_back(e.path().filename().string());
    }
    std::vector<std::string> cmake_files = {"CMakeLists.txt", "core/CMakeLists.txt", "bridge/CMakeLists.txt", "app/CMakeLists.txt"};
    for (const auto& e : fs::directory_iterator(root + "/cmake")) {
        if (e.path().extension() == ".cmake") cmake_files.push_back("cmake/" + e.path().filename().string());
    }
    for (const std::string& c : cmake_files) {
        for (const std::string& n : scrape_cmake_names(read_file(root + "/" + c))) in.cmake_names.push_back(n);
    }
    in.size_of = [root](const std::string& rel) -> long {
        struct stat st;
        if (::stat((root + "/" + rel).c_str(), &st) != 0) return -1;
        return S_ISDIR(st.st_mode) ? 0 : static_cast<long>(st.st_size);
    };
    const auto findings = check_consistency(in);
    for (const std::string& f : findings) std::printf("         finding: %s\n", f.c_str());
    PB_CHECK(findings.empty());
}

PB_TEST_MAIN()
