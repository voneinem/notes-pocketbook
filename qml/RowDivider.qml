import QtQuick

// A dotted hairline across a row (spec 010): the separator of lists and choices in the device's own apps.
Item {
    height: 2
    Image {
        anchors.fill: parent
        source: Qt.resolvedUrl("icons/row-dots.svg")
        sourceSize: Qt.size(6, 2)
        fillMode: Image.Tile
        opacity: 0.55
    }
}
