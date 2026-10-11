---

description: "Task list for the text editing tools and the title in the reader"
---

# Tasks: Text Editing Tools and Title in the Reader

**Input**: Design documents from `/specs/005-text-editing-reader-title/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: REQUIRED. The constitution makes Test-First non-negotiable (Principle I): every test task
below MUST be written and observed FAILING before the implementation task(s) it names (a test that
does not compile against the missing header or signature counts as failing). Tests for `notes_core`
run on the host without Qt, the SDK or a device (Principle V). Unit tests under `tests/unit/` are
picked up by the glob in `tests/CMakeLists.txt`; bridge tests live in `tests/bridge/` and must be
**listed by name** in the bridge loop of `tests/CMakeLists.txt` (this feature adds one:
`test_edit_bridge`).

**Organization**: Tasks are grouped by user story. Paths are relative to the repository root. This
feature builds on features 001–004; all existing tests must stay green.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on an incomplete task)
- **[Story]**: US1..US4 as in spec.md
- **[MANUAL]**: needs a person or the reader; record the result in the named file. An implementing
  agent skips these and reports them as open

## Conventions that apply to every task

- C++17; namespace `pbnotes` in the core; `notes_core` MUST NOT include Qt or `inkview.h` headers
  (`scripts/check-constitution.sh` R3). `QClipboard`/`QGuiApplication` appear only in `bridge/`.
- **No text in logs**: no log line, `qDebug`/`qWarning`, file or server field may carry note text,
  copied text, pasted text or a title (FR-033; constitution R4/R6). Event names and counts only.
- Limits and rules, quoted from the design (apply in code, not by convention):
  - Clipboard/paste: "at most 1,000,000 code points"; control characters are U+0000–U+001F except
    `\n` (U+000A) and `\t` (U+0009), U+007F and U+0080–U+009F — dropped; `\r\n`, `\r`, U+2028,
    U+2029 become `\n`; invalid UTF-8 refused (`paste_invalid`), over the limit refused
    (`paste_too_large`).
  - Title paste: every run of line breaks and tabs becomes one space; the result must not exceed
    "255 code points" (error key `title_too_long`, text "The title is too long (at most 255
    characters)."); the title is not trimmed while typing.
  - History: "max_steps = 200, max_chars = 2,000,000; the oldest steps are dropped, the newest is
    always kept"; typing merges while "same field, adjacent, ≤ 1500 ms since the previous
    observation, merged insertion ≤ 80 UTF-16 units, no `\n`"; `Category` and `Favorite` steps
    never merge; a new step clears the redo stack; offsets are UTF-16 code units and a surrogate pair
    is never split.
- UI strings go through `qsTr`. Interfaces are those of `contracts/core-library.md` and
  `contracts/qml-bridge.md`; if a task finds a contract wrong, fix the contract in the same commit.
- Device builds use the manual moc list: every new `Q_OBJECT` header must be added to
  `pbnotes_moc(...)` in `bridge/CMakeLists.txt` as well as to the host source/header lists.

---

## Phase 1: Setup

- [X] T001 Build the host target and run the whole suite on the untouched branch
  (`cmake -S . -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build-host && ctest
  --test-dir build-host --output-on-failure`). Every test must pass (including `app_self_test`); if
  one does not, stop and report it — later failures must be attributable to this feature.

---

## Phase 2: Foundational (clipboard text and the clipboard bridge — used by US1 and US4)

**Purpose**: the cleaned clipboard text and the object QML talks to. No user story can paste or
copy without it.

**Tests first (observe failing)**

- [X] T002 [P] Create `tests/unit/test_text_clean.cpp` for `pbnotes::clean_pasted_text` and
  `pbnotes::single_line` (`contracts/core-library.md`; header `pbnotes/text_clean.h` does not exist
  yet, so the test fails to compile). Cases: `"a\x01" "b"` → ok, `"ab"`; `"a\tb\nc"` unchanged;
  `"a\r\nb"`, `"a\rb"`, `"a\xE2\x80\xA8" "b"` (U+2028), `"a\xE2\x80\xA9" "b"` (U+2029) → `"a\nb"`;
  NUL (`std::string("a\0b", 3)`), DEL `\x7f`, and C1 `"\xC2\x85"` (U+0085) dropped; invalid UTF-8
  (`"\xC3("`, a lone `"\xFF"`, an overlong `"\xC0\xAF"`) → `ok == false`, `error_key ==
  "paste_invalid"`; empty input → ok, empty; exactly 1,000,000 code points (as `"ä"` ×1,000,000 —
  2,000,000 bytes) → ok; 1,000,001 → `paste_too_large`; the cap counts code points, not bytes; emoji
  and combining marks pass through unchanged. `single_line`: `"a\n\nb"` → `"a b"`, `"a\r\nb"` →
  `"a b"`, `"\tx"` → `" x"`, `"x\n"` → `"x "` (not trimmed), `"one line"` unchanged.
- [X] T003 [P] Create `tests/bridge/test_edit_bridge.cpp` for `ClipboardBridge`
  (`contracts/qml-bridge.md`; header `edit_bridge.h` does not exist yet) using a plain
  `QCoreApplication` (so the in-app buffer is used) and add `test_edit_bridge` to the bridge test
  name list in `tests/CMakeLists.txt`. Cases: a new bridge has `hasText == false` and `text() == ""`;
  `copy("hello")` → `hasText`, `text() == "hello"`, the `changed` signal fired once; `copy("")` and
  `copy("\x01")` (cleans to empty) are ignored (no signal, previous text kept); `copy` of
  `"a\xE2\x80\xA9" "b"` stores `"a\nb"`; `copy` drops control characters but keeps `\t`/`\n`; with
  no `QGuiApplication` nothing throws or crashes. (The platform-clipboard path of `QClipboard` is
  not covered by CTest; spike S-3, T007 and T041, covers it on the reader.)

**Implementation**

- [X] T004 Implement `core/include/pbnotes/text_clean.h` and `core/src/text_clean.cpp` exactly per
  `contracts/core-library.md` (rules quoted in *Conventions*; validate UTF-8 by decoding code
  points; reject overlong forms, surrogates U+D800–DFFF and values above U+10FFFF as invalid); add
  `src/text_clean.cpp` to `PBNOTES_CORE_SOURCES` in `core/CMakeLists.txt`. Make T002 pass.
- [X] T005 Implement `ClipboardBridge` in new `bridge/include/edit_bridge.h` and
  `bridge/src/edit_bridge.cpp` per `contracts/qml-bridge.md`: `Q_PROPERTY(bool hasText ... NOTIFY
  changed)`, `Q_INVOKABLE void copy(const QString&)` (normalizes U+2029/U+2028/`\r\n`/`\r` to `\n`,
  drops control characters via the same rules as `clean_pasted_text` but WITHOUT the size cap, stores
  into `QGuiApplication::clipboard()` when `QGuiApplication::instance()` is non-null, else into an
  in-app `QString`; ignores an empty result), `Q_INVOKABLE QString text() const`; connect
  `QClipboard::dataChanged` to update `hasText`. Register the class: add `src/edit_bridge.cpp` and
  `include/edit_bridge.h` to the source/header lists and to `pbnotes_moc(...)` in
  `bridge/CMakeLists.txt`; add `paste_invalid` ("The copied text cannot be pasted.") and
  `paste_too_large` ("The copied text is too large to paste.") to the text table in
  `bridge/src/notes_bridge.cpp`; create the object in `app/main.cpp` next to `about` and expose it as
  context property `clipboard`. Make T003 pass.

**Checkpoint**: clipboard text and bridge ready; `ctest` green.

---

## Phase 3: User Story 1 — Select text and copy, cut, paste or delete it (Priority: P1) 🎯 MVP

**Goal**: in the editor, select text in the title or the text and Copy, Cut, Paste, Delete, with a
Select all and press-and-hold word selection, without the keyboard closing.

**Independent Test**: type "Hello world" into the text; hold "world", Copy, tap at the start, Paste →
"worldHello world"; hold "Hello", Cut, Paste at the end; hold a word, Del; Copy from the text and
Paste into the Title (line breaks become spaces). (quickstart §4 rows 1–8; US1 scenarios 1–12.)

### Tests first (observe failing)

- [X] T006 [P] [US1] Extend `tests/bridge/test_edit_bridge.cpp` for `ClipboardBridge.cleanPaste()`
  and `titlePaste(current, selBegin, selEnd)` (`contracts/qml-bridge.md`). `cleanPaste`: clipboard
  `"a\r\nb\x01"` → `{ok:true, text:"a\nb"}`; empty clipboard → `{ok:false, errorKey:""}` (silent
  no-op); 1,000,001 code points copied → `{ok:false, errorKey:"paste_too_large", errorText:"The
  copied text is too large to paste."}`; invalid UTF-8 cannot be produced through `QString`, so
  cover `paste_invalid` in T002 only. `titlePaste`: current `"Hello"`, selection 1..3, clipboard
  `"i\nthere"` → `{ok:true, text:"Hi therelo", caret:8}`; no selection (3..3) inserts at the caret;
  clipboard `"a\tb"` → single spaces; current of 250 `a` + clipboard of 10 `b` → `{ok:false,
  errorKey:"title_too_long", errorText:"The title is too long (at most 255 characters)."}`; exactly
  255 code points → ok; 128 emoji (`"\U0001F600"`, 256 UTF-16 units but 128 code points) + 127 `a` →
  ok (code points, not UTF-16 units); selection offsets beyond the length or reversed are clamped;
  the surrogate pairs of a selection boundary are not split (an offset inside a pair moves to the
  pair's start for `selBegin` and its end for `selEnd`); the returned `caret` is a UTF-16 offset.

### Spikes (device) — before T009

- [ ] T007 [MANUAL] [US1] Spikes **S-1** and **S-3** (`quickstart.md` §2) on the InkPad: a throwaway
  page (not committed) with a `TextEdit` and a `TextField` each carrying `TapHandler {
  longPressThreshold: 0.6; onLongPressed: parent.selectWord() }`, and a button doing
  `clipboard.copy("x"); label.text = clipboard.text()`. Record in
  `specs/005-text-editing-reader-title/checklists/device-results.md`: does the held press fire and
  select the word; does the keyboard stay; does the clipboard round-trip; does `QClipboard` report
  a platform clipboard. If S-1 fails, T009 adds the *Word* button fallback (research R-3).

### Implementation

- [X] T008 [US1] Implement `Q_INVOKABLE QVariantMap cleanPaste() const` and `Q_INVOKABLE QVariantMap
  titlePaste(const QString& current, int selBegin, int selEnd) const` in
  `bridge/include/edit_bridge.h` / `bridge/src/edit_bridge.cpp`: take `text()`, UTF-8 encode, call
  `pbnotes::clean_pasted_text` (errors → key and `NotesBridge`-style text; the two paste texts are
  duplicated as constants in `edit_bridge.cpp` or fetched through a `std::function` — do not make
  `ClipboardBridge` depend on `NotesBridge`), and for the title apply `pbnotes::single_line`, build
  the result with the clamped selection, count code points (`QString::toUcs4().size()`) against
  255 and return `{ok, text, caret, errorKey, errorText}`. An empty cleaned text returns `{ok:false,
  errorKey:""}`. Make T006 pass.
- [X] T009 [P] [US1] Create `qml/EditTools.qml` per `contracts/qml-bridge.md` and register it in
  `qml/qml.qrc`: a `Row` of `InkButton`s with `focusPolicy: Qt.NoFocus`, `font.pixelSize: 28`,
  `leftPadding/rightPadding: 14`, in the order Undo, Redo, All, Extend, Copy, Cut, Paste, Del;
  properties `canCopy`, `canPaste`, `canUndo`, `canRedo`, `extendArmed`, `showHistory` (default
  `false` — hides Undo/Redo until US2); signals `undo() redo() selectAll() extend() copy() cut()
  paste() remove()`; `enabled` of Copy/Cut/Del/Extend bound to `canCopy`, Paste to `canPaste`,
  Undo/Redo to `canUndo`/`canRedo`; the Extend label is `★ Extend` while `extendArmed`. Add the
  *Word* button (`signal word()`, hidden unless `property bool showWord: false` is set) only if T007
  reported S-1 failed.
- [X] T010 [US1] Wire the tools into `qml/pages/EditorPage.qml`: insert `EditTools` in the `top`
  column directly under the button `Row`; add `property string activeField` set from
  `titleField.onActiveFocusChanged` / `area.onActiveFocusChanged` (`"title"` / `"text"`) and
  `readonly property var activeItem: activeField === "title" ? titleField : area`; add
  `property bool extendArmed`, `property var lastSelection` (`{field,start,end}`, updated whenever a
  field's `selectionStart < selectionEnd`), and `property bool hasSelection: activeItem.selectionStart
  < activeItem.selectionEnd`. Handlers: **All** → `activeItem.selectAll()`; **Copy** →
  `clipboard.copy(activeItem.selectedText)`; **Cut** → Copy then `activeItem.remove(selectionStart,
  selectionEnd)`; **Del** → `remove` only; **Paste** → for the text field `var r =
  clipboard.cleanPaste()`, for the title `var r = clipboard.titlePaste(titleField.text,
  titleField.selectionStart, titleField.selectionEnd)`; on `!r.ok` with a non-empty `errorText`
  set `page.message = r.errorText` and change nothing; on ok remove the selection if any, `insert(pos,
  r.text)` (text) or assign the title result and `cursorPosition = r.caret`, leaving the caret after
  the inserted text. **Extend**: toggles `extendArmed` (needs `lastSelection`); both fields get a
  `TapHandler` whose `onTapped` (when `extendArmed` and the field equals `lastSelection.field`)
  computes `p = positionAt(...)` and selects `last.start..p` when `p >= last.start`, otherwise
  `p..last.end`, then disarms; any
  other tools button, a tap in the other field and `Cancel`/`Save` disarm it. A `TapHandler {
  longPressThreshold: 0.6; onLongPressed: parent.selectWord() }` on each field starts a word
  selection (or the *Word* button, see T009). No button may take focus: verify
  `Qt.inputMethod.visible` is unchanged after each command. `showHistory` stays `false`. Keep
  `Component.onCompleted` and `dirty` unchanged.
- [X] T011 [US1] Host check: run `ctest --test-dir build-host --output-on-failure` (incl.
  `app_self_test`) and render the editor with the desktop app at 1080×1335 and 1440×1080 (the
  `PBNOTES_START_PAGE=pages/EditorPage.qml PBNOTES_QML_SHOT=<file>.png PBNOTES_SHOT_W/H` options in
  `app/main.cpp`, as earlier features' screenshots did). The tools row must fit on one line at both
  widths without eliding a label. Record both screenshot results in
  `specs/005-text-editing-reader-title/checklists/device-results.md` (as host results).
- [ ] T012 [MANUAL] [US1] On the reader, quickstart §4 rows 1–8 and 13. Record results in
  `specs/005-text-editing-reader-title/checklists/device-results.md`.

**Checkpoint**: US1 works alone: select, Copy/Cut/Paste/Delete in title and text; Undo/Redo are not
yet shown.

---

## Phase 4: User Story 2 — Undo and redo editing steps (Priority: P1)

**Goal**: one history over title, text, category and favorite; Undo/Redo buttons; undoing everything
clears the unsaved-changes prompt.

**Independent Test**: type "abc", paste "XY", Undo → "abc", Redo → "abcXY"; cut a word, Undo → the word
is back and selected; change category, toggle Favorite, type a word, Undo ×3 undoes them in reverse;
undo all, Cancel → no discard prompt. (quickstart §4 rows 9–12; US2 scenarios 1–11.)

### Tests first (observe failing)

- [X] T013 [P] [US2] Create `tests/unit/test_edit_history.cpp` for observation, steps and grouping
  of `pbnotes::EditHistory` (`contracts/core-library.md`; `pbnotes/edit_history.h` does not exist
  yet). Cases: inactive before `reset` (`observe` ignored, `can_undo()` false); after `reset`
  `can_undo()`/`can_redo()` false and `undo_size() == 0`; observing the same value is a no-op;
  minimal diff — `"abcdef"` → `"abXdef"` is one step with pos 2, removed `"c"`, inserted `"X"` and its
  undo is `{pos 2, remove_len 1, insert "c"}`; repeated characters `"aaa"` → `"aaaa"` is an insertion
  at pos 3; surrogate pairs — removing `"😀"` from `"a😀b"` yields a 2-unit removal (never half a
  pair), replacing `"😀"` by `"😁"` replaces the whole pair; typing grouping with an injected clock:
  five single characters 100 ms apart → `undo_size() == 1`; a gap of 1501 ms starts a new step, a gap
  of exactly 1500 ms merges; the 81st typed character starts a new step (80 merge); `\n` is a step of
  its own and the next character starts another; a non-adjacent position starts a new step; a
  different field starts a new step; backspace run (`"hello"` → `"hell"` → `"hel"` → … 100 ms
  apart) merges into one removal step with the whole removed text and its undo re-inserts it; a
  forward-delete run merges likewise; a multi-character insertion (paste) never merges with typing
  before or after it; two `Category` changes are two steps; `Favorite` toggles are two steps;
  typing 40 characters 150 ms apart is exactly one step (SC-004); caps: `EditHistory(3, …)` with
  five spaced steps keeps 3 and drops the oldest; `EditHistory(200, 10)` with steps of 6 characters
  keeps the newest even though it exceeds the cap and drops older ones; observing a single
  character in a 100,000-character value 1,000 times finishes in under 1 s.
- [X] T014 [P] [US2] Create `tests/unit/test_edit_history_undo.cpp` for undo/redo, groups and
  apply handling. Cases: `begin_group(Text)` + three intermediate `observe` calls (cut-style remove,
  then paste-style insert) + `end_group()` → exactly one step whose undo restores the value at
  `begin_group`; a group with no net change pushes no step; a second `begin_group` ends the first;
  undo order across fields — change title, text, category, favorite in that order, four `undo()`
  return them in reverse (`Favorite` value `""`/`"1"` as before) and four `redo()` in order;
  `undo()` sets applying — an `observe` before `finish_apply` is ignored — and after `finish_apply`
  with the real values an `observe` of the same value is a no-op; `finish_apply` with different
  real values re-syncs the known values and keeps both stacks; a new edit after an undo clears the
  redo stack (`can_redo()` false); undoing everything leaves `can_undo()` false and the known values
  equal to the `reset` values (the opened content is never removable); selection results — undo of a
  removal returns `sel_begin..sel_end` over the restored text, undo of an insertion returns a caret
  (equal) at `pos`, redo of an insertion a caret after the inserted text, redo of a removal a caret
  at `pos`; `reset` after use clears both stacks and any group/applying state; `undo()`/`redo()` on
  empty stacks return `nullopt`.
- [X] T015 [P] [US2] Extend `tests/bridge/test_edit_bridge.cpp` for `EditSession`
  (`contracts/qml-bridge.md`): `reset("T","body","Cat",false)`; `observe("text","bodyX")` →
  `canUndo` true and the `changed` signal fired; field names other than `title text category
  favorite` are ignored (no step); `undo()` returns `{field:"text", pos:4, removeLen:1, insert:"",
  value:"", selBegin:4, selEnd:4}`; a category change returns `{field:"category", value:"Cat"}`
  when undone; favorite undo value `""` (was off) / `"1"` (was on); `beginGroup("text")` /
  `endGroup()` around two `observe` calls is one step; positions are UTF-16 units — a text with
  `"😀"` before the edit shifts `pos` by 2; `finishApply(title, text)` re-enables `observe`;
  `setClockForTest` drives grouping (two single characters 2,000 ms apart are two steps); an empty
  map is returned when there is nothing to undo; `reset` clears `canUndo`/`canRedo`.

### Implementation

- [X] T016 [US2] Implement `core/include/pbnotes/edit_history.h` and `core/src/edit_history.cpp`
  per `contracts/core-library.md` and `data-model.md` (EditStep fields, merge rules and caps quoted
  in *Conventions*, `EditApplied` selection results); add `src/edit_history.cpp` to
  `PBNOTES_CORE_SOURCES` in `core/CMakeLists.txt`. Diff = longest common prefix (then suffix, not
  overlapping), moved off any surrogate-pair boundary. Make T013 and T014 pass.
- [X] T017 [US2] Implement `EditSession` in `bridge/include/edit_bridge.h` /
  `bridge/src/edit_bridge.cpp` per `contracts/qml-bridge.md` (wraps `pbnotes::EditHistory`;
  `QString` ↔ `std::u16string` via `utf16()`; clock from a `QElapsedTimer` with
  `setClockForTest(std::function<qint64()>)`; `canUndo`/`canRedo` properties with a `changed`
  notify emitted after every state change); add the class to `pbnotes_moc(...)` (already listed from
  T005) and create it in `app/main.cpp` as context property `editSession`. Make T015 pass.
- [X] T018 [US2] Wire the history into `qml/pages/EditorPage.qml`: after the initial
  `titleField.text = …; area.text = …` assignments in `Component.onCompleted` call
  `editSession.reset(titleField.text, area.text, category, favorite)` (those assignments must not
  record); add `onTextChanged: editSession.observe("title", text)` on `titleField` and `…("text",
  text)` on `area`; call `editSession.observe("category", page.category)` and
  `observe("favorite", page.favorite ? "1" : "")` from `onCategoryChanged` / `onFavoriteChanged`
  (guarded so the initial assignment is not recorded); wrap Cut, Del and the replace-selection part
  of Paste in `editSession.beginGroup(activeField)` / `endGroup()`; set `showHistory: true` on
  `EditTools` and bind `canUndo`/`canRedo` to `editSession.canUndo/canRedo`. Add `function
  applyHistory(step)` : for `title`/`text` do `item.remove(step.pos, step.pos + step.removeLen)`,
  `item.insert(step.pos, step.insert)`, then `item.select(step.selBegin, step.selEnd)` (equal values
  = caret via `cursorPosition`); for `category` set `page.category = step.value`; for `favorite` set
  `page.favorite = step.value === "1"`; then `editSession.finishApply(titleField.text, area.text)`;
  Undo/Redo call `var s = editSession.undo()/redo(); if (s.field) applyHistory(s)`. The attachment
  `insertToken` needs no change (one `insert` = one step, FR-016). `dirty` stays as is — verify by
  reading it that undoing everything makes it false (FR-017).
- [X] T019 [US2] Run `ctest` in full and `app_self_test`; all green. (Added during implementation: the
  headless page test `tests/qml/` — `test_qml_pages` loads the real `qml/pages/EditorPage.qml` offscreen
  with stub bridge objects and runs `tests/qml/editor_flow.qml`, registered as CTest `qml_editor_flow`;
  it checks select/Copy/Cut/Paste/Delete/Extend, the title paste rules and Undo/Redo over all four
  fields. The reading-page scenarios of US3/US4 reuse it.) Add to
  `tests/bridge/test_edit_bridge.cpp` a scenario that replays quickstart row 10 through
  `EditSession` (category, favorite, word → three undos in reverse; redo ×3) if T015 does not
  already cover it.
- [ ] T020 [MANUAL] [US2] On the reader, quickstart §4 rows 9–12. Record results in
  `specs/005-text-editing-reader-title/checklists/device-results.md`.

**Checkpoint**: US1 + US2 work in the editor.

---

## Phase 5: User Story 3 — See the title when reading a note (Priority: P2)

**Goal**: the reading page shows the stored title above the text, once, bold and larger, hidden when
empty, text untouched.

**Independent Test**: open a note titled "Groceries" whose text begins with a different line: the
title is above the text; a note whose first line repeats the title shows both; a 255-character
title wraps in full. (quickstart §4 rows 14–18; US3 scenarios 1–7.)

### Tests first (observe failing)

- [X] T021 [P] [US3] Extend `tests/bridge/test_notes_bridge.cpp` for `openNote`
  (`contracts/qml-bridge.md`): a note with title `"Groceries"` → `hasTitle == true`, `titleText ==
  "Groceries"`, `title == "Groceries"`; a title of 255 characters is returned whole (no elision);
  a stored title of `""` and of `"   "` (legacy rows; create them through the store) → `hasTitle ==
  false`, `titleText` as stored, `title == "(untitled)"` and `listNotes()` still shows
  `(untitled)`; the title with a leading `#` is returned as stored (`"# Plan"`), not stripped;
  `blocks` is unchanged for a note whose text starts with the same line as the title (the text is
  not de-duplicated, FR-022).

### Implementation

- [X] T022 [US3] In `bridge/src/notes_bridge.cpp` `NotesBridge::openNote` add `hasTitle` (title is
  non-empty after trimming ASCII whitespace) and `titleText` (the stored title as is); keep `title`
  as `display_title(...)`. Make T021 pass; update the `openNote` row in the bridge's header comment.
- [X] T023 [P] [US3] Create `qml/TitleBlock.qml` and register it in `qml/qml.qrc`: an `Item` with
  `property string title`, `property int fontPx`, a wrapping `Text` (bold, `font.pixelSize:
  Math.round(fontPx * 1.7)`, `wrapMode: Text.Wrap`, `color: "#000000"`, **no `elide`**), height =
  text height + 24 px bottom gap, width bound by the caller. (US4 replaces the `Text` by a
  selectable item in T041; keep the root properties.)
- [X] T024 [US3] In `qml/pages/ReaderPage.qml` insert, as the first child of the `body` `Column`
  (before the `Repeater`), `TitleBlock { visible: page.note.hasTitle === true; height: visible ?
  implicitHeight : 0; title: page.note.titleText; fontPx: bridge.fontPx; width: body.width }` so an
  empty title leaves no gap (FR-024); it scrolls with the content and follows `bridge.fontPx`
  (FR-021). Do not change the text blocks. Check that `refreshUnsplittable()` still uses block
  positions inside `body` (its `body.y + it.y` arithmetic is unaffected).
- [X] T025 [US3] Host check: `ctest` green; render the reading page (`PBNOTES_START_PAGE=pages/
  ReaderPage.qml PBNOTES_START_NOTE=<id>`) for a note with a short, a 255-character and an empty
  title at font sizes small/large; the long title wraps in full and the empty one leaves no gap.
  Note the result in `specs/005-text-editing-reader-title/checklists/device-results.md`.
- [ ] T026 [MANUAL] [US3] On the reader, quickstart §4 rows 14–18. Record results in
  `specs/005-text-editing-reader-title/checklists/device-results.md`.

**Checkpoint**: US3 works alone.

---

## Phase 6: User Story 4 — Select and copy text while reading (Priority: P2)

**Goal**: on the reading page select text (word hold, Extend, All) and Copy the note's **source
text** to the clipboard; the page stays read-only.

**Independent Test**: on the reading page hold a word, tap Copy, open another note's editor, tap Paste →
the word is inserted and the first note is unchanged; a fully selected heading/bold phrase/task copies
its marks, two words inside a bold phrase copy none. (quickstart §4 rows 19–27; US4 scenarios 1–10.)

### Tests first (observe failing)

- [X] T027 [P] [US4] Create `tests/unit/test_markdown_source_map.cpp` for the parser's additive
  source fields (`contracts/core-library.md`, `data-model.md` invariants). Helper: for each parsed
  block, the source line is `split` of the note text (CR removed). Cases (all assert `src.size() ==
  text.size()`, `src` non-decreasing, each `src[i]` inside `[el_begin, el_end)` of its span, and that
  the block extent is a valid slice of the line): `"plain text"` → `src == 0..9`; `"# Heading"` →
  `src_begin 0`, `src_end 9`, text `"Heading"` with `src` starting at 2; `"- item"`, `"  - [ ] task"`
  and `"1. one"` (prefix and indentation inside the block extent, text bytes after them); `"**bold**
  text"` → bold span bytes `src {2,3,4,5}`, `el 0..8`, plain span `" text"` `src` 8..12 with its own
  extent; `"*it*"`, `"***bi***"`, `"_it_"`; `"a [label](https://example.org/x) b"` → link span
  `"label"` `src` 3..7, `el 2..31`; `"<https://example.org/a>"` autolink → text is the address, `src`
  starting at 1, `el 0..23`; a bare address `"see https://example.org/a b"`; escapes
  `"\\*star\\*"` → text `"*star*"` with `src` of each escaped character = the backslash's index
  (`{0,2,3,4,5,6}`); inline code `` "`a*b*`" `` kept verbatim (backticks included, identity `src`);
  nested `"**bold [l](https://example.org) x**"` (outer emphasis wins, link kept: the extents of
  all its spans equal the outer element's `0..len`); multibyte `"ä **ö**"` (`ä` is two bytes: `src
  {0,1}`) ; text after an attachment reference `"see ![n](pbatt:3) after"` → paragraph `"see"`, an
  `Attachment` block whose extent is exactly `![n](pbatt:3)`, then paragraph `"after"` whose `src`
  starts at 18 (the trim of the segment is accounted for); a code fence line and a line inside a
  fence (literal spans, identity `src`); a line longer than 16,384 bytes (literal); a `Table` block
  and a `Blank` block (extents untouched, no crash); the whole existing parser behavior unchanged
  (the existing `tests/unit/test_markdown*.cpp` must still pass without edits).
- [X] T028 [P] [US4] Create `tests/unit/test_copy_source.cpp` for `pbnotes::copy_source`
  (`contracts/core-library.md` rules 1–5). Helper `copy(text, fromBlock, fromByte, toBlock, toByte,
  fromWhole=false, toWhole=false)` over `parse_markdown(text)` blocks. Cases: `"hello world"` bytes
  [6,11) → `"world"`; `"**bold text** x"`: the bytes of `"bold"` → `"bold"` (partly selected, no
  marks); all of `"bold text"` → `"**bold text**"`; `"bold text x"` (bold completely plus part of
  the plain span) → `"**bold text** x"`; `"**one two three**"` selecting `"two"` → `"two"`; `"# Title"`
  selecting `"Tit"` → `"Tit"`, whole (block 0, bytes 0..5, i.e. every byte) → `"# Title"`; `"-
  item"` whole → `"- item"`; `"  - [ ] task"` whole → `"  - [ ] task"`; `"- [x] done"` whole;
  a link `"[label](https://example.org/x)"` all of `"label"` → `"[label](https://example.org/x)"`,
  `"abel"` → `"abel"`; `"<https://example.org/a>"` all → the autolink as written; three blocks —
  `"alpha beta\n# Mid\n- gamma"` from `"beta"` (block 0) to `"gam"` (block 2) → `"beta\n# Mid\n-
  gam"` (partial first block without prefix, whole middle block, last block with its prefix up to
  the last selected byte); a blank block inside → an empty line (`"a\n\nb"`); a reversed range →
  `""`; table row 0 and a data row as whole units → their exact source lines (header `| a | b |`
  is `line_index`, a data row is `line_index + row + 1`); an `Attachment` block whole →
  `![n](.attachments.4/pic.png)` as written; escapes: `"\\*lit\\*"` selecting the characters
  `"*lit"` → `"\\*lit"` (the backslash of the first escape is included), `"lit"` → `"lit"`; a
  multibyte note (`"ä **ö** ü"`) with a byte offset inside `ä` moves to the next boundary; a note
  with CRLF line ends joins lines with `"\n"`; text after an attachment reference on the same line;
  offsets beyond the block are clamped; a selection of one whole block returns the line extent
  including indentation.
- [X] T029 [P] [US4] Extend `tests/bridge/test_reader_blocks.cpp`: every entry of `reader_blocks(note)`
  carries integer `block` (index into `parse_markdown(note.content)`), `row` (0 for non-table) and
  `src` (`{begin, end}`); for `"# T\n\ntext\n| a | b |\n|---|---|\n| 1 | 2 |\n![x](.attachments.1/y.png)"`
  the heading is `block 0`, the blank `block 1`, the paragraph `block 2`, the table rows share one
  `block` with `row` 0 and 1, the attachment has its own `block`; existing assertions unchanged.
- [X] T030 [P] [US4] Extend `tests/bridge/test_notes_bridge.cpp` for `readerCopy(localId, fromUnit,
  fromOffset, toUnit, toOffset)`: for a note titled `"Plan"` with text `"# Head\nsome **bold**
  words"`: unit 0 is the title — offsets 0..4 of unit 0 → `"Plan"`; unit 0 offset 0 to unit 1 (the
  heading) offset 4 (whole title through the 4th UTF-16 unit of `Head`) → `"Plan\n# Head"` when
  the heading is entered from the previous unit; units/offsets are UTF-16 (an emoji before the
  selection shifts by 2 units); a note without a title has no title unit (unit 0 is the first
  block); a range inside the bold phrase returns the characters only; reversed or out-of-range
  units return `""`; table rows and attachments (whole units) return their lines; **the call changes
  nothing**: the note's title, content, category, favorite, `modified`, `status` and the list order
  are identical before and after (SC-010) and `pendingCount` is unchanged.

### Spike (device) — before T037

- [ ] T031 [MANUAL] [US4] Spike **S-2** (`quickstart.md` §2): on the InkPad render one note with a
  heading, bold/italic, a link, a task list and a table with `Text` and with a read-only `TextEdit`
  (`textFormat: TextEdit.RichText`, same `html`); compare line breaks and heights, test a link tap
  and confirm that tapping does not open the keyboard. Record in
  `specs/005-text-editing-reader-title/checklists/device-results.md`. If parity fails, STOP T037–T041
  and ask the author whether partial selection is limited to the editor (research R-7 fallback:
  whole-block selection) before continuing.

### Implementation

- [X] T032 [US4] Record source offsets in the parser (`contracts/core-library.md`): add
  `Span::src`, `Span::el_begin`, `Span::el_end` and `Block::src_begin`, `Block::src_end` to
  `core/include/pbnotes/markdown.h`; extend `md::add_span` in `core/src/markdown_internal.h` /
  `core/src/markdown_inline.cpp` with optional per-byte offsets and element extent (merging adjacent
  spans merges `src` and unions the extent); give `parse_inline` a `base` offset (offset of the
  parsed substring inside the source line) and track the origin index of every byte appended to
  `literal` (an escaped character gets the backslash's index); nested calls (`parse_inline(label…)`,
  emphasis inner text) pass `base + offset`; in `core/src/markdown.cpp` pass the offsets of
  `rest` in the line, of `body` in `rest` (`hashes + 1`, list marker + task box) and of each
  `md::Segment` in `body` (add `size_t begin` to `md::Segment`, set in `split_attachment_refs`),
  plus the leading bytes removed by `trimmed(seg.text)`; set `Block::src_begin/src_end` for every
  text block (the whole line extent from column 0 up to the end of its segment), for Attachment blocks
  (the reference as written) and for literal/fence lines. Make T027 pass; all existing
  `tests/unit/test_markdown*.cpp` and `tests/bridge/test_reader_blocks.cpp` stay green.
- [X] T033 [US4] Implement `pbnotes::copy_source` in new `core/src/markdown_copy.cpp` (declared in
  `core/include/pbnotes/markdown.h` with `SourcePoint`), following rules 1–5 of
  `contracts/core-library.md`; add the file to `PBNOTES_CORE_SOURCES`. Use the same line splitting as
  the parser (CR removed). Make T028 pass.
- [X] T034 [US4] In `bridge/src/reader_blocks.cpp` add `block`, `row` and `src` to every entry
  (`append_table_rows` sets `row` per row and the shared `block`); the `parse_markdown` loop passes
  the block index through. Make T029 pass.
- [X] T035 [US4] Add `Q_INVOKABLE QString readerCopy(qlonglong localId, int fromUnit, int fromOffset,
  int toUnit, int toOffset) const` to `bridge/include/notes_bridge.h` / `bridge/src/notes_bridge.cpp`
  (const; reads the note only): normalize `from ≤ to`, map units (title first when the note has a
  title, then one per `reader_blocks` entry), convert UTF-16 offsets to byte offsets in a block's
  displayed text with `QString::fromUtf8(plain).left(off).toUtf8().size()`, mark table rows,
  attachments and blank lines as whole units, call `pbnotes::copy_source`, and put the title's
  characters (and a `\n` before the following text) in front when the range starts in the title
  unit. Make T030 pass.
- [X] T036 [P] [US4] Update the whole-unit views: add `property bool selected: false` and a 4 px black
  frame (`border.width: selected ? 4 : 0`) to `qml/TableRowView.qml` and `qml/AttachmentView.qml`;
  blank lines are handled in T037.
- [X] T037 [US4] Rework `qml/BlockText.qml`: replace the `Text` that shows `block.html` by a
  `TextEdit { readOnly: true; textFormat: TextEdit.RichText; selectionColor: "#000000";
  selectedTextColor: "#ffffff"; activeFocusOnPress: false; selectByMouse: false }` with the same
  font settings, wrap and link behavior (`onLinkActivated` → `root.linkActivated`); add properties
  `selFrom`/`selTo` (UTF-16 offsets, −1 = none; an `onSelFromChanged`/`onSelToChanged` calls
  `select(selFrom, selTo)` or `deselect()`), a read-only `textLength` (`length`), signals
  `wordHeld(int start, int end)` (a `TapHandler { longPressThreshold: 0.6 }` that calls
  `positionAt` + `selectWord()` and emits the selected range) and `tapped(int pos)` (a tap that is
  not on a link); a `selected` bool for blank blocks draws the 4 px frame. Keep the task box
  `MouseArea` and the `taskToggled` signal as is. Only after S-2 (T031) passed.
- [X] T038 [US4] Make `qml/TitleBlock.qml` selectable the same way (read-only `TextEdit`, bold,
  `fontPx × 1.7`, same signals/properties as `BlockText`, no elide).
- [X] T039 [US4] Build the selection model into `qml/pages/ReaderPage.qml`: units = the title (if
  `note.hasTitle`) then `note.blocks`; state `selection` (`{fromUnit, fromOffset, toUnit, toOffset}`,
  `null` when none), `anchor` (the held word `{unit, start, end}`), `extendArmed`; a function
  `normalize()` ordering `from ≤ to`; each unit gets `selFrom`/`selTo` (first unit `[fromOffset,
  length]`, middle units `[0, length]` or `selected: true`, last `[0, toOffset]`; whole-unit kinds
  set `selected`); `onWordHeld(start, end)` sets selection and anchor; when `extendArmed`, the next
  `onTapped(pos)` on a text unit (or a tap on a whole unit) selects from the anchor to that point
  (to the unit for whole units) and disarms; a tap outside the selection that is neither armed nor
  on a link/task/attachment clears it; edge `MouseArea`s and `PageButtons` never touch it
  (FR-030); `load()` and `bridge.fontSize` changes clear it (`Connections` on `bridge`
  `onFontSizeChanged`). Footer row beside `PageButtons`: `InkButton`s **All** (selection from the
  first to the last unit), **Extend** (armed state shown as `★ Extend`, enabled with a selection),
  **Copy** (enabled with a selection; `clipboard.copy(bridge.readerCopy(noteId, …))`), all with
  `focusPolicy: Qt.NoFocus`. The page must not offer Cut, Paste, Delete, Undo or Redo (FR-027) and
  never write to the note. With no selection a tap on a link/task/attachment behaves as before;
  with one, the first tap only clears the selection (US4-7).
- [X] T040 [US4] Host check: `ctest` green including `app_self_test`; render the reading page at
  1080×1335 and 1440×1080 with a selection state set through a temporary debug property or the
  desktop mouse; the footer fits on one line next to the page buttons. Record the result in
  `specs/005-text-editing-reader-title/checklists/device-results.md`.
- [ ] T041 [MANUAL] [US4] On the reader, quickstart §4 rows 19–27 (row 27 settles spike S-3's
  delivered behavior). Record results in `specs/005-text-editing-reader-title/checklists/
  device-results.md`.

**Checkpoint**: all four stories work independently and together.

---

## Phase 7: Polish & cross-cutting concerns

- [X] T042 [P] Update the documents the feature extends: `README.md` (editor tools, undo/redo,
  reader title and reader copy; the clipboard is in-app unless S-3 shows sharing); in
  `specs/001-pocketbook-notes-mvp/spec.md` add a note under the reader requirement that the reader
  page now starts with the title block (feature 005); update the comment at the top of
  `qml/pages/ReaderPage.qml` and `qml/pages/EditorPage.qml` to mention the new tools and selection;
  in `specs/005-text-editing-reader-title/plan.md` Constitution Check row IV change "tested with an
  offscreen platform when present" to "the platform clipboard path is exercised on the reader (S-3)".
- [X] T043 Run `scripts/pre-merge.sh`: all tests, constitution checks (R3 core is Qt-free, R4 no
  titles/text in log calls, R5 no absolute paths, R6 no forbidden log keys), fixture anonymization
  and the secret scan green. Verify with `grep -rn "qDebug\|qWarning\|std::cerr" bridge/src/edit_bridge.cpp`
  that no text reaches a log.
- [X] T044 Release 1.2.0 (constitution 1.3.0 requires a release task): raise `VERSION` to `1.2.0` and
  add a `## 1.2.0` entry at the top of `CHANGELOG.md` (editor: select, Copy/Cut/Paste/Delete, Undo/
  Redo over title, text, category and favorite; reading page: title block, select and Copy of the
  note's source text; clipboard in the app). Run `scripts/check-version-bump.sh` as
  `scripts/pre-merge.sh` does. Tag `v1.2.0` after the merge.
- [ ] T045 [MANUAL] Full quickstart §4 on the reader with the release package (all 27 rows), including
  landscape and a note with the 100,000-character case (SC-006: each command shows its result within
  1 second). Record in `specs/005-text-editing-reader-title/checklists/device-results.md` and
  update `research.md` R-3/R-4/R-7 with the spike results.
- [X] T046 Update `specs/005-text-editing-reader-title/checklists/requirements.md` notes: every
  FR-001..FR-033 is covered by a task above (map: FR-001..FR-010 → T002–T012; FR-011..FR-018 →
  T013–T020; FR-019..FR-025 → T021–T026; FR-026..FR-031 → T027–T041; FR-032/FR-033 → T002–T008,
  T043); list the open [MANUAL] tasks.

---

## Phase 8: Rework after the first device test (long-press menu and markers)

The author's first device test (2026-10-11): too many buttons in the editor, selection did not work at all. New
interaction (spec Clarifications, FR-003/006/010/011/027/034): header with Cancel, Save, Favorite, Attach, Delete only;
a long press on a word selects it, shows two draggable markers and a bubble menu; the reading page works the same way.

- [X] T047 Replace `TapHandler` gestures by `MouseArea` (`pressAndHold`, `clicked`) in `qml/pages/EditorPage.qml`,
  `qml/SelectableText.qml`, `qml/BlockText.qml` and the reader's whole-unit delegates (mouse events are what the
  reader's touch screen is known to deliver; `TapHandler` was the likely reason selection did not work).
- [X] T048 [P] Create `qml/SelectionHandle.qml` (a black square with three white lines hanging below the marked line, a
  stem up to it; the start marker hangs left, the end marker right; drag reports the text point under it),
  `qml/SelectionHandles.qml` (the two markers of a TextEdit/TextInput following `selectionStart/End`, moving them when
  dragged) and `qml/SelectionMenu.qml` (a bubble with a thick border and text buttons that never take focus; above the
  selection, below it when there is no room, kept above the keyboard); register them in `qml/qml.qrc`; delete
  `qml/EditTools.qml`.
- [X] T049 Editor: remove the tools row; long press on a word selects it and opens the menu (Cut, Copy, Paste, Remove,
  Select all, Undo, Redo — only those that apply); long press below the text opens it without a selection; a tap places
  the caret and closes it; markers drag the ends; Copy ends the selection; Select all keeps the menu.
- [X] T050 Reading page: footer back to the page buttons only; long press selects a word (or a whole table row,
  attachment, blank line) and opens a bubble with Copy and Select all; two markers in the flickable's content move the
  ends across blocks; a tap outside, a font-size change and a reload end the selection; a page turn closes the bubble
  but keeps the selection (a marker tap reopens it).
- [X] T051 Tests: `tests/qml/test_qml_pages.cpp` injects real mouse events (`QTest`), `tests/qml/editor_flow.qml` and
  `tests/qml/reader_select.qml` are rewritten around the gestures (header buttons, tap vs long press, menu items, Copy,
  Cut, Remove, Paste, Undo, markers dragged, tap outside, Select all, the title field, whole units, links).
- [X] T052 Spec, research, plan, contract, README and CHANGELOG updated for the new interaction.
- [X] T053 [MANUAL] (informal: the author tried it on the reader and approved, see device-results.md) Device check of the new interaction (quickstart §4, rows 1–8, 19–25 as reworded): long press on the
  reader, marker dragging with a finger, bubble placement above the keyboard, rotation. Record in
  `specs/005-text-editing-reader-title/checklists/device-results.md`.

---

## Dependencies & Execution Order

- Phase 1 (T001) first. Phase 2: T002 ‖ T003 (tests, parallel) → T004 (needs T002) and T005
  (needs T003 and the `ClipboardBridge` part only; T005 does not need T004 to compile but its
  cleaning must match the same rules — do T004 first).
- US1 (Phase 3) needs Phase 2. T006 (test) → T008; T007 (spike) before T010; T009 can run any time
  after T005; T010 needs T008, T009 and T007's result; T011, T012 last.
- US2 (Phase 4) needs US1's T010 (the tools row and `activeField` exist). Tests T013 ‖ T014 ‖ T015
  are independent files; T016 needs T013 and T014; T017 needs T015 and T016; T018 needs T017 and
  edits `EditorPage.qml` after T010 (sequential on that file).
- US3 (Phase 5) is independent of US1/US2 (different files: `notes_bridge`, `TitleBlock.qml`,
  `ReaderPage.qml`); it may start after Phase 1. T021 → T022; T023 ‖ T022; T024 needs T022 and
  T023.
- US4 (Phase 6) needs Phase 2 (`ClipboardBridge`) and US3's T023/T024 (the title unit). Tests T027 ‖
  T028 ‖ T029 ‖ T030 are independent; T032 needs T027; T033 needs T028 and T032; T034 needs T029;
  T035 needs T030, T033, T034; T031 (spike) gates T037–T041; T036 ‖ T037 ‖ T038 touch different
  files; T039 edits `ReaderPage.qml` after T024 and needs T035–T038.
- Polish needs the stories it documents or runs. T044 (version bump) comes last before the merge.

### Parallel opportunities

- Phase 2: T002 ‖ T003.
- US2 tests: T013 ‖ T014 ‖ T015. US3 may run in parallel with US1/US2 on a separate branch of work.
- US4 tests: T027 ‖ T028 ‖ T029 ‖ T030; implementation T036 ‖ T037 ‖ T038.
- Polish: T042 alongside T043 once the code is done.

## Implementation Strategy

- **MVP = Phase 1 + Phase 2 + US1** (T001–T012): select, Copy/Cut/Paste/Delete work in the editor.
  Ship-ready on its own; Undo/Redo follow immediately as US2 because Cut/Delete are destructive
  (the spec ranks them both P1).
- Then US2, US3 (small, independent), and last US4 (the largest: parser offsets, `TextEdit` in the
  reader, gated by spike S-2).
- Every code task is preceded by its failing test; the [MANUAL] tasks are the only device steps and
  are reported as open by an implementing agent.
