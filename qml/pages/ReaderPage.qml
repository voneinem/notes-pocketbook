import QtQuick
import QtQuick.Controls
import ".."

// Reads one note: headings, bold/italic, lists and task boxes (read-only here), links as plain
// text. Large type, font size small/medium/large, scrolling by page buttons and edge taps.
Item {
    id: page
    property int noteId: 0
    property var note: ({ title: "", blocks: [] })

    function load() { note = bridge.openNote(noteId) }
    Component.onCompleted: load()
    StackView.onActivated: load()

    function headingPx(level) {
        var scale = [1.6, 1.4, 1.25, 1.15, 1.1, 1.05]
        return Math.round(bridge.fontPx * scale[Math.min(5, Math.max(0, level - 1))])
    }

    Row {
        id: top
        x: 30; y: 20
        width: parent.width - 60
        spacing: 16
        InkButton { id: back; text: qsTr("Back"); onClicked: page.StackView.view.pop() }
        InkButton {
            text: qsTr("A-")
            enabled: bridge.fontSize !== "small"
            onClicked: bridge.fontSize = bridge.fontSize === "large" ? "medium" : "small"
        }
        InkButton {
            text: qsTr("A+")
            enabled: bridge.fontSize !== "large"
            onClicked: bridge.fontSize = bridge.fontSize === "small" ? "medium" : "large"
        }
        InkButton {
            text: qsTr("Edit")
            onClicked: page.StackView.view.push(Qt.resolvedUrl("EditorPage.qml"), { noteId: page.noteId })
        }
        Text {
            width: Math.max(0, parent.width - 4 * 160)
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
            font.pixelSize: 26
            color: "#000000"
            text: (page.note.favorite ? "★ " : "") + (page.note.category || "")
        }
    }
    Rectangle { id: rule; anchors.top: top.bottom; anchors.topMargin: 12; width: parent.width; height: 2; color: "#111111" }

    Flickable {
        id: flick
        anchors { left: parent.left; right: parent.right; top: rule.bottom; bottom: footer.top }
        contentWidth: width
        contentHeight: body.implicitHeight + 40
        interactive: false
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: body
            x: 50; y: 20
            width: flick.width - 100
            spacing: 10

            Repeater {
                model: page.note.blocks
                delegate: Item {
                    id: blockItem
                    readonly property var b: modelData
                    width: body.width
                    height: b.kind === "blank" ? Math.round(bridge.fontPx * 0.5) : row.implicitHeight

                    Row {
                        id: row
                        visible: blockItem.b.kind !== "blank"
                        x: blockItem.b.kind === "listitem" ? blockItem.b.depth * 36 : 0
                        spacing: 14
                        width: parent.width - x

                        // list marker: bullet, number, or a drawn checkbox
                        Item {
                            visible: blockItem.b.kind === "listitem"
                            width: blockItem.b.kind === "listitem" ? Math.round(bridge.fontPx * 1.1) : 0
                            height: bridge.fontPx * 1.3
                            Rectangle {
                                visible: blockItem.b.task
                                width: Math.round(bridge.fontPx * 0.8); height: width
                                anchors.verticalCenter: parent.verticalCenter
                                color: "#ffffff"; border.width: 3; border.color: "#000000"
                                Rectangle {
                                    visible: blockItem.b.checked
                                    anchors.centerIn: parent
                                    width: parent.width - 12; height: width
                                    color: "#000000"
                                }
                                // generous touch target around the drawn box; saves at once (offline edit)
                                MouseArea {
                                    anchors.centerIn: parent
                                    width: parent.width + 40; height: parent.height + 40
                                    onClicked: {
                                        var y = flick.contentY
                                        if (bridge.toggleTask(page.noteId, blockItem.b.line)) { page.load(); flick.contentY = y }
                                    }
                                }
                            }
                            Text {
                                visible: !blockItem.b.task
                                anchors.verticalCenter: parent.verticalCenter
                                font.pixelSize: bridge.fontPx
                                color: "#000000"
                                text: blockItem.b.ordered ? blockItem.b.number + "." : "•"
                            }
                        }
                        Text {
                            width: parent.width - (blockItem.b.kind === "listitem" ? Math.round(bridge.fontPx * 1.1) + 14 : 0)
                            wrapMode: Text.Wrap
                            textFormat: Text.StyledText
                            color: "#000000"
                            font.pixelSize: blockItem.b.kind === "heading" ? page.headingPx(blockItem.b.level) : bridge.fontPx
                            font.bold: blockItem.b.kind === "heading"
                            text: blockItem.b.html
                        }
                    }
                }
            }
        }

        // Tap zones at the left and right edge: previous / next page, instantly.
        // (Placed in the viewport, not in the content.)
    }
    MouseArea {
        anchors { left: parent.left; top: flick.top; bottom: flick.bottom }
        width: 50
        onClicked: pager.pageBy(-1)
    }
    MouseArea {
        anchors { right: parent.right; top: flick.top; bottom: flick.bottom }
        width: 50
        onClicked: pager.pageBy(1)
    }

    Row {
        id: footer
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 16 }
        spacing: 24
        PageButtons { id: pager; target: flick }
    }
}
