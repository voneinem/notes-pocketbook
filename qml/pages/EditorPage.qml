import QtQuick
import QtQuick.Controls
import ".."

// Plain-text editor for one note (noteId <= 0 creates a new one). Nothing is saved automatically:
// "Save" writes the note (offline, it waits for the next sync); leaving with unsaved changes asks first.
// The title and the category are single lines above the text. The title is what the reader types in its own
// field; it is never taken from the text. The category is chosen from a list (existing categories, none).
// Editing tools (spec 005) live in a context menu, like the reader's own: a long press on a word selects it and
// shows two markers to drag and a bubble with Cut, Copy, Paste, Remove, Select all, Undo and Redo; a long press on
// empty text offers Paste, Select all, Undo and Redo. The header keeps only Cancel, Save, Favorite, Attach, Delete.
// Undo and Redo step back and forth over every change to the title, the text, the category and the favorite marker.
// Typing "/" in the text (at the start of a line or after a space) opens a list of formats next to the caret; choosing
// one inserts its Markdown (spec 006). The insertion, with the typed "/", is one undo step.
Item {
    id: page
    property int noteId: -1
    property string startTitle: ""
    property string startText: ""
    property string startCategory: ""
    property string category: ""
    property bool startFavorite: false
    property bool favorite: false
    property string message: ""
    property bool historyReady: false   // set once the opened values are in the fields and in editSession

    // ---- the "/" menu (spec 006) ----
    property string previousText: ""    // the text area's text before its latest change (input of slash.detect)
    property bool programmatic: false   // the editor itself is changing the text (paste, undo, token, menu insertion)
    property var slashState: null       // {pos, kind}: the "/" a popup belongs to; kind "menu", "datetime" or "emoji"; null = none
    property var now: null              // the clock of the date-time picker; tests set a date, null = the real time
    property bool keyboardWasShown: false
    function clock() { return now ? now : new Date() }

    // ---- selection tools (spec 005) ----
    property string activeField: "text"
    readonly property var activeItem: activeField === "title" ? titleField : area
    readonly property bool hasSelection: activeItem.selectionStart < activeItem.selectionEnd

    readonly property bool dirty: titleField.text !== startTitle || area.text !== startText
                                  || category !== startCategory || favorite !== startFavorite
    readonly property real keyboardHeight: Qt.inputMethod.visible ? Qt.inputMethod.keyboardRectangle.height * Screen.devicePixelRatio : 0

    // An open editor keeps the automatic "Auto Sync" from starting under it (spec 008).
    Component.onDestruction: bridge.setEditorOpen(false)

    Component.onCompleted: {
        bridge.setEditorOpen(true)
        if (noteId > 0) {
            var n = bridge.loadForEdit(noteId)
            if (n.text !== undefined) {
                startTitle = n.title
                startText = n.text
                startCategory = n.category
                startFavorite = n.favorite
            }
        }
        titleField.text = startTitle
        area.text = startText
        category = startCategory
        favorite = startFavorite
        editSession.reset(titleField.text, area.text, category, favorite)
        historyReady = true
        if (noteId > 0) {
            area.forceActiveFocus()
            area.cursorPosition = area.length
        } else {
            titleField.forceActiveFocus()   // a new note starts with its title
        }
        Qt.inputMethod.show()
    }

    onWidthChanged: closeSlash()
    onHeightChanged: closeSlash()
    onCategoryChanged: if (historyReady) editSession.observe("category", category)
    onFavoriteChanged: if (historyReady) editSession.observe("favorite", favorite ? "1" : "")

    Connections {
        target: bridge
        function onErrorOccurred(messageKey, text) { page.message = text }
    }

    function leave() {
        Qt.inputMethod.hide()
        attachments.discardStaged()  // files added but not saved with the note go away (saved ones are bound)
        page.StackView.view.pop()
    }

    // ---- the "/" menu: detection, popups, insertion (spec 006, contracts/qml-bridge.md) ----
    // Called for every change of the text. A typed "/" at the start of a line or after a blank opens the menu; the
    // history group is opened *before* the "/" is reported, so the "/" and the later replacement become one undo step.
    // Any other change while a popup is open closes it first (the "/" then stays as an ordinary step).
    function areaChanged() {
        if (menu) menu.close()
        var current = area.text
        var d = null
        if (!programmatic && historyReady)
            d = slash.detect(previousText, current, area.cursorPosition, slashState ? slashState.pos : -1)
        if (d && d.trigger === "menu") {
            closeSlash()
            editSession.beginGroup("text")
            editSession.observe("text", current)
            openSlashMenu(d.pos)
        } else if (d && d.trigger === "datetime") {  // a second "/" right behind the one that opened the menu
            editSession.observe("text", current)
            openDateTime()
        } else {
            if (slashState && !programmatic) closeSlash()
            if (historyReady) editSession.observe("text", current)
        }
        previousText = current
    }
    // Typing moves the caret too, and the caret and the text change in no fixed order, so a moved caret is judged a
    // moment later, when the text change (if any) has been seen: a caret anywhere but right behind the typed slash(es)
    // closes the popup.
    function cursorMoved() { Qt.callLater(cursorSettled) }
    function cursorSettled() {
        if (slashState && !programmatic && area.cursorPosition !== slashState.pos + (slashState.kind === "datetime" ? 2 : 1))
            closeSlash()
    }
    function openSlashMenu(pos) {
        slashState = { pos: pos, kind: "menu" }
        var r = area.positionToRectangle(pos)
        var p = area.mapToItem(page, r.x, r.y)
        slashMenu.anchorX = p.x
        slashMenu.caretTop = p.y
        slashMenu.caretBottom = p.y + r.height
        slashMenu.minY = topBar.height + 10
        slashMenu.maxY = bottomPanel.y
        slashMenu.open()
    }
    // Ends whatever popup is open and its history group; the typed text stays. Does nothing when no popup is open.
    function closeSlash() {
        if (slashState === null) return
        slashState = null
        hideSlashPopups()
        editSession.endGroup()
    }
    // Hides the menu and the pickers; the keyboard comes back if a picker had hidden it.
    function hideSlashPopups() {
        slashMenu.close()
        dateTimePicker.close()
        emojiPicker.close()
        if (keyboardWasShown) { keyboardWasShown = false; Qt.inputMethod.show() }
    }
    // The pickers are used without the keyboard (their buttons need the room).
    function hideKeyboardForPicker() {
        keyboardWasShown = Qt.inputMethod.visible
        Qt.inputMethod.hide()
    }
    function openDateTime() {
        slashMenu.close()
        slashState = { pos: slashState.pos, kind: "datetime" }
        hideKeyboardForPicker()
        dateTimePicker.minY = 20
        dateTimePicker.maxY = page.height
        dateTimePicker.open(clock())
    }
    function openEmoji() {
        slashMenu.close()
        slashState = { pos: slashState.pos, kind: "emoji" }
        hideKeyboardForPicker()
        emojiPicker.minY = 20
        emojiPicker.maxY = page.height
        emojiPicker.open()
    }
    function backHandled() {
        if (slashState === null) return false
        closeSlash()
        return true
    }
    function slashChosen(id) {
        if (slashState === null) return
        if (id === "emoji") { openEmoji(); return }
        var e = slash.expand(id, area.text, slashState.pos, 1)
        if (e.text === undefined) { closeSlash(); return }
        applySlashEdit(e)
    }
    // Replaces the typed "/" (or "//") by the edit's text and ends the history group: one undo step.
    function applySlashEdit(e) {
        programmatic = true
        area.remove(e.begin, e.end)
        area.insert(e.begin, e.text)
        area.cursorPosition = e.caret
        programmatic = false
        previousText = area.text
        editSession.observe("text", area.text)
        slashState = null
        hideSlashPopups()
        editSession.endGroup()
        area.forceActiveFocus()
    }

    // ---- attachments: pick a file, copy it into the app, put its token on its own line at the cursor ----
    function openPicker() {
        page.message = ""
        Qt.inputMethod.hide()
        var picker = page.StackView.view.push(Qt.resolvedUrl("FilePickerPage.qml"))
        if (picker) picker.picked.connect(function(filePath) { attachments.stageAttachment(filePath) })
    }
    function insertToken(token) {
        var t = area.text
        var p = area.cursorPosition
        var lineStart = p === 0 ? 0 : t.lastIndexOf("\n", p - 1) + 1
        closeSlash()
        programmatic = true
        area.insert(lineStart, token + "\n")
        area.cursorPosition = lineStart + token.length + 1
        programmatic = false
        area.forceActiveFocus()
    }
    Connections {
        target: attachments
        function onStageFinished(result) {
            if (result.ok) page.insertToken(result.token)
            else page.message = result.errorText
        }
    }
    function save() {
        page.message = ""
        var id = bridge.saveNote(noteId, titleField.text, area.text, category, favorite)
        if (id > 0) leave()
    }
    // ---- category: one tap in a list; no typing for an existing category ----
    function openCategoryPicker() {
        page.message = ""
        categoryDialog.choices = bridge.categoryChoices(page.category)
        categoryDialog.current = page.category === "" ? "none" : "c:" + page.category
        categoryDialog.open()
    }
    function categoryChosen(value) {
        if (value === "none") page.category = ""
        else if (value.indexOf("c:") === 0) page.category = value.substring(2)
        else if (value === "new") { newCategoryDialog.open(""); return }
        area.forceActiveFocus()
    }
    // A typed name is judged by the core; a valid one becomes the note's category (it is saved with the note and
    // only then offered to other notes), an invalid one keeps the entry open with the reason.
    function newCategoryTyped(text) {
        var r = bridge.checkCategory(text)
        if (!r.ok) { newCategoryDialog.errorText = r.errorText; return }
        page.category = r.name
        newCategoryDialog.close()
        area.forceActiveFocus()
    }
    // ---- selection tools: each acts on the field that has the caret ----
    function selectAllActive() { activeItem.selectAll() }
    function copySelection() { if (hasSelection) clipboard.copy(activeItem.selectedText) }
    function deleteSelection() {
        var f = activeItem
        var a = f.selectionStart, b = f.selectionEnd
        if (a >= b) return
        editSession.beginGroup(activeField)
        f.remove(a, b)
        editSession.endGroup()
        f.cursorPosition = a
    }
    function cutSelection() {
        if (!hasSelection) return
        copySelection()
        deleteSelection()
    }
    function pasteIntoActive() {
        page.message = ""
        var f = activeItem
        var a = f.selectionStart, b = f.selectionEnd
        var r
        if (activeField === "title") {
            r = clipboard.titlePaste(titleField.text, a, b)
            if (!r.ok) { if (r.errorText) page.message = r.errorText; return }
            editSession.beginGroup("title")
            titleField.text = r.text
            editSession.endGroup()
            titleField.cursorPosition = r.caret
        } else {
            r = clipboard.cleanPaste()
            if (!r.ok) { if (r.errorText) page.message = r.errorText; return }
            closeSlash()
            programmatic = true
            editSession.beginGroup("text")
            if (a < b) area.remove(a, b)
            area.insert(a, r.text)
            editSession.endGroup()
            area.cursorPosition = a + r.text.length
            programmatic = false
        }
    }

    // ---- touch: a tap places the caret, a long press selects the word and opens the menu ----
    function fieldPoint(field, item, x, y) {
        return field === "title" ? item.positionAt(x) : item.positionAt(x, y)
    }
    // A tap anywhere that is not the selection ends it: in the text, in the other field, or on empty space.
    function dismissSelection() {
        if (menu) menu.close()
        area.deselect()
        titleField.deselect()
    }
    function fieldClicked(field, item, x, y) {
        dismissSelection()
        page.activeField = field
        item.forceActiveFocus()
        item.cursorPosition = fieldPoint(field, item, x, y)
        item.deselect()
        Qt.inputMethod.show()
    }
    function fieldHeld(field, item, x, y) {
        page.activeField = field
        item.forceActiveFocus()
        // below the last line of the text there is no word: the menu then only offers Paste, Select all, Undo, Redo
        var last = item.positionToRectangle(item.length)
        if (field === "text" && y > last.y + last.height) {
            item.cursorPosition = item.length
            item.deselect()
        } else {
            item.cursorPosition = fieldPoint(field, item, x, y)
            item.selectWord()
        }
        openMenu()
    }

    // ---- the context menu ----
    function menuItems() {
        var f = activeItem
        var sel = f.selectionStart < f.selectionEnd
        var list = []
        if (sel) {
            list.push({ id: "cut", label: qsTr("Cut") })
            list.push({ id: "copy", label: qsTr("Copy") })
        }
        if (clipboard.hasText) list.push({ id: "paste", label: qsTr("Paste") })
        if (sel) list.push({ id: "remove", label: qsTr("Remove") })
        if (f.length > 0 && !(sel && f.selectionStart === 0 && f.selectionEnd === f.length))
            list.push({ id: "all", label: qsTr("Select all") })
        if (editSession.canUndo) list.push({ id: "undo", label: qsTr("Undo") })
        if (editSession.canRedo) list.push({ id: "redo", label: qsTr("Redo") })
        return list
    }
    function placeMenu() {
        var f = activeItem
        var a = f.positionToRectangle(f.selectionStart)
        var b = f.positionToRectangle(f.selectionEnd)
        var pa = f.mapToItem(page, a.x, a.y)
        var pb = f.mapToItem(page, b.x, b.y + b.height)
        menu.anchorX = f.selectionStart < f.selectionEnd ? (pa.x + pb.x) / 2 : pa.x
        menu.selTop = pa.y
        menu.selBottom = pb.y
        menu.minY = topBar.height + 10
        menu.maxY = bottomPanel.y
        menu.place()
    }
    function openMenu() {
        closeSlash()
        menu.items = menuItems()
        if (menu.items.length === 0) { menu.close(); return }
        placeMenu()
        menu.visible = true
    }
    // After an action the selection is gone (Copy leaves the caret where the selection ended); Select all shows the
    // menu again for the new selection.
    function menuChosen(id) {
        menu.close()
        page.message = ""
        if (id === "cut") cutSelection()
        else if (id === "copy") { var end = activeItem.selectionEnd; copySelection(); activeItem.deselect(); activeItem.cursorPosition = end }
        else if (id === "paste") pasteIntoActive()
        else if (id === "remove") deleteSelection()
        else if (id === "all") { selectAllActive(); openMenu() }
        else if (id === "undo") undoStep()
        else if (id === "redo") redoStep()
    }

    // ---- undo / redo: the session says what to apply; the fields are changed with remove/insert ----
    function applyHistory(step) {
        programmatic = true
        applyHistoryStep(step)
        programmatic = false
    }
    function applyHistoryStep(step) {
        if (step.field === "category") page.category = step.value
        else if (step.field === "favorite") page.favorite = step.value === "1"
        else {
            var f = step.field === "title" ? titleField : area
            f.remove(step.pos, step.pos + step.removeLen)
            f.insert(step.pos, step.insert)
            if (!f.activeFocus) f.forceActiveFocus()
            if (step.selBegin === step.selEnd) f.cursorPosition = step.selBegin
            else f.select(step.selBegin, step.selEnd)
        }
        editSession.finishApply(titleField.text, area.text)
    }
    function undoStep() {
        closeSlash()
        page.message = ""
        var s = editSession.undo()
        if (s.field) applyHistory(s)
    }
    function redoStep() {
        closeSlash()
        page.message = ""
        var s = editSession.redo()
        if (s.field) applyHistory(s)
    }
    function requestLeave() {
        if (dirty) { Qt.inputMethod.hide(); discardDialog.open() } else leave()
    }

    // empty space (between and beside the controls): a tap there ends a selection; everything above it keeps its own taps
    MouseArea {
        anchors.fill: parent
        onClicked: page.dismissSelection()
    }

    TopBar {
        id: topBar
        title: page.noteId > 0 ? qsTr("Edit note") : qsTr("New note")
        leftItems: [ IconButton { text: qsTr("Back"); icon: "chevron-left"; onClicked: page.requestLeave() } ]
        rightItems: [
            IconButton { text: qsTr("Undo"); icon: "undo"; enabled: editSession.canUndo; onClicked: page.undoStep() },
            IconButton { text: qsTr("Redo"); icon: "redo"; enabled: editSession.canRedo; onClicked: page.redoStep() },
            IconButton { text: qsTr("Favorite"); icon: "star"; toggled: page.favorite; onClicked: page.favorite = !page.favorite },
            IconButton { text: qsTr("Save"); icon: "save"; onClicked: page.save() }
        ]
    }

    Column {
        id: top
        x: 30
        anchors.top: topBar.bottom
        anchors.topMargin: 14
        width: parent.width - 60
        spacing: 12

        // the title: a labelled field (the label above it, as in the device's own dialogs)
        Text {
            text: qsTr("Title")
            font.pixelSize: Style.labelSize
            color: Style.ink
        }
        TextField {
            id: titleField
            objectName: "titleField"
            width: parent.width
            height: 78
            font.pixelSize: Style.bodySize
            color: Style.ink
            placeholderText: qsTr("required")
            inputMethodHints: Qt.ImhNoPredictiveText
            background: Rectangle { color: Style.paper; radius: Style.radiusField; border.width: Style.ruleWidth; border.color: Style.ink }
            // a title is one line: Return moves on to the text, it never inserts a line break
            Keys.onReturnPressed: area.forceActiveFocus()
            Keys.onEnterPressed: area.forceActiveFocus()
            persistentSelection: false
            onTextChanged: { if (menu) menu.close(); if (page.historyReady) editSession.observe("title", text) }
            onActiveFocusChanged: { if (activeFocus) page.activeField = "title"; else deselect() }
            MouseArea {
                anchors.fill: parent
                pressAndHoldInterval: 600
                property bool held: false
                onPressed: held = false
                onPressAndHold: function(mouse) { held = true; page.fieldHeld("title", titleField, mouse.x, mouse.y) }
                onClicked: function(mouse) { if (!held) page.fieldClicked("title", titleField, mouse.x, mouse.y) }
            }
            SelectionHandles {
                target: titleField
                multiline: false
                active: page.activeField === "title"
                onDraggingChanged: if (dragging) menu.close()
                onFinished: page.openMenu()
            }
        }
        Text {
            width: parent.width
            visible: !attachments.supported
            wrapMode: Text.Wrap
            font.pixelSize: Style.captionSize
            color: Style.ink
            text: qsTr("Attachments need a newer Notes app on the server.")
        }
        Text {
            width: parent.width
            visible: page.message.length > 0
            wrapMode: Text.Wrap
            font.pixelSize: Style.captionSize
            font.bold: true
            color: Style.ink
            text: "\u2717 " + page.message
        }
        Rectangle { width: parent.width; height: Style.hairlineWidth; color: Style.hairline }
    }

    Flickable {
        id: flick
        anchors { left: parent.left; right: parent.right; top: top.bottom; bottom: bottomPanel.top; topMargin: 8 }
        contentWidth: width
        contentHeight: Math.max(area.implicitHeight, height)
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        // keep the caret in view while typing (no animation)
        onContentYChanged: if (menu && menu.visible) page.placeMenu()
        function ensureCaretVisible() {
            var r = area.cursorRectangle
            if (r.y < contentY) contentY = r.y
            else if (r.y + r.height > contentY + height) contentY = r.y + r.height - height
        }

        TextArea.flickable: TextArea {
            id: area
            objectName: "textArea"
            font.pixelSize: bridge.fontPx
            color: "#000000"
            wrapMode: TextEdit.Wrap
            height: Math.max(implicitHeight, flick.height)  // the empty part below the text is tappable too
            leftPadding: 30; rightPadding: 30; topPadding: 10; bottomPadding: 40
            inputMethodHints: Qt.ImhNoPredictiveText
            background: null
            onCursorRectangleChanged: flick.ensureCaretVisible()
            persistentSelection: false
            onTextChanged: page.areaChanged()
            onCursorPositionChanged: page.cursorMoved()
            onActiveFocusChanged: { if (activeFocus) page.activeField = "text"; else { deselect(); page.closeSlash() } }
            MouseArea {
                anchors.fill: parent
                pressAndHoldInterval: 600
                property bool held: false
                onPressed: held = false
                onPressAndHold: function(mouse) { held = true; page.fieldHeld("text", area, mouse.x, mouse.y) }
                onClicked: function(mouse) { if (!held) page.fieldClicked("text", area, mouse.x, mouse.y) }
            }
            SelectionHandles {
                target: area
                multiline: true
                active: page.activeField === "text"
                onDraggingChanged: if (dragging) menu.close()
                onFinished: page.openMenu()
            }
        }
    }

    // Attach, Category and Delete sit directly above the on-screen keyboard (or the bottom edge), so none costs a tap more
    BottomPanel {
        id: bottomPanel
        objectName: "bottomPanel"
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; bottomMargin: page.keyboardHeight }
        z: 5
        tools: [
            ToolButton { text: qsTr("Attach"); icon: "attach"; enabled: attachments.supported; onClicked: page.openPicker() },
            ToolButton {
                id: categoryButton
                text: page.category === "" ? qsTr("Category") : page.category
                icon: "tag"
                onClicked: page.openCategoryPicker()
            },
            ToolButton {
                text: qsTr("Delete"); icon: "trash"
                visible: page.noteId > 0
                onClicked: { Qt.inputMethod.hide(); deleteDialog.open() }
            }
        ]
    }

    ChoiceDialog {
        id: categoryDialog
        title: qsTr("Category")
        anchor: categoryButton
        onChosen: function (value) { page.categoryChosen(value) }
    }
    TextEntryDialog {
        id: newCategoryDialog
        title: qsTr("New category")
        placeholder: qsTr("Name")
        onAccepted: function (text) { page.newCategoryTyped(text) }
        onRejected: area.forceActiveFocus()
    }
    ConfirmDialog {
        id: discardDialog
        title: qsTr("Discard changes?")
        text: qsTr("This note has changes that were not saved.")
        acceptText: qsTr("Discard")
        cancelText: qsTr("Keep editing")
        onAccepted: page.leave()
        onRejected: area.forceActiveFocus()
    }
    ConfirmDialog {
        id: deleteDialog
        title: qsTr("Delete this note?")
        text: qsTr("The note is removed here and, at the next sync, from the server.")
        acceptText: qsTr("Delete")
        cancelText: qsTr("Cancel")
        onAccepted: {
            if (bridge.deleteNote(page.noteId)) { Qt.inputMethod.hide(); page.StackView.view.pop(null) }
        }
    }

    SelectionMenu {
        id: menu
        onChosen: function(id) { page.menuChosen(id) }
    }

    // a tap anywhere outside the "/" menu closes it and leaves the typed "/" (the press goes no further)
    MouseArea {
        id: slashScrim
        objectName: "slashScrim"
        anchors.fill: parent
        z: 1999
        visible: page.slashState !== null
        onPressed: page.closeSlash()
    }
    SlashMenu {
        id: slashMenu
        entries: slash.entries()
        onChosen: function(id) { page.slashChosen(id) }
    }
    DateTimePicker {
        id: dateTimePicker
        onAccepted: function(text) { if (page.slashState) page.applySlashEdit(slash.replace(page.slashState.pos, 2, text)) }
        onCancelled: page.closeSlash()
    }
    EmojiPicker {
        id: emojiPicker
        emojis: slash.emojis()
        onChosen: function(emoji) { if (page.slashState) page.applySlashEdit(slash.replace(page.slashState.pos, 1, emoji)) }
        onCancelled: page.closeSlash()
    }
}
