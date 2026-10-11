// US4/US5 (spec 002 FR-019..FR-029, data-model.md "Reference validation", "Token operations"): the pure
// helpers for attachment references in note text.
#include <string>
#include <vector>

#include "pbnotes/attachment_ref.h"
#include "pbnotes/types.h"
#include "test_support.h"

using namespace pbnotes;

// ---- validation of a path (as the server knows it, decoded) ------------------------------------------

PB_TEST(a_normal_attachment_path_is_valid) {
    PB_CHECK_EQ(validate_attachment_path(".attachments.12/photo one.png").value_or("x"), std::string(".attachments.12/photo one.png"));
    PB_CHECK(validate_attachment_path("a.png").has_value());
    PB_CHECK(validate_attachment_path("../Images/a.png").has_value());   // the web editor writes such paths
    PB_CHECK(validate_attachment_path("dir/./a.png").has_value());
    PB_CHECK(validate_attachment_path("na\xC3\xAFve \xE2\x82\xAC.pdf").has_value());  // non-ASCII is fine
}

PB_TEST(empty_overlong_and_rooted_paths_are_rejected) {
    PB_CHECK(!validate_attachment_path("").has_value());
    PB_CHECK(!validate_attachment_path("/etc/passwd").has_value());
    PB_CHECK(!validate_attachment_path(std::string(1025, 'a')).has_value());
    PB_CHECK(validate_attachment_path(std::string(1024, 'a')).has_value());
}

PB_TEST(control_characters_nul_and_backslashes_are_rejected) {
    PB_CHECK(!validate_attachment_path(std::string("a") + '\0' + "b").has_value());
    PB_CHECK(!validate_attachment_path("a\nb.png").has_value());
    PB_CHECK(!validate_attachment_path("a\tb.png").has_value());
    PB_CHECK(!validate_attachment_path(std::string("a") + '\x7f' + ".png").has_value());
    PB_CHECK(!validate_attachment_path("dir\\a.png").has_value());
}

PB_TEST(a_scheme_is_rejected_except_the_local_token_scheme_which_is_not_a_path) {
    PB_CHECK(!validate_attachment_path("https://example.org/a.png").has_value());
    PB_CHECK(!validate_attachment_path("javascript:alert(1)").has_value());
    PB_CHECK(!validate_attachment_path("file:a.png").has_value());
    PB_CHECK(!validate_attachment_path("C:/a.png").has_value());
    PB_CHECK(validate_attachment_path("a:b/c.png").has_value() == false);   // "a:" looks like a scheme
    PB_CHECK(validate_attachment_path("dir/a:b.png").has_value());          // a colon after the first segment is fine
}

PB_TEST(an_empty_last_segment_is_rejected) {
    PB_CHECK(!validate_attachment_path("dir/").has_value());
    PB_CHECK(!validate_attachment_path(".attachments.12/").has_value());
}

// ---- decoding a target as written in the note (percent-encoded per segment) ---------------------------

PB_TEST(a_target_is_percent_decoded_and_validated) {
    PB_CHECK_EQ(decode_attachment_target(".attachments.12/photo%20one.png").value_or("x"),
                std::string(".attachments.12/photo one.png"));
    PB_CHECK_EQ(decode_attachment_target("a%28b%29.png").value_or("x"), std::string("a(b).png"));
    PB_CHECK_EQ(decode_attachment_target("%C3%A4.png").value_or("x"), std::string("\xC3\xA4.png"));
    PB_CHECK_EQ(decode_attachment_target("../Images/a.png").value_or("x"), std::string("../Images/a.png"));
}

PB_TEST(decoding_rejects_what_is_hidden_behind_percent_escapes) {
    PB_CHECK(!decode_attachment_target("a%00b.png").has_value());
    PB_CHECK(!decode_attachment_target("a%0Ab.png").has_value());
    PB_CHECK(!decode_attachment_target("dir%5Ca.png").has_value());    // %5C is a backslash
    PB_CHECK(!decode_attachment_target("%2Fetc/passwd").has_value());  // a decoded leading slash
    PB_CHECK(!decode_attachment_target("javascript%3Aalert(1)").has_value());
    PB_CHECK(!decode_attachment_target("a%zz.png").has_value());       // a bad escape
    PB_CHECK(!decode_attachment_target("a%2.png").has_value());        // a truncated escape
    PB_CHECK(!decode_attachment_target("").has_value());
}

PB_TEST(encoding_percent_encodes_each_segment_and_round_trips) {
    PB_CHECK_EQ(encode_attachment_path(".attachments.12/photo one.png"), std::string(".attachments.12/photo%20one.png"));
    PB_CHECK_EQ(encode_attachment_path("a(b).png"), std::string("a%28b%29.png"));
    PB_CHECK_EQ(encode_attachment_path("\xC3\xA4.png"), std::string("%C3%A4.png"));
    PB_CHECK_EQ(encode_attachment_path("../x/y z.pdf"), std::string("../x/y%20z.pdf"));
    for (const char* p : {".attachments.1/a b.png", "x/(y)/z%.png", "../\xE2\x82\xAC/\xF0\x9F\x98\x80.png", "plain.txt"})
        PB_CHECK_EQ(decode_attachment_target(encode_attachment_path(p)).value_or("x"), std::string(p));
}

// ---- classification ----------------------------------------------------------------------------------

PB_TEST(images_are_recognized_by_extension_case_insensitively) {
    for (const char* n : {"a.jpg", "a.JPEG", "a.png", "A.Gif", "a.bmp", "a.WEBP", "dir/x.png"})
        PB_CHECK(attachment_kind_for(n) == AttachmentKind::Image);
    for (const char* n : {"a.pdf", "a.svg", "a", "a.png.txt", "png", ".png.bak", "a.tiff", ""})
        PB_CHECK(attachment_kind_for(n) == AttachmentKind::File);
}

PB_TEST(safe_extension_is_lower_case_alphanumeric_up_to_eight_characters) {
    PB_CHECK_EQ(safe_extension("photo.PNG"), std::string("png"));
    PB_CHECK_EQ(safe_extension("archive.tar.gz"), std::string("gz"));
    PB_CHECK_EQ(safe_extension("a.abcdefgh"), std::string("abcdefgh"));
    PB_CHECK_EQ(safe_extension("a.abcdefghi"), std::string());     // nine characters
    PB_CHECK_EQ(safe_extension("noext"), std::string());
    PB_CHECK_EQ(safe_extension("trailing."), std::string());
    PB_CHECK_EQ(safe_extension("a.p ng"), std::string());
    PB_CHECK_EQ(safe_extension("a.p/ng"), std::string());          // only the last path segment counts
    PB_CHECK_EQ(safe_extension("a.p\xC3\xA4"), std::string());     // non-ASCII
    PB_CHECK_EQ(safe_extension("dir.x/file"), std::string());
}

PB_TEST(viewable_extensions_are_exactly_the_list_the_reader_opens) {
    for (const char* e : {"pdf", "epub", "fb2", "txt", "rtf", "doc", "docx", "html", "htm", "djvu", "mobi", "cbz", "cbr", "PDF"})
        PB_CHECK(is_viewable_extension(e));
    for (const char* e : {"", "png", "xyz", "exe", "zip", "pdfx", "pd", "sh"})
        PB_CHECK(!is_viewable_extension(e));
}

PB_TEST(a_display_name_is_the_last_segment_without_control_characters_and_at_most_255_characters) {
    PB_CHECK_EQ(attachment_file_name(".attachments.1/photo one.png"), std::string("photo one.png"));
    PB_CHECK_EQ(attachment_file_name("plain.txt"), std::string("plain.txt"));
    PB_CHECK_EQ(attachment_file_name(std::string("d/a\x01") + "b\x7f" + "c\nd.png"), std::string("abcd.png"));
    const std::string name = attachment_file_name("d/" + std::string(300, 'x') + ".png");
    PB_CHECK_EQ(name.size(), static_cast<size_t>(255));
    // never cut inside a UTF-8 sequence
    const std::string umlauts = attachment_file_name("d/" + [] { std::string s; for (int i = 0; i < 300; ++i) s += "\xC3\xA4"; return s; }());
    PB_CHECK_EQ(umlauts.size(), static_cast<size_t>(510));
}

// ---- tokens ------------------------------------------------------------------------------------------

PB_TEST(a_token_names_the_attachment_and_is_found_again) {
    PB_CHECK_EQ(make_token(7, "photo.png"), std::string("![photo.png](pbatt:7)"));
    PB_CHECK((find_tokens("text ![photo.png](pbatt:7) text\n![b](pbatt:12)") == std::vector<int64_t>{7, 12}));
    PB_CHECK(find_tokens("no token here ![x](a.png) [y](pbatt:3)").empty());  // only image syntax counts
}

PB_TEST(a_hostile_name_cannot_break_out_of_the_token) {
    const std::string t = make_token(5, "a]b[c\\d\ne\tf");
    PB_CHECK((find_tokens(t) == std::vector<int64_t>{5}));
    PB_CHECK_EQ(t.find('\n'), std::string::npos);
    PB_CHECK_EQ(t.rfind("](pbatt:5)"), t.size() - 10);
    PB_CHECK_EQ(make_token(1, std::string(500, 'x')).size(), make_token(1, std::string(100, 'x')).size());  // truncated
}

PB_TEST(replace_token_writes_the_encoded_server_path) {
    const std::string text = "intro\n![photo.png](pbatt:7)\nmore ![b](pbatt:8)";
    PB_CHECK_EQ(replace_token(text, 7, ".attachments.12/photo one.png"),
                std::string("intro\n![photo.png](.attachments.12/photo%20one.png)\nmore ![b](pbatt:8)"));
    PB_CHECK_EQ(replace_token(text, 99, "x.png"), text);   // unknown id: unchanged
}

PB_TEST(strip_tokens_removes_the_reference_and_a_lone_line_break) {
    PB_CHECK_EQ(strip_tokens("a\n![x](pbatt:1)\nb", {1}), std::string("a\nb"));
    PB_CHECK_EQ(strip_tokens("a ![x](pbatt:1) b", {1}), std::string("a  b"));          // mid-line: the text stays
    PB_CHECK_EQ(strip_tokens("![x](pbatt:1)", {1}), std::string());
    PB_CHECK_EQ(strip_tokens("a\n![x](pbatt:1)\n![y](pbatt:2)\nb", {1, 2}), std::string("a\nb"));
    PB_CHECK_EQ(strip_tokens("a\n![x](pbatt:1)\nb", {9}), std::string("a\n![x](pbatt:1)\nb"));  // unknown id
    PB_CHECK_EQ(strip_tokens("a\n![x](pbatt:1)\n![y](pbatt:2)", {1}), std::string("a\n![y](pbatt:2)"));
    PB_CHECK_EQ(strip_tokens("a\r\n![x](pbatt:1)\r\nb", {1}), std::string("a\r\nb"));
    PB_CHECK_EQ(strip_tokens("a\n![x](pbatt:1)", {1}), std::string("a"));            // the last line: the break before it goes
    PB_CHECK_EQ(strip_tokens("a\r\n![x](pbatt:1)", {1}), std::string("a"));
    PB_CHECK_EQ(strip_tokens("a\n![x](pbatt:1)\n![y](pbatt:2)", {1, 2}), std::string("a"));
}

PB_TEST_MAIN()
