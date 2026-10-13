import QtQuick

// A modal dialog over the current page (spec 010): the page behind is dimmed (white at 60 %, so it reads grey) and every
// touch outside the box is swallowed; the box is a DialogBox centred, or at `boxY` when that is set (the text entry sits in the
// upper part so the on-screen keyboard does not cover it). No animations (e-ink).
Item {
    id: root
    property alias title: box.title
    property alias footer: box.footer
    property real boxY: -1
    property real boxWidth: Math.min(width - 80, 900)
    default property alias contents: box.contents
    signal closeRequested()
    signal footerClicked(string role)

    anchors.fill: parent
    visible: false
    z: 1000

    function open() { visible = true }
    function close() { visible = false }

    // swallow touches outside the box
    MouseArea { anchors.fill: parent }
    Rectangle { anchors.fill: parent; color: Style.paper; opacity: Style.scrim }

    DialogBox {
        id: box
        width: root.boxWidth
        x: (root.width - width) / 2
        y: root.boxY >= 0 ? root.boxY : (root.height - height) / 2
        onCloseRequested: root.closeRequested()
        onFooterClicked: function(role) { root.footerClicked(role) }
    }
}
