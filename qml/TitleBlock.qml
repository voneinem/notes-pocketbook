import QtQuick

// The note's title above its text on the reading page (spec 005): bold, larger than the body text, wrapped in full
// (never elided), selectable like a text block. The caller shows it only for a note that has a title.
Item {
    id: root
    property string title
    property int fontPx: 38
    property int selFrom: -1
    property int selTo: -1
    signal wordHeld(int start, int end)
    signal tapped(int pos)
    readonly property int textLength: label.length
    function rectAt(offset) {
        var r = label.positionToRectangle(offset)
        var p = label.mapToItem(root, r.x, r.y)
        return Qt.rect(p.x, p.y, r.width, r.height)
    }
    function offsetAt(x, y) {
        var p = root.mapToItem(label, x, y)
        return label.positionAt(p.x, p.y)
    }

    implicitHeight: label.implicitHeight + 24   // the gap to the first block

    function escaped(s) { return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;") }

    SelectableText {
        id: label
        width: parent.width
        html: root.escaped(root.title)
        font.pixelSize: Math.round(root.fontPx * 1.7)
        font.bold: true
        selFrom: root.selFrom
        selTo: root.selTo
        onWordHeld: function(start, end) { root.wordHeld(start, end) }
        onTapped: function(pos) { root.tapped(pos) }
    }
}
