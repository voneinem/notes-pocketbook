#pragma once
// Link rules of the reader (spec 002 FR-001, FR-002, FR-004). Pure string functions.

#include <string_view>

namespace pbnotes {

// True for an http:// or https:// address (scheme in any letter case) with a non-empty host, no
// whitespace or control characters and at most 2048 bytes. Nothing else is ever opened.
bool is_followable_url(std::string_view url);

// False iff `text` is itself a web address naming the same destination as `url` after ignoring the
// scheme, a leading "www.", the letter case of the host and one trailing "/". True otherwise: the user
// is then shown the real address before the browser opens.
bool link_needs_confirmation(std::string_view text, std::string_view url);

}  // namespace pbnotes
