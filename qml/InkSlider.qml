import QtQuick

// A slider with stops in the device's style (spec 010): round minus and plus buttons at the ends move one stop, a hairline
// track with a dot at each end carries a ring knob, a tap on the track moves to the nearest stop, and `label` (a size with its
// unit) sits at the right. It only reports (`moved(index)`): the owner sets `index`.
Item {
    id: root
    property int stops: 3
    property int index: 0
    property string label
    signal moved(int index)

    readonly property real knob: 76
    height: Style.touchMin

    IconButton {
        id: minus
        x: 0
        anchors.verticalCenter: parent.verticalCenter
        text: qsTr("Decrease"); icon: "minus-circle"; iconSize: 80
        enabled: root.index > 0
        onClicked: root.moved(root.index - 1)
    }
    IconButton {
        id: plus
        anchors { right: valueLabel.left; rightMargin: 4; verticalCenter: parent.verticalCenter }
        text: qsTr("Increase"); icon: "plus-circle"; iconSize: 80
        enabled: root.index < root.stops - 1
        onClicked: root.moved(root.index + 1)
    }
    Text {
        id: valueLabel
        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
        width: 130
        horizontalAlignment: Text.AlignRight
        text: root.label
        font.pixelSize: Style.bodySize
        color: Style.ink
    }
    // the track runs between the buttons, with room for the knob at both ends
    Item {
        id: track
        objectName: "sliderTrack"
        x: minus.width + 20 + root.knob / 2
        width: plus.x - 20 - root.knob / 2 - x
        anchors.verticalCenter: parent.verticalCenter
        height: Style.touchMin

        Rectangle { anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter } height: Style.hairlineWidth; color: Style.ink }
        Rectangle { x: -6; anchors.verticalCenter: parent.verticalCenter; width: 12; height: 12; radius: 6; color: Style.ink }
        Rectangle { x: parent.width - 6; anchors.verticalCenter: parent.verticalCenter; width: 12; height: 12; radius: 6; color: Style.ink }
        Rectangle {
            id: knobItem
            objectName: "sliderKnob"
            width: root.knob; height: root.knob; radius: root.knob / 2
            anchors.verticalCenter: parent.verticalCenter
            x: (root.stops > 1 ? track.width * root.index / (root.stops - 1) : 0) - width / 2
            color: Style.paper
            border.width: 6
            border.color: Style.ink
        }
        // a tap anywhere on the track (and on the knob's overhang) picks the nearest stop
        MouseArea {
            x: -root.knob / 2
            width: parent.width + root.knob
            height: parent.height
            onClicked: function(mouse) {
                if (root.stops < 2) return
                var f = (mouse.x - root.knob / 2) / track.width
                var i = Math.max(0, Math.min(root.stops - 1, Math.round(f * (root.stops - 1))))
                if (i !== root.index) root.moved(i)
            }
        }
    }
}
