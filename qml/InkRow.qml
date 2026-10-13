import QtQuick

// A row of a settings list (spec 010): a label at the left, an optional value and chevron at the right, a dotted divider
// underneath. A tap on it emits `clicked`; a disabled row is drawn lighter. Put a longer explanation under it as its own text.
Item {
    id: root
    property string text
    property string value
    property bool chevron: false
    signal clicked()

    implicitHeight: 110
    height: implicitHeight
    width: parent ? parent.width : 0
    opacity: enabled ? 1.0 : 0.4

    Rectangle { anchors.fill: parent; color: Style.pressed; visible: tap.pressed && root.enabled }
    Text {
        id: label
        x: 30
        anchors.verticalCenter: parent.verticalCenter
        text: root.text
        font.pixelSize: Style.bodySize
        color: Style.ink
    }
    Text {
        anchors { left: label.right; leftMargin: 20; right: chev.left; rightMargin: 10; verticalCenter: parent.verticalCenter }
        horizontalAlignment: Text.AlignRight
        elide: Text.ElideRight
        text: root.value
        font.pixelSize: Style.labelSize
        color: Style.ink
    }
    Icon {
        id: chev
        visible: root.chevron
        anchors { right: parent.right; rightMargin: 24; verticalCenter: parent.verticalCenter }
        width: visible ? 40 : 0
        size: 40
        source: root.chevron ? "chevron-right" : ""
    }
    RowDivider { objectName: "rowDivider"; anchors.bottom: parent.bottom; x: 30; width: parent.width - 60 }
    MouseArea { id: tap; anchors.fill: parent; enabled: root.enabled; onClicked: root.clicked() }
}
