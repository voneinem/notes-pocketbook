// US3 (spec 002 FR-018, research R-12): utf8_cluster_end finds the end of the display unit that starts at a
// byte position, so text is never cut inside an emoji sequence.
#include <string>
#include <vector>

#include "pbnotes/utf8.h"
#include "test_support.h"

using pbnotes::utf8_cluster_end;

namespace {

// All units of `s`, found by walking with utf8_cluster_end.
std::vector<std::string> units(const std::string& s) {
    std::vector<std::string> out;
    size_t pos = 0;
    while (pos < s.size()) {
        const size_t end = utf8_cluster_end(s, pos);
        if (end <= pos) PB_FAIL_AT("no progress at byte " + std::to_string(pos));
        out.push_back(s.substr(pos, end - pos));
        pos = end;
    }
    return out;
}

const std::string kGrin = "\U0001F600";
const std::string kHeart = "❤️";
const std::string kThumbSkin = "\U0001F44D\U0001F3FD";
const std::string kFamily = "\U0001F468‍\U0001F469‍\U0001F467";
const std::string kFlagDE = "\U0001F1E9\U0001F1EA";
const std::string kFlagFR = "\U0001F1EB\U0001F1F7";
const std::string kKeycap = "1️⃣";
const std::string kEngland = "\U0001F3F4\U000E0067\U000E0062\U000E0065\U000E006E\U000E0067\U000E007F";

}  // namespace

PB_TEST(a_single_emoji_is_one_unit) {
    PB_CHECK_EQ(utf8_cluster_end(kGrin, 0), kGrin.size());
    PB_CHECK_EQ(utf8_cluster_end("a" + kGrin, 1), 1 + kGrin.size());
}

PB_TEST(a_variation_selector_stays_with_its_base) {
    PB_CHECK_EQ(utf8_cluster_end(kHeart, 0), kHeart.size());
    PB_CHECK_EQ(units(kHeart + "x").size(), static_cast<size_t>(2));
}

PB_TEST(a_skin_tone_modifier_stays_with_its_base) {
    PB_CHECK_EQ(utf8_cluster_end(kThumbSkin, 0), kThumbSkin.size());
}

PB_TEST(a_zwj_family_is_one_unit) {
    PB_CHECK_EQ(utf8_cluster_end(kFamily, 0), kFamily.size());
    PB_CHECK_EQ(units(kFamily + kGrin).size(), static_cast<size_t>(2));
}

PB_TEST(a_flag_is_one_unit_and_two_flags_are_two) {
    PB_CHECK_EQ(utf8_cluster_end(kFlagDE, 0), kFlagDE.size());
    auto u = units(kFlagDE + kFlagFR);
    PB_CHECK_EQ(u.size(), static_cast<size_t>(2));
    PB_CHECK_EQ(u[0], kFlagDE);
    PB_CHECK_EQ(u[1], kFlagFR);
}

PB_TEST(a_lone_regional_indicator_is_one_unit) {
    const std::string lone = "\U0001F1E9";
    PB_CHECK_EQ(utf8_cluster_end(lone + "a", 0), lone.size());
}

PB_TEST(a_keycap_is_one_unit) {
    PB_CHECK_EQ(utf8_cluster_end(kKeycap, 0), kKeycap.size());
    PB_CHECK_EQ(units(kKeycap + "2").size(), static_cast<size_t>(2));
}

PB_TEST(a_tag_sequence_is_one_unit) {
    PB_CHECK_EQ(utf8_cluster_end(kEngland, 0), kEngland.size());
}

PB_TEST(a_base_letter_keeps_its_combining_mark) {
    const std::string e_acute = "é";
    PB_CHECK_EQ(utf8_cluster_end(e_acute + "x", 0), e_acute.size());
}

PB_TEST(plain_text_is_one_code_point_per_unit) {
    PB_CHECK_EQ(utf8_cluster_end("ab", 0), static_cast<size_t>(1));
    auto u = units("aä€\U0001D11E");  // a, a-umlaut, euro, musical G clef
    PB_CHECK_EQ(u.size(), static_cast<size_t>(4));
    PB_CHECK_EQ(u[1].size(), static_cast<size_t>(2));
    PB_CHECK_EQ(u[2].size(), static_cast<size_t>(3));
    PB_CHECK_EQ(u[3].size(), static_cast<size_t>(4));
}

PB_TEST(a_joiner_without_a_following_character_is_not_swallowed) {
    const std::string s = "\U0001F468‍";
    auto u = units(s);
    PB_CHECK_EQ(u.size(), static_cast<size_t>(2));
}

PB_TEST(invalid_bytes_are_one_unit_each) {
    PB_CHECK_EQ(utf8_cluster_end("\xFF" "a", 0), static_cast<size_t>(1));
    PB_CHECK_EQ(utf8_cluster_end("\x80" "a", 0), static_cast<size_t>(1));
    PB_CHECK_EQ(utf8_cluster_end("\xC3", 0), static_cast<size_t>(1));         // truncated at the end
    PB_CHECK_EQ(utf8_cluster_end("\xE2\x82", 0), static_cast<size_t>(1));     // truncated
    PB_CHECK_EQ(utf8_cluster_end("\xC3(", 0), static_cast<size_t>(1));        // bad continuation
    PB_CHECK_EQ(units("\xFF\xFE\xFD").size(), static_cast<size_t>(3));
}

PB_TEST(at_the_end_of_the_text_the_size_is_returned) {
    PB_CHECK_EQ(utf8_cluster_end("", 0), static_cast<size_t>(0));
    PB_CHECK_EQ(utf8_cluster_end("a", 1), static_cast<size_t>(1));
    PB_CHECK_EQ(utf8_cluster_end(kGrin, kGrin.size()), kGrin.size());
}

PB_TEST(the_result_is_always_past_the_start_inside_the_text) {
    const std::string mixed = "a" + kGrin + kHeart + "\xFF" + kFamily + kFlagDE + kKeycap + "é" + kEngland + "z";
    size_t pos = 0, count = 0;
    while (pos < mixed.size()) {
        const size_t end = utf8_cluster_end(mixed, pos);
        PB_CHECK(end > pos && end <= mixed.size());
        pos = end;
        ++count;
    }
    PB_CHECK_EQ(pos, mixed.size());
    PB_CHECK_EQ(count, static_cast<size_t>(10));  // a, grin, heart, 0xFF, family, flag, keycap, e-acute, england, z
}

PB_TEST_MAIN()
