#include "pbnotes/slash.h"

#include <algorithm>

namespace pbnotes {

namespace {

constexpr int kMinYear = 1970, kMaxYear = 2099;

bool blank(char16_t c) { return c == u' ' || c == u'\t'; }
bool boundary(char16_t c) { return c == u'\n' || blank(c); }

bool is_block(SlashEntry e) {
    switch (e) {
        case SlashEntry::BulletList:
        case SlashEntry::Table:
        case SlashEntry::InfoBox:
        case SlashEntry::Details:
        case SlashEntry::Quote:
        case SlashEntry::CodeBlock:
            return true;
        default:
            return false;
    }
}

bool is_multiline(SlashEntry e) {
    return e == SlashEntry::Table || e == SlashEntry::InfoBox || e == SlashEntry::Details || e == SlashEntry::CodeBlock;
}

struct Piece {
    const char16_t* text;
    size_t caret;  // offset of the caret in the text
};

Piece piece_of(SlashEntry e) {
    switch (e) {
        case SlashEntry::BulletList: return {u"- ", 2};
        case SlashEntry::Table: return {u"|  |  |\n| --- | --- |\n|  |  |", 2};
        case SlashEntry::InfoBox: return {u"> [!NOTE]\n> ", 12};
        case SlashEntry::Link: return {u"[](https://)", 1};
        case SlashEntry::Bold: return {u"****", 2};
        case SlashEntry::Italic: return {u"**", 1};
        case SlashEntry::Underline: return {u"<u></u>", 3};
        case SlashEntry::Strikethrough: return {u"~~~~", 2};
        case SlashEntry::Details: return {u"<details>\n<summary></summary>\n\n</details>", 19};
        case SlashEntry::Quote: return {u"> ", 2};
        case SlashEntry::CodeBlock: return {u"```\n\n```", 4};
        case SlashEntry::Emoji: break;
    }
    return {u"", 0};
}

// True when the line of `text` that contains offset `pos` holds non-blank text before `pos`.
bool text_before_on_line(std::u16string_view text, size_t pos) {
    for (size_t i = pos; i > 0; --i) {
        const char16_t c = text[i - 1];
        if (c == u'\n') return false;
        if (!blank(c)) return true;
    }
    return false;
}

// True when the line holds non-blank text from `from` on.
bool text_after_on_line(std::u16string_view text, size_t from) {
    for (size_t i = from; i < text.size(); ++i) {
        const char16_t c = text[i];
        if (c == u'\n') return false;
        if (!blank(c)) return true;
    }
    return false;
}

}  // namespace

const std::array<SlashEntryInfo, 12>& slash_entries() {
    static const std::array<SlashEntryInfo, 12> entries = {{
        {SlashEntry::BulletList, "bullet_list"},
        {SlashEntry::Table, "table"},
        {SlashEntry::InfoBox, "info_box"},
        {SlashEntry::Link, "link"},
        {SlashEntry::Bold, "bold"},
        {SlashEntry::Italic, "italic"},
        {SlashEntry::Underline, "underline"},
        {SlashEntry::Strikethrough, "strikethrough"},
        {SlashEntry::Details, "details"},
        {SlashEntry::Quote, "quote"},
        {SlashEntry::CodeBlock, "code_block"},
        {SlashEntry::Emoji, "emoji"},
    }};
    return entries;
}

SlashDetected detect_slash(std::u16string_view before, std::u16string_view after, size_t caret, size_t menu_pos) {
    SlashDetected none;
    if (after.size() != before.size() + 1 || caret == 0 || caret > after.size()) return none;
    const size_t p = caret - 1;  // the typed character sits right before the caret
    if (after[p] != u'/') return none;
    if (after.substr(0, p) != before.substr(0, p) || after.substr(p + 1) != before.substr(p)) return none;
    SlashDetected d;
    d.pos = p;
    if (menu_pos != kNoSlash && p > 0 && menu_pos == p - 1 && after[menu_pos] == u'/') {
        d.trigger = SlashTrigger::DateTime;
        return d;
    }
    if (p == 0 || boundary(after[p - 1])) d.trigger = SlashTrigger::Menu;
    return d;
}

SlashEdit expand_slash(SlashEntry entry, std::u16string_view text, size_t pos, size_t len) {
    SlashEdit edit;
    if (pos > text.size() || len > text.size() - pos) return edit;
    edit.begin = pos;
    edit.end = pos + len;
    if (entry == SlashEntry::Emoji) {
        edit.caret = pos;
        return edit;
    }
    const Piece piece = piece_of(entry);
    std::u16string out;
    size_t caret_offset = piece.caret;
    if (is_block(entry) && text_before_on_line(text, pos)) {
        out += u'\n';
        ++caret_offset;
    }
    out += piece.text;
    if (is_multiline(entry) && text_after_on_line(text, pos + len)) out += u'\n';
    edit.text = std::move(out);
    edit.caret = pos + caret_offset;
    return edit;
}

SlashEdit replace_slash(size_t pos, size_t len, std::u16string_view insert) {
    SlashEdit edit;
    edit.begin = pos;
    edit.end = pos + len;
    edit.text.assign(insert);
    edit.caret = pos + insert.size();
    return edit;
}

int days_in_month(int year, int month) {
    year = std::min(std::max(year, kMinYear), kMaxYear);
    month = std::min(std::max(month, 1), 12);
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return month == 2 && leap ? 29 : days[month - 1];
}

std::u16string format_timestamp(int year, int month, int day, int hour, int minute) {
    year = std::min(std::max(year, kMinYear), kMaxYear);
    month = std::min(std::max(month, 1), 12);
    day = std::min(std::max(day, 1), days_in_month(year, month));
    hour = std::min(std::max(hour, 0), 23);
    minute = std::min(std::max(minute, 0), 59);
    auto digits = [](int v, int width) {
        std::u16string s;
        for (int d = width - 1; d >= 0; --d) {
            int p = 1;
            for (int k = 0; k < d; ++k) p *= 10;
            s += static_cast<char16_t>(u'0' + (v / p) % 10);
        }
        return s;
    };
    return digits(year, 4) + u"-" + digits(month, 2) + u"-" + digits(day, 2) + u" " + digits(hour, 2) + u":" + digits(minute, 2);
}

}  // namespace pbnotes
