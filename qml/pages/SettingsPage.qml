import QtQuick
import QtQuick.Controls
import ".."

// Certificates, screen rotation, and the account (with "Forget account").
Item {
    id: page

    property string message: ""

    Connections {
        target: bridge
        function onErrorOccurred(messageKey, text) { page.message = text }
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight + 80
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Column {
            id: column
            x: 40
            y: 30
            width: parent.width - 80
            spacing: 22

            Text { text: qsTr("Settings"); font.pixelSize: 54; font.bold: true; color: "#000000" }

            Text { text: qsTr("Certificates"); font.pixelSize: 36; font.bold: true; color: "#000000" }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: 28
                color: "#000000"
                text: qsTr("Connections are always verified. Trusted authorities: %1.")
                          .arg(bridge.caSource === "device" ? qsTr("the reader's own store") : qsTr("the list shipped with the app"))
            }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: 28
                font.bold: true
                color: "#000000"
                text: bridge.customCaStatus === "loaded" ? qsTr("Custom CA file: loaded (%1 certificate(s)).").arg(bridge.customCaCertificates)
                      : bridge.customCaStatus === "invalid" ? qsTr("Custom CA file: INVALID, it is ignored.")
                      : qsTr("Custom CA file: none.")
            }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: 24
                color: "#000000"
                text: qsTr("For a private or self-signed certificate, copy its certificate file to %1 on the reader and restart the app.").arg(bridge.customCaPath)
            }

            Text { text: qsTr("Screen rotation"); font.pixelSize: 36; font.bold: true; color: "#000000" }
            InkButton {
                text: bridge.rotationPreference === "auto" ? qsTr("Rotation: follow the sensor")
                      : bridge.rotationPreference === "portrait" ? qsTr("Rotation: portrait only")
                      : qsTr("Rotation: landscape only")
                onClicked: bridge.rotationPreference = bridge.rotationPreference === "auto" ? "portrait"
                           : bridge.rotationPreference === "portrait" ? "landscape" : "auto"
            }

            Text { text: qsTr("Account"); font.pixelSize: 36; font.bold: true; color: "#000000" }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: 28
                color: "#000000"
                text: bridge.configured ? qsTr("%1 on %2").arg(bridge.username).arg(bridge.serverUrl) : qsTr("No account.")
            }
            InkButton {
                text: qsTr("Forget account")
                enabled: bridge.configured
                onClicked: confirmForget.open()
            }
            Text {
                width: parent.width
                visible: page.message.length > 0
                wrapMode: Text.Wrap
                font.pixelSize: 28
                font.bold: true
                color: "#000000"
                text: "✗ " + page.message
            }

            InkButton { text: qsTr("Back"); onClicked: page.StackView.view.pop() }
        }
    }

    ConfirmDialog {
        id: confirmForget
        title: qsTr("Forget this account?")
        text: qsTr("The saved login and the notes stored on this reader are removed. Your notes on the server are not touched. This is only possible when everything is synced.")
        acceptText: qsTr("Forget")
        onAccepted: { page.message = ""; bridge.forgetAccount() }
    }
}
