import QtQuick

// The bar at the top of every screen (spec 010): items at the left, the title centred in capitals, items at the right,
// a hairline underneath. Starts below `Style.topInset` (the system's own status bar, when it overlays the app).
Item {
    id: root
    property string title
    property alias leftItems: leftRow.data
    property alias rightItems: rightRow.data

    width: parent ? parent.width : 0
    height: Style.barHeight + Style.topInset

    Row {
        id: leftRow
        x: 14
        y: Style.topInset
        height: Style.barHeight
        topPadding: (Style.barHeight - Style.touchMin) / 2
        spacing: 6
    }
    Row {
        id: rightRow
        x: root.width - width - 14
        y: Style.topInset
        height: Style.barHeight
        topPadding: (Style.barHeight - Style.touchMin) / 2
        spacing: 6
    }
    Text {
        objectName: "topBarTitle"
        anchors.horizontalCenter: parent.horizontalCenter
        y: Style.topInset
        height: Style.barHeight
        width: Math.max(0, root.width - 2 * Math.max(leftRow.width, rightRow.width) - 40)
        text: root.title
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: Style.titleSize
        font.capitalization: Font.AllUppercase
        color: Style.ink
    }
    Rectangle {
        objectName: "topBarRule"
        anchors.bottom: parent.bottom
        width: parent.width
        height: Style.hairlineWidth
        color: Style.hairline
    }
}
