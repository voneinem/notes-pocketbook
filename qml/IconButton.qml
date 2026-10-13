import QtQuick

// An icon-only control without a border (spec 010): the universal actions of a top bar. `text` is its label: the
// accessible name, what the tests look for, and what is drawn when the icon cannot be loaded. `toggled` shows the
// `<icon>-filled` picture (only for icons that have one). Disabled controls are drawn lighter; a press fills light grey.
Item {
    id: root
    property string text
    property string icon
    property bool toggled: false
    property int iconSize: Style.iconTop
    signal clicked()

    implicitWidth: Style.touchMin
    implicitHeight: Style.touchMin
    width: implicitWidth
    height: implicitHeight
    opacity: enabled ? 1.0 : 0.4

    Rectangle { anchors.fill: parent; color: Style.pressed; visible: tap.pressed && root.enabled }
    Icon {
        id: glyph
        anchors.centerIn: parent
        size: root.iconSize
        source: root.icon.length > 0 ? (root.toggled ? root.icon + "-filled" : root.icon) : ""
    }
    // the fallback: the label, when there is no picture
    Text {
        anchors.centerIn: parent
        visible: !glyph.ready
        width: Math.min(implicitWidth, parent.width)
        text: root.text
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: Style.captionSize
        color: Style.ink
    }
    MouseArea {
        id: tap
        anchors.fill: parent
        enabled: root.enabled
        onClicked: root.clicked()
    }
}
