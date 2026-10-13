import QtQuick

// The frame of every pop-up menu (spec 010): a white rounded rectangle with a heavy outline and a pointer toward where it
// came from (the pointer sits on the top edge pointing up, or on the bottom edge pointing down). The owner sets `width` and
// `height`, places the bubble and puts the content inside it (children start at the bubble's top left corner). Nothing animates.
Item {
    id: root
    objectName: "popupBubble"
    property real pointerX: width / 2     // where the pointer touches the edge, in the bubble's own coordinates
    property bool pointerUp: true         // true: the pointer is on the top edge; false: on the bottom edge
    property bool showPointer: true
    default property alias contents: frame.data

    Rectangle {
        id: frame
        anchors.fill: parent
        radius: Style.radiusDialog
        color: Style.paper
        border.width: Style.heavyWidth
        border.color: Style.ink
    }
    Image {
        objectName: "bubblePointer"
        visible: root.showPointer
        source: Qt.resolvedUrl("icons/bubble-pointer.svg")
        sourceSize: Qt.size(40, 24)
        width: 40
        height: 24
        x: Math.max(Style.radiusDialog, Math.min(root.pointerX, root.width - Style.radiusDialog)) - width / 2
        y: root.pointerUp ? Style.heavyWidth - height : root.height - Style.heavyWidth
        rotation: root.pointerUp ? 0 : 180
    }
}
