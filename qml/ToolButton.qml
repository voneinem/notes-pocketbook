import QtQuick

// A tool of a bottom panel (spec 010): an icon over a one-line label, no border. `text` is the label (also what a driver
// looks for); `toggled` shows the `<icon>-filled` picture. Disabled tools are drawn lighter; a press fills light grey.
Item {
    id: root
    property string text
    property string icon
    property bool toggled: false
    signal clicked()

    implicitWidth: 160
    implicitHeight: Style.touchMin
    width: implicitWidth
    height: implicitHeight
    opacity: enabled ? 1.0 : 0.4

    Rectangle { anchors.fill: parent; color: Style.pressed; visible: tap.pressed && root.enabled }
    Icon {
        id: glyph
        anchors.horizontalCenter: parent.horizontalCenter
        y: 2
        size: Style.iconTool
        source: root.icon.length > 0 ? (root.toggled ? root.icon + "-filled" : root.icon) : ""
    }
    Text {
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 2 }
        width: parent.width - 8
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        text: root.text
        font.pixelSize: Style.captionSize
        color: Style.ink
    }
    MouseArea { id: tap; anchors.fill: parent; enabled: root.enabled; onClicked: root.clicked() }
}
