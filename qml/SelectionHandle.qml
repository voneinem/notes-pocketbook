import QtQuick

// One selection marker (spec 005), like the reader's own: a black square with three white lines that hangs below the
// text line it marks, with a thin stem up to that line. The start marker hangs to the left of its point and the end
// marker to the right. The parent places the marker by setting `point` (the pointed corner, in the parent's
// coordinates); dragging it reports where the finger points at, as a point on the text line above the marker.
Item {
    id: root
    objectName: "selectionHandle"
    property bool atStart: true
    property real lineHeight: 40
    property point point: Qt.point(0, 0)   // where the stem meets the text: x, bottom of the marked line
    readonly property real size: 64
    signal dragged(real x, real y)         // the text point under the marker, in the parent's coordinates
    signal released()
    signal pressedDown()

    x: point.x - (atStart ? size : 0)
    y: point.y
    width: size
    height: size

    // the stem along the marked line
    Rectangle {
        x: root.atStart ? root.size - 4 : 0
        y: -root.lineHeight
        width: 4
        height: root.lineHeight
        color: "#000000"
    }
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        Column {
            anchors.centerIn: parent
            spacing: 8
            Repeater { model: 3; Rectangle { width: 28; height: 4; color: "#ffffff" } }
        }
    }

    MouseArea {
        id: area
        // a generous touch target around the square
        x: -20; y: -20; width: root.width + 40; height: root.height + 40
        preventStealing: true
        property real grabX: 0
        property real grabY: 0
        onPressed: function(mouse) {
            grabX = mouse.x + x
            grabY = mouse.y + y
            root.pressedDown()
        }
        onPositionChanged: function(mouse) {
            if (!pressed) return
            var p = mapToItem(root.parent, mouse.x, mouse.y)
            // where the pointed corner would be if the marker followed the finger
            var cornerX = p.x - (grabX - (root.atStart ? root.size : 0))
            var cornerY = p.y - grabY
            root.dragged(cornerX, cornerY - root.lineHeight / 2)
        }
        onReleased: root.released()
        onCanceled: root.released()
    }
}
