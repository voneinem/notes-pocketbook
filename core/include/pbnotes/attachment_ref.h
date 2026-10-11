#pragma once
// Attachment references in note text (spec 002, data-model.md "Reference validation", "Token operations").
// Pure string functions. A reference or file name that comes from a note or from the server is validated
// here before anything uses it, and it is never used to build a local path.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "pbnotes/types.h"

namespace pbnotes {

// The path as the server knows it (decoded): non-empty, at most 1,024 bytes, no NUL, control character or
// backslash, no scheme (`^[A-Za-z][A-Za-z0-9+.-]*:`), not starting with "/", and a non-empty last segment.
// ".." segments are allowed (the web editor writes them; the server confines them). Returns the path or nullopt.
std::optional<std::string> validate_attachment_path(std::string_view decoded_path);

// A reference target as written in the note text (each segment percent-encoded): decodes every segment
// (a bad or truncated escape fails; a segment that decodes to something containing "/" fails) and validates
// the result with validate_attachment_path.
std::optional<std::string> decode_attachment_target(std::string_view target);

// The inverse: percent-encodes every segment (everything except A-Z a-z 0-9 - . _ ~), keeping "/".
std::string encode_attachment_path(std::string_view path);

// Image for jpg, jpeg, png, gif, bmp, webp (case-insensitive), File otherwise.
AttachmentKind attachment_kind_for(std::string_view file_name);

// Lower-case extension of the last path segment if it is 1-8 characters of [A-Za-z0-9], else "".
std::string safe_extension(std::string_view file_name);

// True for the extensions the reader application opens: pdf, epub, fb2, txt, rtf, doc, docx, html, htm, djvu,
// mobi, cbz, cbr (case-insensitive; `extension` without the dot).
bool is_viewable_extension(std::string_view extension);

// Last path segment with control characters removed, at most 255 characters (code points).
std::string attachment_file_name(std::string_view path);

// ---- local tokens: "![name](pbatt:<id>)" for an attachment that is not on the server yet ---------------

// The token for attachment `id`; the name is sanitized ("[", "]", "\" and control characters replaced, at most
// 100 characters) so it cannot break out of the reference.
std::string make_token(int64_t id, std::string_view name);

// Ids of all tokens in image syntax, in order of appearance.
std::vector<int64_t> find_tokens(std::string_view content);

// The token of `id` with its target replaced by the percent-encoded server path; other text unchanged.
std::string replace_token(std::string_view content, int64_t id, std::string_view remote_path);

// Removes the whole reference of every listed id. A reference that stands alone on its line takes the
// following line break with it (on the last line, the break before it); in the middle of a line only the
// reference goes.
std::string strip_tokens(std::string_view content, const std::vector<int64_t>& ids);

}  // namespace pbnotes
