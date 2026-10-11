import QtQuick

// The "/" menu of the editor (spec 006): a bubble in the look of SelectionMenu with the entries in a two-column list
// (row by row, in the order of `entries`). The page fills `entries` ([{id, label}]) and says where the caret is
// (`anchorX`, `caretTop`, `caretBottom`) inside the room between `minY` and `maxY` (the keyboard's top). The bubble sits
// below the caret line, above it when there is no room below; it scrolls only if it is taller than the room. No
// button takes focus (the keyboard stays), nothing animates (e-ink).
Item {
    id: root
    objectName: "slashMenu"
    property var entries: []
    property real anchorX: 0
    property real caretTop: 0
    property real caretBottom: 0
    property real minY: 0
    property real maxY: 100000
    property real gap: 12
    readonly property real pad: 14
    signal chosen(string id)

    visible: false
    z: 2000
    width: bubble.width
    height: bubble.height

    function open() { place(); visible = entries.length > 0 }
    function close() { visible = false }
    function place() {
        var w = bubble.width, h = bubble.height
        x = Math.max(20, Math.min(anchorX - 40, (parent ? parent.width : w) - w - 20))
        var below = caretBottom + gap
        y = below + h <= maxY ? below : caretTop - h - gap
        y = Math.max(minY, Math.min(y, maxY - h - 10))
    }
    onEntriesChanged: if (visible) place()
    onWidthChanged: if (visible) place()
    onHeightChanged: if (visible) place()
    onVisibleChanged: if (visible) place()

    Rectangle {
        id: bubble
        readonly property real cell: Math.min(340, ((root.parent ? root.parent.width : 800) - 40 - 3 * root.pad) / 2)
        readonly property real room: Math.max(120, root.maxY - root.minY - 20)
        width: 2 * cell + 3 * root.pad
        height: Math.min(grid.height + 2 * root.pad, room)
        radius: 14
        color: "#ffffff"
        border.width: 4
        border.color: "#000000"

        Flickable {
            id: list
            x: root.pad; y: root.pad
            width: parent.width - 2 * root.pad
            height: parent.height - 2 * root.pad
            contentWidth: width
            contentHeight: grid.height
            interactive: contentHeight > height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Grid {
                id: grid
                columns: 2
                spacing: root.pad
                Repeater {
                    model: root.entries
                    delegate: InkButton {
                        width: bubble.cell
                        text: modelData.label
                        focusPolicy: Qt.NoFocus
                        font.pixelSize: 30
                        bordered: true
                        onClicked: root.chosen(modelData.id)
                    }
                }
            }
        }
    }
}
