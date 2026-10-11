# Contract: core library (`notes_core`)

New and changed public headers. All are Qt-free and unit-tested on the host (constitution V).
Existing behavior is unchanged unless stated. Offsets in `slash.h` are UTF-16 code units (the unit
Qt's text items use, as in `edit_history.h`).

## `pbnotes/markdown.h` (changed)

```cpp
enum class BlockKind { Heading, Paragraph, ListItem, Blank, Table, Attachment,
                       Fence, Code, Callout, Summary, Marker };          // last five new

struct Span {                                  // + two flags
    enum Kind { Plain, Bold, Italic, BoldItalic };
    Kind kind = Plain;
    bool strike = false;                       // ~~text~~
    bool underline = false;                    // <u>text</u>
    /* text, url, link_confirm, src, el_begin, el_end unchanged */
};

enum class QuoteKind { None, Quote, Info };
enum class CalloutKind { Note, Tip, Warning, Important, Caution };

struct Block {                                 // + fields (defaults = "not applicable")
    QuoteKind quote = QuoteKind::None;
    CalloutKind callout = CalloutKind::Note;   // Callout blocks
    bool quote_first = false, quote_last = false;
    bool code_first = false, code_last = false;
    int details_id = -1;                       // 0-based line of the section's <details> line
    bool details_open = false;                 // written <details open>
    std::string fence_info;                    // opening Fence: the info string, tests only
    /* kind, line_index, line_count, level, ordered, number, depth, is_task, checked, spans, table,
       attachment, src_begin, src_end unchanged */
};

std::vector<Block> parse_markdown(std::string_view text);   // signature unchanged
```

Behavior (the rules are in `data-model.md`; the testable statements):

- Every source line still yields at least one block with its `line_index`; only a table covers
  several lines (`line_count`). `Fence`, `Code`, `Callout`, `Summary`, `Marker` are one line each.
- A fence line is never a `Paragraph`; `Code` spans are a single literal span (`literal_spans`) with
  `src` mapping bytes one to one. A `#`, `**`, `-`, `|`, `[a](b)` inside a fence stays literal.
- `quote` applies to any kind of block inside a quote group (heading, list item, paragraph, `Code`,
  `Fence`); `src_begin == 0` for quote lines so a whole selection copies the `> ` prefix.
- `parse_markdown` of a text without fences, quotes, `<details>`, `~~` and `<u>` produces exactly
  the blocks and spans it produced before (the new fields keep their defaults) — pinned by the
  existing tests plus a differential test on the fixtures.

```cpp
// copy_source (signature unchanged). New cases: Fence and Marker and Callout blocks copy their whole
// source line; Code and Summary behave like text blocks (a complete selection copies src_begin..
// src_end, a partial one only the selected characters); a quote's prefix is copied only when the
// whole block is selected; blocks hidden by a collapsed section are included like any other.
std::string copy_source(std::string_view note_text, const std::vector<Block>& blocks,
                        SourcePoint from, SourcePoint to);
```

`find_attachment_refs`, `tokenize_attachment_refs`, `toggle_task_item` use the shared fence rule
(research R-1): a reference or a task box inside a fenced block (backtick or tilde fence) is plain
text exactly where the reader shows code.

## `pbnotes/slash.h` (new)

```cpp
namespace pbnotes {

enum class SlashEntry { BulletList, Table, InfoBox, Link, Bold, Italic, Underline,
                        Strikethrough, Details, Quote, CodeBlock, Emoji };

struct SlashEntryInfo { SlashEntry entry; const char* id; };     // id: "bullet_list" ... "emoji"
// The twelve entries, in the order the menu shows them.
const std::array<SlashEntryInfo, 12>& slash_entries();

enum class SlashTrigger { None, Menu, DateTime };
struct SlashDetected { SlashTrigger trigger = SlashTrigger::None; size_t pos = 0; };  // pos of the "/" to replace

// `before` -> `after` is the latest change of the note text, `caret` the caret in `after`, `menu_pos`
// the position of the "/" the open menu belongs to (npos = no menu open).
//  Menu     : `after` is `before` with exactly one "/" inserted at p, caret == p+1, and p is 0 or after[p-1]
//             is '\n', ' ' or '\t'.
//  DateTime : the same insertion at p, menu_pos == p-1 (so the text now holds "//" at p-1).
//  None     : anything else (a delete, a paste of several characters, "/" after a letter/digit/punctuation).
SlashDetected detect_slash(std::u16string_view before, std::u16string_view after, size_t caret, size_t menu_pos);

struct SlashEdit {                  // replace [begin, end) of the text by `text`, then put the caret
    size_t begin = 0, end = 0;
    std::u16string text;
    size_t caret = 0;               // absolute position in the text after the replacement
};

// The replacement for a chosen entry (not Emoji) over the typed "/" at [pos, pos+len). Block entries get a
// "\n" in front when the line holds non-blank text before pos, and a "\n" behind when the replacement spans
// several lines and text follows on the line.
SlashEdit expand_slash(SlashEntry entry, std::u16string_view text, size_t pos, size_t len);

// Plain replacement of [pos, pos+len) by `insert` (an emoji, a date), caret after it.
SlashEdit replace_slash(size_t pos, size_t len, std::u16string_view insert);

// "YYYY-MM-DD HH:MM", zero-padded, 24-hour. Arguments are not range-checked beyond clamping to the valid ranges.
std::u16string format_timestamp(int year, int month, int day, int hour, int minute);
int days_in_month(int year, int month);          // 28..31, leap years by the Gregorian rule

}  // namespace pbnotes
```

Exact replacements (⏎ = `\n`; `‸` = the caret, not part of the text; the table's cells are two
spaces, so the caret sits between them in the first header cell):

| Entry | Text | Caret (offset in the replacement) |
|-------|------|-----------------------------------|
| BulletList | `- ` | 2 |
| Table | `\|  \|  \|` ⏎ `\| --- \| --- \|` ⏎ `\|  \|  \|` | 2 |
| InfoBox | `> [!NOTE]` ⏎ `> ` | end |
| Link | `[](https://)` | 1 |
| Bold | `****` | 2 |
| Italic | `**` | 1 |
| Underline | `<u></u>` | 3 |
| Strikethrough | `~~~~` | 2 |
| Details | `<details>` ⏎ `<summary></summary>` ⏎ ⏎ `</details>` | 19 |
| Quote | `> ` | 2 |
| CodeBlock | ```` ``` ```` ⏎ ⏎ ```` ``` ```` | 4 |

(In the Table row `\|` stands for a literal pipe.) A leading/trailing `\n` added by the new-line
rule moves the caret by the length of the leading one.

Invariants tested: `detect_slash` never returns Menu/DateTime for an insertion that is not exactly
one `/`; `expand_slash(...).text` re-parsed by `parse_markdown` yields the block kinds the reading
page shows for it (Table; Fence/Code/Fence; Callout + Info quote; Summary + Marker; Quote;
ListItem); a result applied twice at the same place is the caller's problem (not guarded).
`format_timestamp(2026, 10, 11, 14, 30) == u"2026-10-11 14:30"`; `days_in_month(2024, 2) == 29`,
`days_in_month(2100, 2) == 28`.

## `pbnotes/edit_history.h` (unchanged)

No change. New tests pin that `begin_group(Text)` followed by several `observe` calls and one
`end_group()` yields one step whose `removed` is empty and whose `inserted` is the whole
replacement text, and that undo restores the text from before the first `/`.
