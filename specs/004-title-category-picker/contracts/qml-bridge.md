# Contract: QML bridge and dialogs

Extends `specs/001-pocketbook-notes-mvp/contracts/qml-bridge.md`. `NotesBridge` is the only object
the editor talks to.

## NotesBridge

| Member | Change | Behavior |
|--------|--------|----------|
| `loadForEdit(localId)` | + `title` | `{title, text, category, favorite}`; `title` is the stored title as is (not display-processed) |
| `saveNote(localId, title, text, category, favorite)` | + `title` | creates (`localId <= 0`) or updates; returns the local id, or `-1` after emitting `errorOccurred(key, text)`; nothing is stored on error |
| `categoryChoices(current)` | new | `[{label, value}]` for `ChoiceDialog`: `{"No category","none"}`, `{"New category…","new"}`, then `{name,"c:"+name}` for every category of `categories()`; if `current` is non-empty and not among them it is added in its sorted place; the editor passes `current` to `ChoiceDialog` as `"none"` or `"c:"+name` |
| `checkCategory(name)` | new | trims `name` (ASCII whitespace), runs `validate_category`; returns `{ok, name, errorText}` (`name` = trimmed value, `errorText` empty when ok); an empty trimmed name is `ok:false` with the category-empty text (the picker has "No category" for that) |
| `openNote(localId).title`, `listNotes()[].title` | changed | the stored title as is; empty (legacy only) → "(untitled)"; heading markers are no longer removed |
| `categories` property | unchanged | `[{name, count}]` |

Error texts added to the bridge table: `title_empty` ("The note needs a title."), `title_too_long`
("The title is too long (at most 255 characters)."), `title_control_chars` and `title_invalid`
("The title contains characters that are not allowed."), `category_empty` ("Enter a category name.").

## TextEntryDialog.qml (new, `qml/`)

Modal full-screen item like `ConfirmDialog`/`ChoiceDialog` (z 1000, white, no animation).

| Property / signal | Meaning |
|-------------------|---------|
| `title` (string) | heading |
| `placeholder` (string) | hint in the empty field |
| `errorText` (string) | line shown under the field when non-empty |
| `open(initial)` / `close()` | show with a starting text / hide |
| `accepted(string text)` | Confirm pressed; the dialog stays open — the caller validates and calls `close()` or sets `errorText` |
| `rejected()` | Cancel pressed; the dialog closes |

One-line field, Return = Confirm, `Qt.ImhNoPredictiveText`, the on-screen keyboard is shown on open
and hidden on close; the field is moved clear of the keyboard like the editor's text.

## EditorPage.qml

- Order: buttons row, **Title** (label + one-line field, Return focuses the text), **Category**
  (label + button showing the current name or "No category"), error line, text.
- Category button opens `ChoiceDialog` fed by `categoryChoices(category)`: `none` → category "";
  `c:<name>` → that name; `new` → `TextEntryDialog`; on `accepted` call `checkCategory`, on `ok` set
  the category and close, else set `errorText`. Dismissing either dialog leaves the category as it was.
- `dirty` = text, title, category or favorite differs from the loaded values. Save calls
  `saveNote(noteId, title, text, category, favorite)`; on `-1` the page stays and shows the message.
- A new note focuses the Title field; an existing note keeps the caret at the end of the text.
