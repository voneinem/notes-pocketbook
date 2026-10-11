// About box (spec 003 FR-006, FR-007, FR-010, FR-016): reading license texts below the licenses root and
// splitting them into paragraphs for paged display. The paths come from a file on a user-writable volume,
// so every escape route is tested.
#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include "pbnotes/about.h"
#include "test_support.h"
#include "tmp_dir.h"

using namespace pbnotes::about;

namespace {

Entry entry_with(std::initializer_list<const char*> texts) {
    Entry e;
    e.id = "x";
    e.name = "X";
    for (const char* t : texts) e.text.push_back(t);
    return e;
}

}  // namespace

PB_TEST(paragraphs_split_at_blank_lines_and_keep_inner_line_breaks) {
    auto p = split_paragraphs("a\nb\n\n\nc");
    PB_CHECK_EQ(p.size(), size_t(2));
    PB_CHECK_EQ(p[0], std::string("a\nb"));
    PB_CHECK_EQ(p[1], std::string("c"));
}

PB_TEST(empty_or_blank_text_has_no_paragraphs) {
    PB_CHECK(split_paragraphs("").empty());
    PB_CHECK(split_paragraphs("\n\n").empty());
    PB_CHECK(split_paragraphs("  \n \t \n").empty());
}

PB_TEST(paragraphs_normalise_crlf_trim_trailing_spaces_and_drop_outer_blank_lines) {
    auto p = split_paragraphs("\r\n\r\nfirst  \r\n  indented\t\r\n\r\n\r\nsecond\r\n\r\n");
    PB_CHECK_EQ(p.size(), size_t(2));
    PB_CHECK_EQ(p[0], std::string("first\n  indented"));
    PB_CHECK_EQ(p[1], std::string("second"));
}

PB_TEST(paragraphs_are_capped_at_5000) {
    std::string t;
    for (int i = 0; i < 5100; ++i) t += "p" + std::to_string(i) + "\n\n";
    auto p = split_paragraphs(t);
    PB_CHECK_EQ(p.size(), size_t(5000));
    PB_CHECK_EQ(p.back(), std::string("p4999"));
}

PB_TEST(a_readable_file_gives_its_paragraphs) {
    pbtest::TmpDir root;
    root.write("LICENSE", "MIT License\n\nCopyright (c) 2026 X\n");
    LicenseText t = load_text(root.path(), entry_with({"LICENSE"}));
    PB_CHECK(t.available);
    PB_CHECK(!t.truncated);
    PB_CHECK_EQ(t.paragraphs.size(), size_t(2));
    PB_CHECK_EQ(t.paragraphs[0], std::string("MIT License"));
}

PB_TEST(several_files_get_a_heading_with_the_base_name_before_each) {
    pbtest::TmpDir root;
    ::mkdir(root.file("licenses").c_str(), 0755);
    root.write("licenses/LGPL-3.0.txt", "lgpl one\n\nlgpl two\n");
    root.write("licenses/GPL-3.0.txt", "gpl one\n");
    LicenseText t = load_text(root.path(), entry_with({"licenses/LGPL-3.0.txt", "licenses/GPL-3.0.txt"}));
    PB_CHECK(t.available);
    PB_CHECK_EQ(t.paragraphs.size(), size_t(5));
    PB_CHECK_EQ(t.paragraphs[0], std::string("LGPL-3.0"));
    PB_CHECK_EQ(t.paragraphs[1], std::string("lgpl one"));
    PB_CHECK_EQ(t.paragraphs[3], std::string("GPL-3.0"));
    PB_CHECK_EQ(t.paragraphs[4], std::string("gpl one"));
}

PB_TEST(a_single_file_has_no_heading) {
    pbtest::TmpDir root;
    root.write("a.txt", "only\n");
    LicenseText t = load_text(root.path(), entry_with({"a.txt"}));
    PB_CHECK_EQ(t.paragraphs.size(), size_t(1));
    PB_CHECK_EQ(t.paragraphs[0], std::string("only"));
}

PB_TEST(unreadable_files_are_skipped_when_another_can_be_read) {
    pbtest::TmpDir root;
    root.write("b.txt", "second\n");
    LicenseText t = load_text(root.path(), entry_with({"missing.txt", "b.txt"}));
    PB_CHECK(t.available);
    PB_CHECK_EQ(t.paragraphs.back(), std::string("second"));
}

PB_TEST(nothing_readable_or_no_text_is_not_available) {
    pbtest::TmpDir root;
    PB_CHECK(!load_text(root.path(), entry_with({"missing.txt"})).available);
    PB_CHECK(!load_text(root.path(), entry_with({})).available);
    root.write("empty.txt", "");
    PB_CHECK(!load_text(root.path(), entry_with({"empty.txt"})).available);
}

PB_TEST(a_file_over_256_kib_is_cut_on_a_line_boundary_and_marked) {
    pbtest::TmpDir root;
    std::string big;
    for (int i = 0; big.size() < 300 * 1024; ++i) big += "line " + std::to_string(i) + " of the long text\n";
    root.write("big.txt", big);
    LicenseText t = load_text(root.path(), entry_with({"big.txt"}));
    PB_CHECK(t.available);
    PB_CHECK(t.truncated);
    PB_CHECK_EQ(t.paragraphs.back(), std::string("Text shortened."));
    size_t total = 0;
    for (const std::string& p : t.paragraphs) total += p.size();
    PB_CHECK(total <= 256 * 1024 + 64);
    // the last text line is complete
    const std::string& last_text = t.paragraphs[t.paragraphs.size() - 2];
    PB_CHECK(last_text.size() >= 4 && last_text.compare(last_text.size() - 4, 4, "text") == 0);
}

PB_TEST(unsafe_paths_are_unreadable) {
    pbtest::TmpDir outer;
    outer.write("secret.txt", "do not show\n");
    ::mkdir(outer.file("root").c_str(), 0755);
    const std::string root = outer.file("root");
    for (const char* bad : {"../secret.txt", "/etc/hostname", "a\\b", "sub/../../secret.txt", ""}) {
        LicenseText t = load_text(root, entry_with({bad}));
        PB_CHECK(!t.available);
        PB_CHECK(t.paragraphs.empty());
    }
    std::string with_nul = std::string("secret") + '\0' + ".txt";
    PB_CHECK(!load_text(root, entry_with({with_nul.c_str()})).available);
}

PB_TEST(a_symlink_that_leaves_the_root_is_unreadable) {
    pbtest::TmpDir outer;
    outer.write("secret.txt", "do not show\n");
    ::mkdir(outer.file("root").c_str(), 0755);
    PB_CHECK_EQ(::symlink(outer.file("secret.txt").c_str(), outer.file("root/link.txt").c_str()), 0);
    PB_CHECK(!load_text(outer.file("root"), entry_with({"link.txt"})).available);
    // a link that stays inside the root is fine
    outer.write("root/real.txt", "fine\n");
    PB_CHECK_EQ(::symlink("real.txt", outer.file("root/inner.txt").c_str()), 0);
    PB_CHECK(load_text(outer.file("root"), entry_with({"inner.txt"})).available);
}

PB_TEST(a_directory_is_not_a_text) {
    pbtest::TmpDir root;
    ::mkdir(root.file("dir").c_str(), 0755);
    PB_CHECK(!load_text(root.path(), entry_with({"dir"})).available);
}

PB_TEST_MAIN()
