import QtQuick
import QtQuick.Controls

// A modal list of choices on top of the current page (the view picker). `choices` is a list of
// {label, value}; the row whose value equals `current` is marked. Paged with buttons, no animations.
Item {
    id: root
    property string title
    property var choices: []
    property string current
    signal chosen(string value)

    anchors.fill: parent
    visible: false
    z: 1000

    function open() { visible = true }
    function close() { visible = false }

    MouseArea { anchors.fill: parent }
    Rectangle { anchors.fill: parent; color: "#ffffff" }

    Text {
        id: heading
        x: 30; y: 20
        width: parent.width - 60
        text: root.title
        font.pixelSize: 44
        font.bold: true
        color: "#000000"
    }
    Rectangle { id: rule; anchors.top: heading.bottom; anchors.topMargin: 12; width: parent.width; height: 2; color: "#111111" }

    ListView {
        id: list
        anchors { left: parent.left; right: parent.right; top: rule.bottom; bottom: footer.top; bottomMargin: 8 }
        clip: true
        interactive: false
        model: root.choices
        delegate: Item {
            width: list.width
            height: 96
            readonly property var choice: modelData
            Text {
                x: 30
                width: parent.width - 60
                anchors.verticalCenter: parent.verticalCenter
                elide: Text.ElideRight
                font.pixelSize: 36
                font.bold: choice.value === root.current
                color: "#000000"
                text: (choice.value === root.current ? "✓ " : "    ") + choice.label
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#808080" }
            Rectangle { anchors.fill: parent; color: "#d8d8d8"; visible: tap.pressed; z: -1 }
            MouseArea {
                id: tap
                anchors.fill: parent
                onClicked: { root.close(); root.chosen(choice.value) }
            }
        }
    }

    Row {
        id: footer
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 16 }
        spacing: 24
        PageButtons { target: list; snap: 96 }
        InkButton { text: qsTr("Cancel"); onClicked: root.close() }
    }
}
