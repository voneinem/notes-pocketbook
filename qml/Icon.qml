import QtQuick

// A line-art icon from qml/icons/<source>.svg, drawn at `size` x `size` panel pixels. An empty or missing name draws
// nothing and `ready` stays false: the control that owns the icon then shows its label instead (spec 010 R-1).
Item {
    id: root
    property string source
    property int size: Style.iconTop
    readonly property bool ready: img.status === Image.Ready

    width: size
    height: size

    Image {
        id: img
        anchors.fill: parent
        source: root.source.length > 0 ? Qt.resolvedUrl("icons/" + root.source + ".svg") : ""
        sourceSize: Qt.size(root.size, root.size)
        smooth: true
        cache: true
    }
}
