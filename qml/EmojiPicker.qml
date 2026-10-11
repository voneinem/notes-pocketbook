import QtQuick

// The emoji grid of the editor's "/" menu (spec 006 US5): a fixed set of common emojis in six columns, each cell at
// least 78 x 78 px; a tap reports the emoji (`chosen`), Cancel closes without a choice. No button takes focus, nothing
// animates.
Item {
    id: root
    objectName: "emojiPicker"
    property var emojis: []
    property real minY: 0
    property real maxY: 100000
    signal chosen(string emoji)
    signal cancelled()

    visible: false
    z: 2000
    width: panel.width
    height: panel.height

    function open() { visible = true; place() }
    function close() { visible = false }
    function place() {
        x = Math.max(0, ((parent ? parent.width : width) - width) / 2)
        y = Math.max(minY + 10, minY + (maxY - minY - height) / 2)
    }
    onWidthChanged: if (visible) place()
    onHeightChanged: if (visible) place()

    Rectangle {
        id: panel
        readonly property int columns: 6
        readonly property real cellW: Math.max(78, Math.min(130, ((root.parent ? root.parent.width : 800) - 2 * 24 - (columns - 1) * 10 - 40) / columns))
        readonly property real cellH: 86
        width: columns * cellW + (columns - 1) * 10 + 2 * 24
        height: Math.min(grid.height + cancel.height + 14 + 2 * 24, Math.max(200, root.maxY - root.minY - 20))
        radius: 14
        color: "#ffffff"
        border.width: 4
        border.color: "#000000"

        Flickable {
            id: list
            x: 24; y: 24
            width: parent.width - 2 * 24
            height: parent.height - 2 * 24 - cancel.height - 14
            contentWidth: width
            contentHeight: grid.height
            interactive: contentHeight > height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            Grid {
                id: grid
                columns: panel.columns
                spacing: 10
                Repeater {
                    model: root.emojis
                    delegate: InkButton {
                        objectName: "emojiCell"
                        width: panel.cellW
                        height: panel.cellH
                        text: modelData
                        focusPolicy: Qt.NoFocus
                        font.pixelSize: 44
                        leftPadding: 0; rightPadding: 0
                        onClicked: root.chosen(modelData)
                    }
                }
            }
        }
        InkButton {
            id: cancel
            anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 24 }
            width: 260
            text: qsTr("Cancel")
            focusPolicy: Qt.NoFocus
            onClicked: root.cancelled()
        }
    }
}
