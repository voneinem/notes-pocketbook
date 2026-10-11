import QtQuick
import QtQuick.Controls

// A modal one-line text entry on top of the current page: a heading, a field, an error line, Cancel and OK.
// No animations (e-ink). The box sits in the upper part of the screen so the on-screen keyboard does not cover it.
// Usage: TextEntryDialog { id: d; title: ...; onAccepted: function (text) { ...validate...; d.close() or d.errorText = ... } },
// d.open("initial"). `accepted` leaves the dialog open: the caller judges the text and either closes it or sets errorText.
Item {
    id: root
    property string title
    property string placeholder
    property string errorText
    property string acceptText: qsTr("OK")
    property string cancelText: qsTr("Cancel")
    signal accepted(string text)
    signal rejected()

    anchors.fill: parent
    visible: false
    z: 1000

    function open(initial) {
        field.text = initial === undefined ? "" : initial
        errorText = ""
        visible = true
        field.forceActiveFocus()
        Qt.inputMethod.show()
    }
    function close() {
        Qt.inputMethod.hide()
        visible = false
    }

    // swallow touches outside the box
    MouseArea { anchors.fill: parent }
    Rectangle { anchors.fill: parent; color: "#ffffff"; opacity: 0.85 }

    Rectangle {
        x: (parent.width - width) / 2
        y: 120
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
            TextField {
                id: field
                width: parent.width
                font.pixelSize: 32
                color: "#000000"
                placeholderText: root.placeholder
                inputMethodHints: Qt.ImhNoPredictiveText
                background: Rectangle { color: "#ffffff"; border.width: 2; border.color: "#111111" }
                onAccepted: root.accepted(text)   // Return = OK
                onTextChanged: root.errorText = ""
            }
            Text {
                width: parent.width
                visible: root.errorText.length > 0
                text: "✗ " + root.errorText
                wrapMode: Text.Wrap
                font.pixelSize: 28
                font.bold: true
                color: "#000000"
            }
            Row {
                spacing: 24
                InkButton { text: root.cancelText; onClicked: { root.close(); root.rejected() } }
                InkButton { text: root.acceptText; onClicked: root.accepted(field.text) }
            }
        }
    }
}
