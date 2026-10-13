import QtQuick

// The "/" menu of the editor (spec 006, look: spec 010): a bubble in the look of SelectionMenu with the entries in two columns
// (row by row), an icon and the label each, split by dotted dividers (in the order of `entries`). The page fills `entries` ([{id, label}]) and says where the caret is
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
    property real gap: 28
    readonly property real pad: 14
    property bool pointerAtTop: true    // the bubble sits below the caret: its pointer is on its top edge
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
        pointerAtTop = below + h <= maxY
        y = below + h <= maxY ? below : caretTop - h - gap
        y = Math.max(minY, Math.min(y, maxY - h - 10))
    }
    onEntriesChanged: if (visible) place()
    onWidthChanged: if (visible) place()
    onHeightChanged: if (visible) place()
    onVisibleChanged: if (visible) place()

    PopupBubble {
        id: bubble
        readonly property real cell: Math.min(340, ((root.parent ? root.parent.width : 800) - 40 - 3 * root.pad) / 2)
        readonly property real room: Math.max(120, root.maxY - root.minY - 20)
        width: 2 * cell + 3 * root.pad
        height: Math.min(grid.height + 2 * root.pad, room)
        pointerUp: root.pointerAtTop
        pointerX: root.anchorX - root.x

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
                    delegate: Item {
                        id: cellItem
                        property string text: modelData.label
                        signal clicked()
                        onClicked: root.chosen(modelData.id)
                        width: bubble.cell
                        height: Style.touchMin
                        Rectangle { anchors.fill: parent; color: Style.pressed; visible: tap.pressed }
                        Icon { id: cellIcon; x: 6; anchors.verticalCenter: parent.verticalCenter; size: 48; source: "slash-" + modelData.id }
                        Text {
                            anchors { left: cellIcon.right; leftMargin: 14; right: parent.right; verticalCenter: parent.verticalCenter }
                            text: modelData.label
                            elide: Text.ElideRight
                            font.pixelSize: Style.labelSize
                            color: Style.ink
                        }
                        // dotted divider under every cell except those of the last row
                        RowDivider { objectName: "rowDivider"; visible: index < root.entries.length - 2; anchors.bottom: parent.bottom; width: parent.width }
                        MouseArea { id: tap; anchors.fill: parent; onClicked: cellItem.clicked() }
                    }
                }
            }
        }
    }
}
