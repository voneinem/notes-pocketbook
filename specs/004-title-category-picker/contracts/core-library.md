# Contract: core library changes (Qt-free, host-tested)

Extends the core contracts of features 001 and 002. Everything lives in `notes_core`: no Qt, no
InkView (constitution V). Tests are written against these first (constitution I).

## title.h

```cpp
// "## Plan" -> "Plan": removes a leading run of 1-6 '#' followed by whitespace, then trims. Anything else
// (including "#hashtag" and a title that is only "###") is returned trimmed and otherwise unchanged.
std::string strip_heading_marker(std::string_view line);

// Preview text for the note list: `text` with whitespace runs collapsed, at most `max_chars` code points,
// never cutting a display unit. The first non-empty line is left out only if
// strip_heading_marker(line) == strip_heading_marker(title) (otherwise byte-exact); else the preview starts at
// the beginning of the text. An empty `title` never matches (nothing is dropped).
std::string make_excerpt(std::string_view text, std::string_view title, std::size_t max_chars = 120);

// The title the schema-3 migration gives an unsynced note written before spec 004: strip_heading_marker, control
// characters -> spaces, trimmed, cut after 255 code points, "Untitled" if nothing is left.
std::string legacy_title(std::string_view stored);

// kept: derive_title (legacy fallback in title_for and in tests), fold_case, conflict_suffix
// removed: with_title_suffix (conflict copies now carry the suffix in the title)
```

## note_store.h

```cpp
enum class EditError { None, EmptyContent, InvalidTitle, InvalidCategory, NotFound, StorageError };
// error_key additionally: title_empty | title_too_long | title_control_chars | title_invalid

// Title rules (data-model.md). Takes the title as given (untrimmed): returns "" if valid after trimming,
// else title_empty | title_too_long | title_control_chars | title_invalid. trim_title() is the trim used.
std::string validate_title(const std::string& title);
std::string trim_title(const std::string& title);

EditResult create_local(const std::string& title, const std::string& content, const std::string& category,
                        bool favorite, int64_t now);
EditResult save_local(int64_t local_id, const std::string& title, const std::string& content,
                      const std::string& category, bool favorite, int64_t now);
```

- Order of checks: title, then content (whitespace-only → `content_empty`), then category. The first
  failure is returned and nothing is stored (the previous version stays).
- A new note with an empty title *and* whitespace-only text is `content_empty` (it is never stored;
  FR-004's last sentence). A new note with text but no title is `title_empty`.
- `save_local` with an unchanged title, content, category and favorite changes nothing.
- `set_category`, `set_favorite`, `toggle_task`, and attachment rewrites keep the stored title.
- `PushSnapshot { title, content, category, favorite }`; `confirm_push` compares all four.
- `resolve_conflict(local_id, server, now)`: identical only if `title`, content, category and favorite
  all match; the copy is `{ title = local.title + suffix, content = local.content }`, numbering via
  `conflict_suffix(now, n)` while a note with that exact title exists.
- `title_for(server)` (internal): the server's title; for a server note without one, the first line with its
  heading marker removed (as the server derives it).
- `kSchemaVersion = 3` (data-only step in data-model.md); `open()` runs it for databases created at
  version 1 or 2.
- `categories()` unchanged (non-empty categories of all non-deleted notes, case-insensitive order).

## api_client.h

```cpp
struct NoteDraft {
    std::string title;      // always sent
    std::string content;
    std::string category;
    bool favorite = false;
    int64_t modified = 0;
};
```

Body: `{"title":…,"content":…,"category":…,"favorite":…,"modified":…}`; strings through the existing JSON
quoter. See [notes-api-title.md](notes-api-title.md).

## sync_engine.cpp

`draft_of(note)` and the push snapshot include `note.title`. A note whose stored title fails
`validate_title` (cannot happen through the editor; defensive) is not pushed and counts as a failed
note with the key `title_invalid`; the local text stays untouched.
