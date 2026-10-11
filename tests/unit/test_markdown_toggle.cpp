// T087: toggle_task_item flips exactly one checkbox and leaves every other byte alone.
#include <string>

#include "pbnotes/markdown.h"
#include "test_support.h"

using pbnotes::toggle_task_item;

PB_TEST(unchecked_becomes_checked_and_back) {
    PB_CHECK_EQ(toggle_task_item("- [ ] milk\n- [x] bread\n", 0), std::string("- [x] milk\n- [x] bread\n"));
    PB_CHECK_EQ(toggle_task_item("- [ ] milk\n- [x] bread\n", 1), std::string("- [ ] milk\n- [ ] bread\n"));
    PB_CHECK_EQ(toggle_task_item("- [X] big X\n", 0), std::string("- [ ] big X\n"));
    PB_CHECK_EQ(toggle_task_item(toggle_task_item("- [ ] a\n", 0), 0), std::string("- [ ] a\n"));
}

PB_TEST(only_the_chosen_line_changes) {
    const std::string text = "Title\n\n- [ ] one\n- [ ] two\n  - [x] nested\n* [ ] star\n1. [ ] numbered\n\ntail";
    for (int line = 0; line < 9; ++line) {
        const std::string out = toggle_task_item(text, line);
        // count differing bytes: either none (not a task line) or exactly one
        int diff = 0;
        PB_CHECK_EQ(out.size(), text.size());
        for (size_t i = 0; i < text.size(); ++i) diff += out[i] != text[i];
        const bool is_task = line == 2 || line == 3 || line == 4 || line == 5 || line == 6;
        PB_CHECK_EQ(diff, is_task ? 1 : 0);
    }
}

PB_TEST(works_for_every_list_marker_and_nesting) {
    PB_CHECK_EQ(toggle_task_item("* [ ] a", 0), std::string("* [x] a"));
    PB_CHECK_EQ(toggle_task_item("+ [ ] a", 0), std::string("+ [x] a"));
    PB_CHECK_EQ(toggle_task_item("    - [ ] deep", 0), std::string("    - [x] deep"));
    PB_CHECK_EQ(toggle_task_item("\t- [ ] tab", 0), std::string("\t- [x] tab"));
    PB_CHECK_EQ(toggle_task_item("12. [x] twelve", 0), std::string("12. [ ] twelve"));
    PB_CHECK_EQ(toggle_task_item("2) [ ] paren", 0), std::string("2) [x] paren"));
}

PB_TEST(line_endings_and_unicode_are_untouched) {
    PB_CHECK_EQ(toggle_task_item("- [ ] Größe 日本語\r\n- [ ] next\r\n", 0), std::string("- [x] Größe 日本語\r\n- [ ] next\r\n"));
    PB_CHECK_EQ(toggle_task_item("- [ ] last without newline", 0), std::string("- [x] last without newline"));
}

PB_TEST(non_task_lines_and_bad_indexes_return_the_text_unchanged) {
    const std::string text = "Plain line\n- bullet\n- [ ]nospace\n- [y] other\n```\n- [ ] in code\n```\n";
    for (int line : {0, 1, 2, 3, 5, -1, 7, 100}) PB_CHECK_EQ(toggle_task_item(text, line), text);
    PB_CHECK_EQ(toggle_task_item("", 0), std::string());
}

PB_TEST(a_task_box_inside_a_fence_of_either_kind_is_not_toggled_but_one_after_it_is) {
    const std::string text = "~~~\n- [ ] in tilde fence\n~~~\n- [ ] after\n````\n- [ ] in long fence\n```\n- [ ] still in it\n````\n- [ ] last";
    PB_CHECK_EQ(toggle_task_item(text, 1), text);
    PB_CHECK_EQ(toggle_task_item(text, 5), text);
    PB_CHECK_EQ(toggle_task_item(text, 7), text);
    PB_CHECK(toggle_task_item(text, 3) != text);
    PB_CHECK(toggle_task_item(text, 9) != text);
}

PB_TEST(a_task_box_in_a_quote_or_a_details_section_is_toggled_in_place) {
    PB_CHECK_EQ(toggle_task_item("> - [ ] task", 0), std::string("> - [x] task"));
    PB_CHECK_EQ(toggle_task_item(">   - [x] nested", 0), std::string(">   - [ ] nested"));
    const std::string t = "<details>\n<summary>S</summary>\n- [ ] inside\n</details>";
    PB_CHECK_EQ(toggle_task_item(t, 2), std::string("<details>\n<summary>S</summary>\n- [x] inside\n</details>"));
}

PB_TEST_MAIN()
