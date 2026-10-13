import QtQuick
import QtQuick.Controls

// A labelled text field in the device's style (spec 010): the label above, a 2 px rounded outline, an error line underneath.
Column {
    id: root
    objectName: "inkField"
    property string label
    property alias text: field.text
    property alias placeholderText: field.placeholderText
    property alias field: field
    property string errorText
    signal accepted()
    signal textEdited()

    spacing: 10

    function forceFocus() { field.forceActiveFocus() }

    Text {
        visible: root.label.length > 0
        width: parent.width
        text: root.label
        font.pixelSize: Style.labelSize
        color: Style.ink
    }
    TextField {
        id: field
        width: parent.width
        height: 78
        font.pixelSize: Style.bodySize
        color: Style.ink
        inputMethodHints: Qt.ImhNoPredictiveText
        verticalAlignment: TextInput.AlignVCenter
        background: Rectangle { color: Style.paper; radius: Style.radiusField; border.width: Style.ruleWidth; border.color: Style.ink }
        onAccepted: root.accepted()
        onTextEdited: root.textEdited()
    }
    Text {
        visible: root.errorText.length > 0
        width: parent.width
        text: "✗ " + root.errorText
        wrapMode: Text.Wrap
        font.pixelSize: Style.captionSize
        font.bold: true
        color: Style.ink
    }
}
