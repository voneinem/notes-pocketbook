import QtQuick
import QtQuick.Controls
import ".."

// Plain-text editor for one note (noteId <= 0 creates a new one). Nothing is saved automatically:
// "Save" writes the note (offline, it waits for the next sync); leaving with unsaved changes asks first.
// The category is a single line above the text; the first line of the text is the note's title.
Item {
    id: page
    property int noteId: -1
    property string startText: ""
    property string startCategory: ""
    property bool startFavorite: false
    property bool favorite: false
    property string message: ""

    readonly property bool dirty: area.text !== startText || categoryField.text !== startCategory || favorite !== startFavorite
    readonly property real keyboardHeight: Qt.inputMethod.visible ? Qt.inputMethod.keyboardRectangle.height * Screen.devicePixelRatio : 0

    Component.onCompleted: {
        if (noteId > 0) {
            var n = bridge.loadForEdit(noteId)
            if (n.text !== undefined) {
                startText = n.text
                startCategory = n.category
                startFavorite = n.favorite
            }
        }
        area.text = startText
        categoryField.text = startCategory
        favorite = startFavorite
        area.forceActiveFocus()
        area.cursorPosition = area.length
        Qt.inputMethod.show()
    }

    Connections {
        target: bridge
        function onErrorOccurred(messageKey, text) { page.message = text }
    }

    function leave() {
        Qt.inputMethod.hide()
        page.StackView.view.pop()
    }
    function save() {
        page.message = ""
        var id = bridge.saveNote(noteId, area.text, categoryField.text, favorite)
        if (id > 0) leave()
    }
    function requestLeave() {
        if (dirty) { Qt.inputMethod.hide(); discardDialog.open() } else leave()
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
                text: qsTr("Category")
                font.pixelSize: 28
                anchors.verticalCenter: parent.verticalCenter
                color: "#000000"
            }
            TextField {
                id: categoryField
                width: parent.width - 170
                font.pixelSize: 30
                color: "#000000"
                placeholderText: qsTr("none")
                inputMethodHints: Qt.ImhNoPredictiveText
                background: Rectangle { color: "#ffffff"; border.width: 2; border.color: "#111111" }
            }
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
        }
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
}
