import QtQuick

// A modal confirmation on top of the current page: a title, a text, and a footer with two buttons (the dismissing one
// first), in the look of the device's own dialogs (spec 010). No animations (e-ink).
// Usage: ConfirmDialog { id: d; title: ...; text: ...; onAccepted: ... }, d.open().
DialogFrame {
    id: root
    property string text
    property string acceptText: qsTr("OK")
    property string cancelText: qsTr("Cancel")
    property bool showCancel: true
    signal accepted()
    signal rejected()

    footer: showCancel ? [{ text: cancelText, role: "dismiss" }, { text: acceptText, role: "accept" }]
                       : [{ text: acceptText, role: "accept" }]
    onCloseRequested: { close(); rejected() }
    onFooterClicked: function(role) {
        close()
        if (role === "accept") accepted()
        else rejected()
    }

    Text {
        visible: root.text.length > 0
        width: parent.width
        text: root.text
        wrapMode: Text.Wrap
        font.pixelSize: Style.bodySize
        color: Style.ink
    }
}
