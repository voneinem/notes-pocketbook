#pragma once
// The editor's "/" menu and "//" date-time picker (spec 006 US3, US5; contracts/core-library.md): when a typed "/"
// opens the menu, what every entry inserts and where the caret goes, and the date-time text. No Qt.
//
// Offsets are UTF-16 code units (the unit Qt's text items use, as in edit_history.h).

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace pbnotes {

enum class SlashEntry { BulletList, Table, InfoBox, Link, Bold, Italic, Underline, Strikethrough, Details, Quote, CodeBlock, Emoji };

struct SlashEntryInfo {
    SlashEntry entry;
    const char* id;  // "bullet_list" ... "emoji"
};

// The twelve entries, in the order the menu shows them.
const std::array<SlashEntryInfo, 12>& slash_entries();

constexpr size_t kNoSlash = static_cast<size_t>(-1);  // "no menu is open"

enum class SlashTrigger { None, Menu, DateTime };
struct SlashDetected {
    SlashTrigger trigger = SlashTrigger::None;
    size_t pos = 0;  // where the typed "/" is
};

// `before` -> `after` is the latest change of the note text and `caret` the caret in `after`; `menu_pos` is the
// position of the "/" the open menu belongs to (kNoSlash = none open).
//  Menu     : `after` is `before` with exactly one "/" inserted just before the caret (at p = caret - 1), and p is
//             0 or follows a line break, a space or a tab.
//  DateTime : the same insertion with menu_pos == p - 1 (the text now holds "//" there).
//  None     : anything else (a delete, a paste of several characters, "/" after a letter, digit or punctuation).
SlashDetected detect_slash(std::u16string_view before, std::u16string_view after, size_t caret, size_t menu_pos);

struct SlashEdit {  // replace [begin, end) of the text by `text`, then put the caret at `caret`
    size_t begin = 0, end = 0;
    std::u16string text;
    size_t caret = 0;  // absolute, in the text after the replacement
};

// The replacement for a chosen entry over the typed "/" at [pos, pos + len). Block entries (bullet list, table, info
// box, details, quote, code block) get a "\n" in front when the line holds non-blank text before `pos`, and the
// multi-line ones (table, info box, details, code block) a "\n" behind when non-blank text follows on the line. The
// Emoji entry has no text of its own: the result only removes the "/" (the emoji comes from replace_slash). An
// out-of-range position gives an empty edit (begin == end == 0, empty text).
SlashEdit expand_slash(SlashEntry entry, std::u16string_view text, size_t pos, size_t len);

// A plain replacement of [pos, pos + len) by `insert`, the caret behind it (an emoji, a date).
SlashEdit replace_slash(size_t pos, size_t len, std::u16string_view insert);

// "YYYY-MM-DD HH:MM", zero-padded, 24-hour. The arguments are clamped to year 1970-2099, month 1-12, day 1 to the
// month's length, hour 0-23, minute 0-59.
std::u16string format_timestamp(int year, int month, int day, int hour, int minute);
// 28..31 for the Gregorian calendar (the year is clamped like above, the month to 1-12).
int days_in_month(int year, int month);

}  // namespace pbnotes
