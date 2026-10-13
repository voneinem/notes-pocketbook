import QtQuick

// An on/off pill in the device's style (spec 010): a rounded outline, the chosen half filled black with "on" or "off" in white,
// the other half white. A tap anywhere on it reports the new state (`toggled(on)`); the owner sets `checked`.
Item {
    id: root
    property string text
    property bool checked: false
    signal toggled(bool on)

    implicitWidth: 170
    implicitHeight: 76
    width: implicitWidth
    height: implicitHeight
    opacity: enabled ? 1.0 : 0.4

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: Style.paper
        border.width: 3
        border.color: Style.ink
    }
    // the filled half: at the right when on, at the left when off
    Rectangle {
        x: root.checked ? parent.width / 2 - 2 : 2
        y: 2
        width: parent.width / 2
        height: parent.height - 4
        radius: height / 2
        color: Style.ink
        Text {
            anchors.centerIn: parent
            text: root.checked ? "on" : "off"
            font.pixelSize: Style.bodySize
            color: Style.paper
        }
    }
    MouseArea { anchors.fill: parent; enabled: root.enabled; onClicked: root.toggled(!root.checked) }
}
