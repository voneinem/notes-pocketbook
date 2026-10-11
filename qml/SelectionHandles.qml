import QtQuick

// The two markers of a text field's selection (spec 005). Put it inside the text item (a TextEdit/TextArea or a
// TextInput/TextField) so the markers scroll with the text; it follows target.selectionStart / selectionEnd and
// moves them when dragged. While a marker is dragged `dragging` is true (the page hides its menu); `finished()` is
// sent when the finger is lifted.
Item {
    id: root
    property Item target
    property bool multiline: true
    property bool active: true
    readonly property bool shown: active && target && target.selectionStart < target.selectionEnd
    property bool dragging: false
    signal finished()

    anchors.fill: parent

    function caretRect(pos) { return target.positionToRectangle(pos) }
    function positionAtPoint(x, y) { return multiline ? target.positionAt(x, y) : target.positionAt(x) }

    SelectionHandle {
        id: startHandle
        atStart: true
        visible: root.shown
        property rect r: root.target ? root.caretRect(root.target.selectionStart) : Qt.rect(0, 0, 0, 0)
        lineHeight: r.height
        point: Qt.point(r.x, r.y + r.height)
        onPressedDown: root.dragging = true
        onDragged: function(x, y) {
            var pos = Math.min(root.positionAtPoint(x, y), root.target.selectionEnd - 1)
            if (pos >= 0 && pos !== root.target.selectionStart) root.target.select(pos, root.target.selectionEnd)
        }
        onReleased: { root.dragging = false; root.finished() }
    }
    SelectionHandle {
        id: endHandle
        atStart: false
        visible: root.shown
        property rect r: root.target ? root.caretRect(root.target.selectionEnd) : Qt.rect(0, 0, 0, 0)
        lineHeight: r.height
        point: Qt.point(r.x, r.y + r.height)
        onPressedDown: root.dragging = true
        onDragged: function(x, y) {
            var pos = Math.max(root.positionAtPoint(x, y), root.target.selectionStart + 1)
            if (pos <= root.target.length && pos !== root.target.selectionEnd) root.target.select(root.target.selectionStart, pos)
        }
        onReleased: { root.dragging = false; root.finished() }
    }
}
