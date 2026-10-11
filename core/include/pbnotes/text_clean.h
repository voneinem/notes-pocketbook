#pragma once
// Text that arrives from outside the app (the clipboard) is untrusted (spec 005 FR-008, FR-032). No Qt.

#include <cstddef>
#include <string>
#include <string_view>

namespace pbnotes {

struct CleanedText {
    bool ok = false;
    std::string text;       // UTF-8, valid when ok
    std::string error_key;  // "paste_invalid" | "paste_too_large" when !ok
};

// External text made safe to insert. Drops control characters (U+0000-U+001F except '\n' and '\t', U+007F,
// U+0080-U+009F); "\r\n", "\r", U+2028 and U+2029 become "\n". Refuses invalid UTF-8 (including overlong forms,
// surrogates and values above U+10FFFF) and more than `max_chars` code points in the input.
CleanedText clean_pasted_text(std::string_view utf8, size_t max_chars = 1000000);

// Every run of line breaks and tabs becomes one space (for the one-line title). Ordinary spaces and the ends of
// the text are left alone.
std::string single_line(std::string_view cleaned);

}  // namespace pbnotes
