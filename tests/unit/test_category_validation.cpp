// T059: category rules — at most 255 characters, no leading/trailing space, no control characters;
// "/" is accepted; an invalid value keeps the previous category and returns an error key.
#include <string>

#include "pbnotes/note_store.h"
#include "pbnotes/title.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::EditError;
using pbnotes::NoteStore;
using pbnotes::validate_category;

PB_TEST(valid_categories) {
    for (const char* ok : {"", "Work", "Work/Projects/2026", "a  b", "Größe & Maß", "日本語", "x"})
        PB_CHECK_EQ(validate_category(ok), std::string());
}

PB_TEST(length_limit_counts_characters_not_bytes) {
    PB_CHECK_EQ(validate_category(std::string(255, 'a')), std::string());
    PB_CHECK_EQ(validate_category(std::string(256, 'a')), std::string("category_too_long"));
    std::string umlauts;
    for (int i = 0; i < 255; ++i) umlauts += "ä";  // 510 bytes, 255 characters
    PB_CHECK_EQ(validate_category(umlauts), std::string());
    PB_CHECK_EQ(validate_category(umlauts + "ä"), std::string("category_too_long"));
}

PB_TEST(leading_and_trailing_spaces_are_rejected) {
    PB_CHECK_EQ(validate_category(" lead"), std::string("category_spaces"));
    PB_CHECK_EQ(validate_category("trail "), std::string("category_spaces"));
    PB_CHECK_EQ(validate_category(" "), std::string("category_spaces"));
}

PB_TEST(control_characters_are_rejected) {
    for (const char* bad : {"a\tb", "a\nb", "a\rb", "\x01", "x\x1f", "a\x7f"})
        PB_CHECK_EQ(validate_category(bad), std::string("category_control_chars"));
    PB_CHECK_EQ(validate_category(std::string("a\0b", 3)), std::string("category_control_chars"));
}

PB_TEST(invalid_utf8_is_rejected) {
    PB_CHECK_EQ(validate_category("\xff"), std::string("category_invalid"));
    PB_CHECK_EQ(validate_category("abc\xc3"), std::string("category_invalid"));  // truncated sequence
}

PB_TEST(an_invalid_category_keeps_the_previous_value) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    auto id = store->create_local(pbnotes::derive_title("Note\nbody"), "Note\nbody", "Work", false, 1000).local_id;
    auto r = store->set_category(id, " bad ", 1100);
    PB_CHECK(r.error == EditError::InvalidCategory);
    PB_CHECK_EQ(r.error_key, std::string("category_spaces"));
    PB_CHECK_EQ(store->get(id)->category, std::string("Work"));
    PB_CHECK_EQ(store->get(id)->modified, static_cast<int64_t>(1000));
    auto r2 = store->create_local(pbnotes::derive_title("Another"), "Another", std::string(300, 'x'), false, 1200);
    PB_CHECK(r2.error == EditError::InvalidCategory);
    PB_CHECK_EQ(store->list().size(), static_cast<size_t>(1));  // the invalid create stored nothing
}

PB_TEST(slash_categories_work_end_to_end) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    auto id = store->create_local(pbnotes::derive_title("Note\nbody"), "Note\nbody", "Work/Projects", false, 1000).local_id;
    PB_CHECK_EQ(store->get(id)->category, std::string("Work/Projects"));
}

PB_TEST_MAIN()
