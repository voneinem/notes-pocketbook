# Contract: core library (`notes_core`)

New and changed public headers. All are Qt-free and unit-tested on the host (constitution V).
Existing behavior is unchanged unless stated.

## `pbnotes/edit_history.h` (new)

```cpp
namespace pbnotes {

enum class EditField { Title, Text, Category, Favorite };

struct EditApplied {                 // what the editor must do for an undo or redo
    EditField field;
    size_t pos = 0;                  // Title/Text: delete remove_len UTF-16 units at pos, insert `insert` there
    size_t remove_len = 0;
    std::u16string insert;
    std::u16string value;            // Category/Favorite: the value to set ("1"/"" for Favorite)
    size_t sel_begin = 0, sel_end = 0;   // select this range afterwards (equal = caret)
};

class EditHistory {
public:
    EditHistory(size_t max_steps = 200, size_t max_chars = 2'000'000);

    // Starts a history at the opened values; clears both stacks, ends any group or apply.
    void reset(std::u16string title, std::u16string text, std::u16string category, bool favorite);
    bool active() const;             // false before the first reset: observe() is ignored

    // Reports the field's new value. No-op when equal to the known value, while applying, or
    // while inactive. May merge into the newest step (typing / delete runs); otherwise pushes one.
    void observe(EditField f, std::u16string_view value, int64_t now_ms);

    // A compound command (Cut, Delete, Paste over a selection): everything observed for `f`
    // until end_group() becomes at most one step. Nesting is not supported (a second begin ends
    // the first).
    void begin_group(EditField f);
    void end_group();

    bool can_undo() const;
    bool can_redo() const;
    std::optional<EditApplied> undo();   // nullopt when nothing to undo; sets "applying"
    std::optional<EditApplied> redo();   // nullopt when nothing to redo;  sets "applying"
    // Ends "applying" and re-syncs the known values with the editor's real values.
    void finish_apply(std::u16string_view title, std::u16string_view text);

    size_t undo_size() const;        // for tests and the memory cap
    size_t redo_size() const;
};
}
```

Behavior (the precise rules are in data-model.md):

- Steps are minimal diffs; `Title`/`Text` steps split on UTF-16 code *points* (a surrogate pair is
  never cut in half).
- Typing merges while: same field, adjacent, ≤ 1500 ms since the previous observation, merged
  insertion ≤ 80 units, no `\n`. Single-unit backspace/forward-delete runs merge likewise.
- `undo()`/`redo()` update the known values, move the step between stacks and return the
  `EditApplied`; the caller applies it and calls `finish_apply`.
- `reset()` is the only baseline; there is no step that removes opened content.
- Caps drop the oldest steps while `steps > max_steps` or `chars > max_chars`, never the newest.

## `pbnotes/text_clean.h` (new)

```cpp
namespace pbnotes {

struct CleanedText {
    bool ok = false;
    std::string text;                // UTF-8, valid when ok
    std::string error_key;           // "paste_invalid" | "paste_too_large" when !ok
};

// External text (the clipboard) made safe to insert. Drops control characters (U+0000-U+001F
// except '\n' and '\t', U+007F, U+0080-U+009F); "\r\n", "\r", U+2028, U+2029 become "\n". Refuses invalid UTF-8 and more than
// `max_chars` code points.
CleanedText clean_pasted_text(std::string_view utf8, size_t max_chars = 1'000'000);

// Every run of line breaks and tabs becomes one space (for the title field).
std::string single_line(std::string_view cleaned);
}
```

## `pbnotes/markdown.h` (extended)

```cpp
struct Span {
    // ...existing...
    std::vector<int> src;      // per byte of `text`: byte index in the block's source line; {} = unknown
    int el_begin = -1;         // source extent of the formatted element (marks included); -1 = unknown
    int el_end = -1;
};

struct Block {
    // ...existing...
    int src_begin = 0;         // extent of the whole block in its source line, prefix included
    int src_end = 0;           // (an Attachment: the reference as written)
};

// A displayed position: block index in the parse result and a byte offset into the block's
// displayed text (the concatenation of its span texts).
struct SourcePoint { size_t block = 0; size_t byte = 0; };

// The note's own source text for the displayed range [from, to] of `blocks` (parse_markdown(text)).
// `from` <= `to` in document order. `from_whole` / `to_whole` mean "the character range is the whole
// block" (also used for table rows, attachments and blank lines). See research R-9 for the rule.
std::string copy_source(std::string_view note_text, const std::vector<Block>& blocks,
                        SourcePoint from, SourcePoint to, bool from_whole = false, bool to_whole = false);
```

`copy_source` rules (tests in `test_copy_source`):

1. One block, a character range inside it: source bytes from the first selected byte's `src`
   (including a backslash of an escape) to the end of the last selected byte, then widened to the
   `[el_begin, el_end)` of every element whose bytes are *all* inside the range. No prefix, no
   marks of a partly selected element.
2. A range over several blocks: the first block as in rule 1 unless it is selected from its start
   to its end (then its whole line extent); blocks in between: their whole extent; the last block:
   its whole extent from the line start up to the last selected byte (prefix included), widened as
   in rule 1. Parts are joined by `\n`. A blank block is an empty line.
3. Table rows and attachment blocks: always their whole source line / reference. A table row's
   source line is the line `line_index + (row == 0 ? 0 : row + 1)` of the note.
4. `from > to` is the caller's error: the function returns an empty string.
5. Offsets beyond a block are clamped; invalid UTF-8 boundaries are moved to the next boundary.

Parser behavior that must stay unchanged: all fields of `Span` and `Block` that exist today, the
block count and order, the HTML the reader builds. `src`, `el_*` and `src_*` are additive.

## Unchanged

`validate_title`, `validate_category`, `make_excerpt`, the store, API client and sync engine: no
edit, selection or clipboard concept reaches them.
