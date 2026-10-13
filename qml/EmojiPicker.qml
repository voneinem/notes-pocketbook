import QtQuick

// The emoji grid of the editor's "/" menu (spec 006 US5): a fixed set of common emojis in six columns, each cell at
// least 78 x 78 px; a tap reports the emoji (`chosen`), Cancel (or the close icon) closes without a choice. In the look of the
// device's dialogs (spec 010). Nothing animates.
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

    readonly property int columns: 6
    readonly property real cellW: Math.max(78, Math.min(130, ((root.parent ? root.parent.width : 800) - 2 * Style.dialogPadding - (columns - 1) * 10 - 40) / columns))
    readonly property real cellH: 86
    // what the box needs besides the grid: the header with its rule, the padding, the footer and the outline
    readonly property real chrome: Style.touchMin + Style.ruleWidth + 2 * Style.dialogPadding + Style.footerHeight + Style.hairlineWidth + 2 * Style.heavyWidth

    DialogBox {
        id: panel
        title: qsTr("Emoji")
        width: root.columns * root.cellW + (root.columns - 1) * 10 + 2 * Style.dialogPadding + 2 * Style.heavyWidth
        footer: [{ text: qsTr("Cancel"), role: "dismiss" }]
        onCloseRequested: root.cancelled()
        onFooterClicked: root.cancelled()

        Flickable {
            id: list
            width: parent.width
            height: Math.min(grid.height, Math.max(root.cellH, root.maxY - root.minY - 20 - root.chrome))
            contentWidth: width
            contentHeight: grid.height
            interactive: contentHeight > height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            Grid {
                id: grid
                columns: root.columns
                spacing: 10
                Repeater {
                    model: root.emojis
                    delegate: Item {
                        objectName: "emojiCell"
                        property string text: modelData
                        signal clicked()
                        onClicked: root.chosen(modelData)
                        width: root.cellW
                        height: root.cellH
                        Rectangle { anchors.fill: parent; color: Style.pressed; visible: tap.pressed }
                        Text {
                            anchors.centerIn: parent
                            text: modelData
                            font.pixelSize: 44
                            color: Style.ink
                        }
                        MouseArea { id: tap; anchors.fill: parent; onClicked: parent.clicked() }
                    }
                }
            }
        }
    }
}
