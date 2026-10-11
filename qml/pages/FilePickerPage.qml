import QtQuick
import QtQuick.Controls
import ".."

// A small file browser for the attach button: the current folder's path, "Up", folders and files as big rows
// (whole rows per page, no kinetic scrolling). Tapping a file shows its name and size and asks before it is
// chosen; a file over the size limit is refused with a message first. Emits picked(path) and goes back.
Item {
    id: page
    property string path: attachments.rootDirectory
    property var rows: attachments.listDirectory(path)
    property string message: ""
    property string chosen: ""
    signal picked(string filePath)

    function join(base, name) { return base.charAt(base.length - 1) === "/" ? base + name : base + "/" + name }
    function parentOf(p) {
        var cut = p.length > 1 && p.charAt(p.length - 1) === "/" ? p.substring(0, p.length - 1) : p
        var i = cut.lastIndexOf("/")
        return i <= 0 ? "/" : cut.substring(0, i)
    }
    function open(entry) {
        message = ""
        if (entry.dir) { path = join(path, entry.name); list.contentY = 0; return }
        if (entry.size > attachments.maxBytes) {
            message = qsTr("This file is larger than the %1 limit.").arg(bridge.formatSize(attachments.maxBytes))
            return
        }
        chosen = join(path, entry.name)
        confirm.title = qsTr("Attach this file?")
        confirm.text = entry.name + "\n" + bridge.formatSize(entry.size)
        confirm.open()
    }

    Row {
        id: top
        x: 30; y: 20
        width: parent.width - 60
        spacing: 16
        InkButton { text: qsTr("Cancel"); onClicked: page.StackView.view.pop() }
        InkButton { text: qsTr("Up"); enabled: page.path !== "/"; onClicked: { page.message = ""; page.path = page.parentOf(page.path); list.contentY = 0 } }
    }
    Text {
        id: pathLine
        anchors { top: top.bottom; topMargin: 12; left: parent.left; right: parent.right; leftMargin: 30; rightMargin: 30 }
        elide: Text.ElideMiddle
        font.pixelSize: 26
        color: "#000000"
        text: page.path
    }
    Text {
        id: notice
        anchors { top: pathLine.bottom; left: parent.left; right: parent.right; leftMargin: 30; rightMargin: 30 }
        visible: page.message.length > 0
        wrapMode: Text.Wrap
        font.pixelSize: 26
        font.bold: true
        color: "#000000"
        text: page.message
    }
    Rectangle { id: rule; anchors.top: notice.visible ? notice.bottom : pathLine.bottom; anchors.topMargin: 8; width: parent.width; height: 2; color: "#111111" }

    ListView {
        id: list
        anchors { left: parent.left; right: parent.right; top: rule.bottom; bottom: footer.top }
        clip: true
        model: page.rows
        interactive: false
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: false

        delegate: Item {
            width: list.width
            height: 104
            readonly property var entry: modelData
            Row {
                x: 30
                width: parent.width - 60
                anchors.verticalCenter: parent.verticalCenter
                spacing: 14
                Text {
                    width: 90
                    font.pixelSize: 26
                    color: "#000000"
                    text: entry.dir ? qsTr("Folder") : ""
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    width: parent.width - 90 - 14 - sizeText.width - 14
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    font.pixelSize: 34
                    font.bold: entry.dir
                    color: "#000000"
                    text: entry.name
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    id: sizeText
                    font.pixelSize: 26
                    color: "#000000"
                    text: entry.dir ? "" : bridge.formatSize(entry.size)
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#808080" }
            Rectangle { anchors.fill: parent; color: "#d8d8d8"; visible: tap.pressed; z: -1 }
            MouseArea { id: tap; anchors.fill: parent; onClicked: page.open(entry) }
        }
    }
    Text {
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 40 }
        visible: page.rows.length === 0
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: 30
        color: "#000000"
        text: qsTr("This folder is empty or cannot be read.")
    }

    Row {
        id: footer
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 16 }
        spacing: 24
        PageButtons { target: list; snap: 104 }
    }

    ConfirmDialog {
        id: confirm
        acceptText: qsTr("Attach")
        cancelText: qsTr("Cancel")
        onAccepted: { page.picked(page.chosen); page.StackView.view.pop() }
    }
}
