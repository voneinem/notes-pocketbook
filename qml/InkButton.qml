import QtQuick
import QtQuick.Controls

// A flat button for e-ink: white fill, a crisp 2 px border, light grey when pressed. The default
// control style's bevel and shadow read as a grey bar on the panel. Adapted from InkFeed (see NOTICE).
Button {
    id: control
    property bool bordered: true
    font.pixelSize: 30
    leftPadding: 22; rightPadding: 22; topPadding: 8; bottomPadding: 8

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.enabled ? "#000000" : "#9a9a9a"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        implicitHeight: 78
        implicitWidth: 96
        radius: 6
        color: control.down ? "#d8d8d8" : (control.bordered ? "#ffffff" : "transparent")
        border.width: control.bordered ? 2 : 0
        border.color: control.enabled ? "#111111" : "#cfcfcf"
    }
}
