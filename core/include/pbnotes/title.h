#pragma once
// Title helpers. Since spec 004 a note's title is entered by the user and stored (never derived from the text);
// derive_title survives only as the fallback for a server note that has no title and in tests.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace pbnotes {

// First non-empty line of `text`, trimmed of ASCII whitespace; empty if the text has none.
// Multi-byte UTF-8 is never altered.
std::string derive_title(std::string_view text);

// "## Plan" -> "Plan": removes a leading run of 1-6 '#' followed by whitespace, then trims. Anything else
// (including "#hashtag" and a line that is only "###") is returned trimmed and otherwise unchanged.
std::string strip_heading_marker(std::string_view line);

// The title an unsynced note written before spec 004 is given by the schema-3 migration: what the list showed
// (strip_heading_marker), made to satisfy the title rules: control characters become spaces, the result is
// trimmed and cut after 255 code points. Never empty ("Untitled" if nothing is left).
std::string legacy_title(std::string_view stored);

// Preview text for the note list: `text` with whitespace runs collapsed to single spaces, at most `max_chars`
// characters (code points), never cutting a display unit. The first non-empty line is left out only if
// strip_heading_marker(line) == strip_heading_marker(title) (byte-exact otherwise); otherwise the preview starts at the beginning of the text.
// An empty `title` never matches.
std::string make_excerpt(std::string_view text, std::string_view title, std::size_t max_chars = 120);

// Simple case folding for searching and sorting: ASCII, Latin-1, Latin Extended-A, Greek and
// Cyrillic letters become lower case; everything else (including invalid UTF-8) is copied as is.
std::string fold_case(std::string_view utf8);

// Conflict-copy naming (spec 004): the suffix is " (conflict YYYY-MM-DD)" (UTC date) with " #n" appended for
// n >= 2; it is appended to the stored title of the copy, the text of the copy is the local text unchanged.
std::string conflict_suffix(int64_t epoch_seconds, int number = 1);

}  // namespace pbnotes
