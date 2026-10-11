#pragma once
// UTF-8 helpers for text that may contain emojis (spec 002 FR-018). No Qt.

#include <cstddef>
#include <string_view>

namespace pbnotes {

// End (exclusive byte offset) of the display unit that starts at byte `pos` of `text`; `pos` must be at a
// code point start. A unit is a code point plus the parts that belong to it: variation selectors
// (U+FE0E/FE0F), emoji modifiers (U+1F3FB..1F3FF), combining marks, U+20E3 (keycap) and tag characters
// (U+E0020..E007F); parts joined by U+200D (zero width joiner, only when a character follows it); a pair of
// regional indicators (a flag). Every invalid byte is one unit. For pos >= size the size is returned; for
// pos < size the result is always greater than pos.
size_t utf8_cluster_end(std::string_view text, size_t pos);

}  // namespace pbnotes
