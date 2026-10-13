import QtQuick

// A borderless text action of a dialog's footer (spec 010): upper-case label, fills its cell, light grey while pressed.
// `text` and `clicked` like every control, so a driver finds it by its label.
Item {
    id: root
    objectName: "footerButton"
    property string text
    property alias font: label.font
    signal clicked()

    implicitWidth: 220
    implicitHeight: Style.footerHeight
    opacity: enabled ? 1.0 : 0.4

    Rectangle { anchors.fill: parent; color: Style.pressed; visible: tap.pressed && root.enabled }
    Text {
        id: label
        anchors.centerIn: parent
        width: parent.width - 24
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        text: root.text
        font.pixelSize: Style.footerSize
        font.capitalization: Font.AllUppercase
        color: Style.ink
    }
    MouseArea { id: tap; anchors.fill: parent; enabled: root.enabled; onClicked: root.clicked() }
}
