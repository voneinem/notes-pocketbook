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

    // ---- selection tools (spec 005) ----
    property string activeField: "text"
    readonly property var activeItem: activeField === "title" ? titleField : area
    readonly property bool hasSelection: activeItem.selectionStart < activeItem.selectionEnd

    readonly property bool dirty: titleField.text !== startTitle || area.text !== startText
                                  || category !== startCategory || favorite !== startFavorite
    readonly property real keyboardHeight: Qt.inputMethod.visible ? Qt.inputMethod.keyboardRectangle.height * Screen.devicePixelRatio : 0

    Component.onCompleted: {
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
        area.insert(lineStart, token + "\n")
        area.cursorPosition = lineStart + token.length + 1
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
        Qt.inputMethod.hide()
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
            editSession.beginGroup("text")
            if (a < b) area.remove(a, b)
            area.insert(a, r.text)
            editSession.endGroup()
            area.cursorPosition = a + r.text.length
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
        menu.minY = 20
        menu.maxY = page.height - page.keyboardHeight
        menu.place()
    }
    function openMenu() {
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
        page.message = ""
        var s = editSession.undo()
        if (s.field) applyHistory(s)
    }
    function redoStep() {
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

    Column {
        id: top
        x: 30; y: 20
        width: parent.width - 60
        spacing: 12

        Row {
            spacing: 14
            InkButton { text: qsTr("Cancel"); onClicked: page.requestLeave() }
            InkButton { text: qsTr("Save"); onClicked: page.save() }
            InkButton { text: page.favorite ? qsTr("★ Favorite") : qsTr("Favorite"); onClicked: page.favorite = !page.favorite }
            InkButton { text: qsTr("Attach"); enabled: attachments.supported; onClicked: page.openPicker() }
            InkButton {
                text: qsTr("Delete")
                visible: page.noteId > 0
                onClicked: { Qt.inputMethod.hide(); deleteDialog.open() }
            }
        }
        Row {
            width: parent.width
            spacing: 14
            Text {
                text: qsTr("Title")
                font.pixelSize: 28
                anchors.verticalCenter: parent.verticalCenter
                color: "#000000"
            }
            TextField {
                id: titleField
                width: parent.width - 120
                font.pixelSize: 30
                color: "#000000"
                placeholderText: qsTr("required")
                inputMethodHints: Qt.ImhNoPredictiveText
                background: Rectangle { color: "#ffffff"; border.width: 2; border.color: "#111111" }
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
        }
        Row {
            width: parent.width
            spacing: 14
            Text {
                text: qsTr("Category")
                font.pixelSize: 28
                anchors.verticalCenter: parent.verticalCenter
                color: "#000000"
            }
            InkButton {
                id: categoryButton
                width: parent.width - 170
                text: page.category === "" ? qsTr("No category") : page.category
                onClicked: page.openCategoryPicker()
            }
        }
        Text {
            width: parent.width
            visible: !attachments.supported
            wrapMode: Text.Wrap
            font.pixelSize: 24
            color: "#000000"
            text: qsTr("Attachments need a newer Notes app on the server.")
        }
        Text {
            width: parent.width
            visible: page.message.length > 0
            wrapMode: Text.Wrap
            font.pixelSize: 26
            font.bold: true
            color: "#000000"
            text: "✗ " + page.message
        }
        Rectangle { width: parent.width; height: 2; color: "#111111" }
    }

    Flickable {
        id: flick
        anchors { left: parent.left; right: parent.right; top: top.bottom; bottom: parent.bottom; topMargin: 8
                  bottomMargin: page.keyboardHeight }
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
            font.pixelSize: bridge.fontPx
            color: "#000000"
            wrapMode: TextEdit.Wrap
            height: Math.max(implicitHeight, flick.height)  // the empty part below the text is tappable too
            leftPadding: 30; rightPadding: 30; topPadding: 10; bottomPadding: 40
            inputMethodHints: Qt.ImhNoPredictiveText
            background: null
            onCursorRectangleChanged: flick.ensureCaretVisible()
            persistentSelection: false
            onTextChanged: { if (menu) menu.close(); if (page.historyReady) editSession.observe("text", text) }
            onActiveFocusChanged: { if (activeFocus) page.activeField = "text"; else deselect() }
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

    ChoiceDialog {
        id: categoryDialog
        title: qsTr("Category")
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
}
