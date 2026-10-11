// About box (spec 003): parsing and loading of licenses/manifest.txt (contracts/manifest-format.md,
// contracts/core-library.md). The manifest is external input: every limit and path rule is checked here.
#include <sys/stat.h>

#include <string>

#include "pbnotes/about.h"
#include "test_support.h"
#include "tmp_dir.h"

using namespace pbnotes::about;

namespace {

const char* kApp =
    "id: pbnotes\n"
    "kind: app\n"
    "name: Notes Pocketbook\n"
    "license: MIT License\n"
    "holder: Jane Author\n"
    "copyright: Copyright (c) 2026 Jane Author\n"
    "text: LICENSE\n";

const char* kSqlite =
    "id: sqlite\n"
    "kind: component\n"
    "name: SQLite\n"
    "purpose: Stores the notes on the reader\n"
    "license: Public domain\n"
    "holder: The SQLite authors\n"
    "url: https://www.sqlite.org/copyright.html\n"
    "origin: bundled\n"
    "source: third_party/sqlite\n";

const char* kQt =
    "id: qt\n"
    "kind: component\n"
    "name: Qt 6.8\n"
    "purpose: Draws the screens\n"
    "license: LGPL-3.0\n"
    "holder: The Qt Company Ltd.\n"
    "url: https://www.qt.io/licensing/\n"
    "origin: system\n"
    "text: licenses/LGPL-3.0.txt, licenses/GPL-3.0.txt\n"
    "cmake: Qt6\n";

std::string join(std::initializer_list<const char*> blocks) {
    std::string s;
    for (const char* b : blocks) { s += b; s += "\n"; }
    return s;
}

// A component block with one line replaced or added.
std::string component_with(const std::string& key, const std::string& value) {
    std::string out;
    bool done = false;
    std::string src = kSqlite;
    size_t pos = 0;
    while (pos < src.size()) {
        size_t nl = src.find('\n', pos);
        std::string line = src.substr(pos, nl - pos);
        pos = nl + 1;
        if (line.compare(0, key.size() + 1, key + ":") == 0) {
            if (!value.empty()) out += key + ": " + value + "\n";
            done = true;
        } else {
            out += line + "\n";
        }
    }
    if (!done && !value.empty()) out += key + ": " + value + "\n";
    return out;
}

}  // namespace

PB_TEST(well_formed_manifest_parses_in_file_order) {
    Manifest m = parse_manifest(join({kApp, kSqlite, kQt}));
    PB_CHECK(m.readable);
    PB_CHECK(m.app.has_value());
    PB_CHECK_EQ(m.app->name, std::string("Notes Pocketbook"));
    PB_CHECK_EQ(m.app->holder, std::string("Jane Author"));
    PB_CHECK_EQ(m.app->copyright, std::string("Copyright (c) 2026 Jane Author"));
    PB_CHECK_EQ(m.app->text.size(), size_t(1));
    PB_CHECK_EQ(m.components.size(), size_t(2));
    PB_CHECK_EQ(m.components[0].id, std::string("sqlite"));
    PB_CHECK(m.components[0].origin == Origin::Bundled);
    PB_CHECK_EQ(m.components[0].source.size(), size_t(1));
    PB_CHECK_EQ(m.components[1].id, std::string("qt"));
    PB_CHECK(m.components[1].origin == Origin::System);
    PB_CHECK_EQ(m.components[1].text.size(), size_t(2));
    PB_CHECK_EQ(m.components[1].text[1], std::string("licenses/GPL-3.0.txt"));
    PB_CHECK_EQ(m.components[1].cmake[0], std::string("Qt6"));
    PB_CHECK_EQ(m.bad_blocks, 0);
}

PB_TEST(empty_input_is_readable_and_empty) {
    Manifest m = parse_manifest("");
    PB_CHECK(m.readable);
    PB_CHECK(!m.app.has_value());
    PB_CHECK(m.components.empty());
    PB_CHECK_EQ(m.bad_blocks, 0);
    Manifest only_blank = parse_manifest("\n\n  \n");
    PB_CHECK(only_blank.readable);
    PB_CHECK_EQ(only_blank.bad_blocks, 0);
}

PB_TEST(missing_required_key_drops_only_that_block) {
    std::string no_license = component_with("license", "");
    Manifest m = parse_manifest(join({kApp, no_license.c_str(), kQt}));
    PB_CHECK_EQ(m.bad_blocks, 1);
    PB_CHECK_EQ(m.components.size(), size_t(1));
    PB_CHECK_EQ(m.components[0].id, std::string("qt"));
    PB_CHECK(m.app.has_value());
}

PB_TEST(each_required_component_key_is_required) {
    for (const char* key : {"id", "kind", "name", "purpose", "license", "holder", "url", "origin"}) {
        std::string b = component_with(key, "");
        Manifest m = parse_manifest(b);
        PB_CHECK_EQ(m.bad_blocks, 1);
        PB_CHECK(m.components.empty());
    }
}

PB_TEST(app_block_needs_copyright) {
    std::string app = "id: pbnotes\nkind: app\nname: X\nlicense: MIT\nholder: A\n";
    Manifest m = parse_manifest(app);
    PB_CHECK(!m.app.has_value());
    PB_CHECK_EQ(m.bad_blocks, 1);
}

PB_TEST(duplicate_id_drops_the_later_block) {
    Manifest m = parse_manifest(join({kSqlite, kSqlite}));
    PB_CHECK_EQ(m.components.size(), size_t(1));
    PB_CHECK_EQ(m.bad_blocks, 1);
}

PB_TEST(second_app_block_is_dropped) {
    std::string app2 = std::string(kApp);
    app2.replace(app2.find("id: pbnotes"), 11, "id: other");
    Manifest m = parse_manifest(join({kApp, app2.c_str()}));
    PB_CHECK(m.app.has_value());
    PB_CHECK_EQ(m.app->id, std::string("pbnotes"));
    PB_CHECK_EQ(m.bad_blocks, 1);
}

PB_TEST(unsafe_text_paths_drop_the_block) {
    for (const char* bad : {"../x", "licenses/../../x", "/etc/passwd", "a\\b", "..", "./../x"}) {
        std::string b = component_with("text", bad);
        Manifest m = parse_manifest(b);
        PB_CHECK_EQ(m.bad_blocks, 1);
        PB_CHECK(m.components.empty());
    }
    std::string with_nul = component_with("text", std::string("a") + '\0' + "b");
    PB_CHECK_EQ(parse_manifest(with_nul).bad_blocks, 1);
    std::string long_path = component_with("text", std::string(201, 'a'));
    PB_CHECK_EQ(parse_manifest(long_path).bad_blocks, 1);
    std::string ok_path = component_with("text", std::string(200, 'a'));
    PB_CHECK_EQ(parse_manifest(ok_path).bad_blocks, 0);
    // one bad item among several drops the block
    std::string mixed = component_with("text", "licenses/ok.txt, ../bad.txt");
    PB_CHECK_EQ(parse_manifest(mixed).bad_blocks, 1);
}

PB_TEST(field_limits_id_name_purpose_license_url_origin_kind) {
    // id: [a-z0-9-]+, <= 40 characters
    PB_CHECK_EQ(parse_manifest(component_with("id", "Bad_Id")).bad_blocks, 1);
    PB_CHECK_EQ(parse_manifest(component_with("id", std::string(41, 'a'))).bad_blocks, 1);
    PB_CHECK_EQ(parse_manifest(component_with("id", std::string(40, 'a'))).bad_blocks, 0);
    // name <= 80
    PB_CHECK_EQ(parse_manifest(component_with("name", std::string(81, 'n'))).bad_blocks, 1);
    PB_CHECK_EQ(parse_manifest(component_with("name", std::string(80, 'n'))).bad_blocks, 0);
    // purpose <= 120
    PB_CHECK_EQ(parse_manifest(component_with("purpose", std::string(121, 'p'))).bad_blocks, 1);
    PB_CHECK_EQ(parse_manifest(component_with("purpose", std::string(120, 'p'))).bad_blocks, 0);
    // license <= 120
    PB_CHECK_EQ(parse_manifest(component_with("license", std::string(121, 'l'))).bad_blocks, 1);
    PB_CHECK_EQ(parse_manifest(component_with("license", std::string(120, 'l'))).bad_blocks, 0);
    // url: https://... <= 200
    PB_CHECK_EQ(parse_manifest(component_with("url", "http://example.org")).bad_blocks, 1);
    PB_CHECK_EQ(parse_manifest(component_with("url", "example.org")).bad_blocks, 1);
    PB_CHECK_EQ(parse_manifest(component_with("url", "https://" + std::string(193, 'u'))).bad_blocks, 1);
    PB_CHECK_EQ(parse_manifest(component_with("url", "https://" + std::string(192, 'u'))).bad_blocks, 0);
    // origin is bundled or system
    PB_CHECK_EQ(parse_manifest(component_with("origin", "elsewhere")).bad_blocks, 1);
    // kind is app or component
    PB_CHECK_EQ(parse_manifest(component_with("kind", "library")).bad_blocks, 1);
}

PB_TEST(line_that_is_not_key_value_makes_a_bad_block) {
    std::string b = std::string(kSqlite) + "this line has no colon\n";
    Manifest m = parse_manifest(join({b.c_str(), kQt}));
    PB_CHECK_EQ(m.bad_blocks, 1);
    PB_CHECK_EQ(m.components.size(), size_t(1));
    PB_CHECK_EQ(m.components[0].id, std::string("qt"));
}

PB_TEST(unknown_keys_comments_and_repeated_keys) {
    std::string b = std::string("# a comment\n") + kSqlite + "future_key: whatever\nname: Second Name\n";
    Manifest m = parse_manifest(b);
    PB_CHECK_EQ(m.bad_blocks, 0);
    PB_CHECK_EQ(m.components.size(), size_t(1));
    PB_CHECK_EQ(m.components[0].name, std::string("Second Name"));  // last one wins
}

PB_TEST(crlf_and_bom_are_accepted) {
    std::string crlf;
    for (char c : join({kApp, kSqlite})) {
        if (c == '\n') crlf += "\r\n"; else crlf += c;
    }
    Manifest m = parse_manifest("\xEF\xBB\xBF" + crlf);
    PB_CHECK_EQ(m.bad_blocks, 0);
    PB_CHECK(m.app.has_value());
    PB_CHECK_EQ(m.app->holder, std::string("Jane Author"));
    PB_CHECK_EQ(m.components.size(), size_t(1));
}

PB_TEST(values_are_trimmed_and_list_items_trimmed_empty_ignored) {
    std::string b = component_with("text", "  licenses/a.txt ,, licenses/b.txt , ");
    Manifest m = parse_manifest(b);
    PB_CHECK_EQ(m.bad_blocks, 0);
    PB_CHECK_EQ(m.components[0].text.size(), size_t(2));
    PB_CHECK_EQ(m.components[0].text[0], std::string("licenses/a.txt"));
    PB_CHECK_EQ(m.components[0].text[1], std::string("licenses/b.txt"));
}

PB_TEST(manifest_over_64_kib_is_not_readable) {
    std::string big = join({kApp, kSqlite});
    big += "# " + std::string(64 * 1024, 'x') + "\n";
    Manifest m = parse_manifest(big);
    PB_CHECK(!m.readable);
    PB_CHECK(!m.app.has_value());
    PB_CHECK(m.components.empty());
    std::string ok = join({kApp, kSqlite});
    ok += "# " + std::string(64 * 1024 - ok.size() - 3, 'x') + "\n";   // exactly 64 KiB
    PB_CHECK_EQ(ok.size(), size_t(64 * 1024));
    PB_CHECK(parse_manifest(ok).readable);
}

PB_TEST(only_the_first_64_blocks_are_read) {
    std::string s;
    for (int i = 0; i < 70; ++i) {
        s += "id: c" + std::to_string(i) + "\nkind: component\nname: C\npurpose: p\nlicense: l\nholder: h\n"
             "url: https://example.org\norigin: bundled\n\n";
    }
    Manifest m = parse_manifest(s);
    PB_CHECK_EQ(m.components.size(), size_t(64));
    PB_CHECK_EQ(m.components.back().id, std::string("c63"));
}

PB_TEST(load_manifest_reads_the_file_below_the_root) {
    pbtest::TmpDir root;
    ::mkdir(root.file("licenses").c_str(), 0755);
    root.write("licenses/manifest.txt", join({kApp, kSqlite}));
    Manifest m = load_manifest(root.path());
    PB_CHECK(m.readable);
    PB_CHECK(m.app.has_value());
    PB_CHECK_EQ(m.components.size(), size_t(1));
}

PB_TEST(load_manifest_of_a_missing_file_is_not_readable) {
    pbtest::TmpDir root;
    Manifest m = load_manifest(root.path());
    PB_CHECK(!m.readable);
    PB_CHECK(!m.app.has_value());
    PB_CHECK(m.components.empty());
    Manifest nowhere = load_manifest(root.file("does-not-exist"));
    PB_CHECK(!nowhere.readable);
}

PB_TEST(load_manifest_over_64_kib_is_not_readable) {
    pbtest::TmpDir root;
    ::mkdir(root.file("licenses").c_str(), 0755);
    root.write("licenses/manifest.txt", "# " + std::string(70 * 1024, 'x') + "\n");
    PB_CHECK(!load_manifest(root.path()).readable);
}

PB_TEST_MAIN()
