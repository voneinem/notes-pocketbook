import QtQuick

// One text block of a note in the reader: a heading, a paragraph or a list item (bullet, number or a
// drawn task box), with its styled text. Emits taskToggled(line) when the task box is tapped and
// linkActivated(link) when a link in the text is tapped; the reader decides what happens then. The text can be
// selected (spec 005): selFrom/selTo (UTF-16 offsets, -1 = none) are set by the page; wordHeld(start, end) reports a
// press-and-hold, tapped(pos) a tap that is not on a link. A blank block is selected whole (`selected`).
Item {
    id: root
    property var block
    property int fontPx: 38
    signal taskToggled(int line)
    signal linkActivated(string link)
    signal wordHeld(int start, int end)
    signal tapped(int pos)
    property int selFrom: -1
    property int selTo: -1
    property bool selected: false          // a blank line inside the selection
    readonly property int textLength: text.length
    // a character's rectangle and the character under a point, in this item's coordinates (for the reader's markers)
    function rectAt(offset) {
        var r = text.positionToRectangle(offset)
        var p = text.mapToItem(root, r.x, r.y)
        return Qt.rect(p.x, p.y, r.width, r.height)
    }
    function offsetAt(x, y) {
        var p = root.mapToItem(text, x, y)
        return text.positionAt(p.x, p.y)
    }

    function headingPx(level) {
        var scale = [1.6, 1.4, 1.25, 1.15, 1.1, 1.05]
        return Math.round(fontPx * scale[Math.min(5, Math.max(0, level - 1))])
    }

    // The block's own height plus the 10 px gap to the next block (table rows must touch each other, so the
    // reader's Column has no spacing of its own).
    implicitHeight: (block.kind === "blank" ? Math.round(fontPx * 0.5) : row.implicitHeight) + 10

    Row {
        id: row
        visible: root.block.kind !== "blank"
        x: root.block.kind === "listitem" ? root.block.depth * 36 : 0
        spacing: 14
        width: parent.width - x

        // list marker: bullet, number, or a drawn checkbox
        Item {
            visible: root.block.kind === "listitem"
            width: root.block.kind === "listitem" ? Math.round(root.fontPx * 1.1) : 0
            height: root.fontPx * 1.3
            Rectangle {
                visible: root.block.task
                width: Math.round(root.fontPx * 0.8); height: width
                anchors.verticalCenter: parent.verticalCenter
                color: "#ffffff"; border.width: 3; border.color: "#000000"
                Rectangle {
                    visible: root.block.checked
                    anchors.centerIn: parent
                    width: parent.width - 12; height: width
                    color: "#000000"
                }
                // generous touch target around the drawn box; saves at once (offline edit)
                MouseArea {
                    anchors.centerIn: parent
                    width: parent.width + 40; height: parent.height + 40
                    onClicked: root.taskToggled(root.block.line)
                }
            }
            Text {
                visible: !root.block.task
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: root.fontPx
                color: "#000000"
                text: root.block.ordered ? root.block.number + "." : "•"
            }
        }
        SelectableText {
            id: text
            width: parent.width - (root.block.kind === "listitem" ? Math.round(root.fontPx * 1.1) + 14 : 0)
            font.pixelSize: root.block.kind === "heading" ? root.headingPx(root.block.level) : root.fontPx
            font.bold: root.block.kind === "heading"
            html: root.block.html
            selFrom: root.selFrom
            selTo: root.selTo
            onLinkTapped: function(link) { root.linkActivated(link) }
            onWordHeld: function(start, end) { root.wordHeld(start, end) }
            onTapped: function(pos) { root.tapped(pos) }
        }
    }

    // a blank line has no text to tap: the whole line is a selection unit
    MouseArea {
        anchors.fill: parent
        enabled: root.block.kind === "blank"
        pressAndHoldInterval: 600
        property bool held: false
        onPressed: held = false
        onPressAndHold: { held = true; root.wordHeld(0, 0) }
        onClicked: if (!held) root.tapped(0)
    }
    Rectangle {
        visible: root.selected
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: root.implicitHeight - 10
        color: "transparent"
        border.width: 4
        border.color: "#000000"
    }
}
