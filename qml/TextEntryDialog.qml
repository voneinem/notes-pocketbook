import QtQuick

// A modal one-line text entry on top of the current page: a heading, a field, an error line and a footer with Cancel and OK,
// in the look of the device's own dialogs (spec 010). No animations (e-ink). The box sits in the upper part of the screen so
// the on-screen keyboard does not cover it.
// Usage: TextEntryDialog { id: d; title: ...; onAccepted: function (text) { ...validate...; d.close() or d.errorText = ... } },
// d.open("initial"). `accepted` leaves the dialog open: the caller judges the text and either closes it or sets errorText.
DialogFrame {
    id: root
    property string placeholder
    property string errorText
    property string acceptText: qsTr("OK")
    property string cancelText: qsTr("Cancel")
    signal accepted(string text)
    signal rejected()

    boxY: 120
    footer: [{ text: cancelText, role: "dismiss" }, { text: acceptText, role: "accept" }]

    function open(initial) {
        input.text = initial === undefined ? "" : initial
        errorText = ""
        visible = true
        input.forceFocus()
        Qt.inputMethod.show()
    }
    function close() {
        Qt.inputMethod.hide()
        visible = false
    }
    onCloseRequested: { close(); rejected() }
    onFooterClicked: function(role) {
        if (role === "accept") accepted(input.text)
        else { close(); rejected() }
    }

    InkField {
        id: input
        width: parent.width
        placeholderText: root.placeholder
        errorText: root.errorText
        onAccepted: root.accepted(text)   // Return = OK
        onTextChanged: root.errorText = ""
    }
}
