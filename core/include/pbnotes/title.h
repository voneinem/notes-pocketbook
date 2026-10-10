#pragma once
// Title and excerpt derivation (spec FR-019): the displayed title is ALWAYS the first
// non-empty line of the note text, computed locally.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace pbnotes {

// First non-empty line of `text`, trimmed of ASCII whitespace; empty if the text has none.
// Multi-byte UTF-8 is never altered.
std::string derive_title(std::string_view text);

// Text after the title line with whitespace runs collapsed to single spaces, at most
// `max_chars` characters (code points), never cutting a UTF-8 sequence.
std::string make_excerpt(std::string_view text, std::size_t max_chars = 120);

// Simple case folding for searching and sorting: ASCII, Latin-1, Latin Extended-A, Greek and
// Cyrillic letters become lower case; everything else (including invalid UTF-8) is copied as is.
std::string fold_case(std::string_view utf8);

// Conflict-copy naming. The suffix is " (conflict YYYY-MM-DD)" (UTC date) with " #n" appended for
// n >= 2; with_title_suffix writes it at the end of the first non-empty line of `content`
// (trailing blanks of that line are dropped, a CR before the line break is kept).
std::string conflict_suffix(int64_t epoch_seconds, int number = 1);
std::string with_title_suffix(std::string_view content, const std::string& suffix);

}  // namespace pbnotes
