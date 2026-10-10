import QtQuick

// A modal confirmation on top of the current page: a title, a text, and two buttons.
// No animations (e-ink). Usage: ConfirmDialog { id: d; title: ...; text: ...; onAccepted: ... }, d.open().
Item {
    id: root
    property string title
    property string text
    property string acceptText: qsTr("OK")
    property string cancelText: qsTr("Cancel")
    property bool showCancel: true
    signal accepted()
    signal rejected()

    anchors.fill: parent
    visible: false
    z: 1000

    function open() { visible = true }
    function close() { visible = false }

    // swallow touches outside the box
    MouseArea { anchors.fill: parent }
    Rectangle { anchors.fill: parent; color: "#ffffff"; opacity: 0.85 }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 80, 900)
        height: column.implicitHeight + 64
        color: "#ffffff"
        border.width: 4
        border.color: "#000000"

        Column {
            id: column
            anchors.fill: parent
            anchors.margins: 32
            spacing: 24
            Text {
                width: parent.width
                text: root.title
                wrapMode: Text.Wrap
                font.pixelSize: 40
                font.bold: true
                color: "#000000"
            }
            Text {
                width: parent.width
                text: root.text
                wrapMode: Text.Wrap
                font.pixelSize: 32
                color: "#000000"
            }
            Row {
                spacing: 24
                InkButton { visible: root.showCancel; text: root.cancelText; onClicked: { root.close(); root.rejected() } }
                InkButton { text: root.acceptText; onClicked: { root.close(); root.accepted() } }
            }
        }
    }
}
