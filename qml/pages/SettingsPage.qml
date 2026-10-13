import QtQuick
import QtQuick.Controls
import ".."

// Certificates, screen rotation, "Auto Sync", the account (with "Forget account"), and About, in the look of the device's own
// settings (spec 010): a top bar with Back, captions in capitals, rows split by dotted dividers, a pill switch for Auto Sync.
Item {
    id: page

    property string message: ""

    Connections {
        target: bridge
        function onErrorOccurred(messageKey, text) { page.message = text }
    }

    TopBar {
        id: topBar
        title: qsTr("Settings")
        leftItems: [ IconButton { text: qsTr("Back"); icon: "chevron-left"; onClicked: page.StackView.view.pop() } ]
    }

    // a section caption
    component Caption: Text {
        width: parent.width
        leftPadding: 30
        topPadding: 24
        bottomPadding: 8
        font.pixelSize: Style.captionSize
        font.bold: true
        font.capitalization: Font.AllUppercase
        color: Style.ink
    }
    // a paragraph of explanation
    component Para: Text {
        width: parent.width
        leftPadding: 30
        rightPadding: 30
        wrapMode: Text.Wrap
        font.pixelSize: Style.labelSize
        color: Style.ink
    }

    Flickable {
        anchors { left: parent.left; right: parent.right; top: topBar.bottom; bottom: parent.bottom }
        contentWidth: width
        contentHeight: column.implicitHeight + 40
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Column {
            id: column
            width: parent.width
            spacing: 8

            Caption { text: qsTr("Certificates") }
            Para {
                text: qsTr("Connections are always verified. Trusted authorities: %1.")
                          .arg(bridge.caSource === "device" ? qsTr("the reader's own store") : qsTr("the list shipped with the app"))
            }
            Para {
                font.bold: true
                text: bridge.customCaStatus === "loaded" ? qsTr("Custom CA file: loaded (%1 certificate(s)).").arg(bridge.customCaCertificates)
                      : bridge.customCaStatus === "invalid" ? qsTr("Custom CA file: INVALID, it is ignored.")
                      : qsTr("Custom CA file: none.")
            }
            Para {
                font.pixelSize: Style.captionSize
                text: qsTr("For a private or self-signed certificate, copy its certificate file to %1 on the reader and restart the app.").arg(bridge.customCaPath)
            }
            RowDivider { x: 30; width: parent.width - 60 }

            Caption { text: qsTr("Screen rotation") }
            InkRow {
                text: qsTr("Screen rotation")
                value: bridge.rotationPreference === "auto" ? qsTr("Follow the sensor")
                       : bridge.rotationPreference === "portrait" ? qsTr("Portrait only")
                       : qsTr("Landscape only")
                chevron: true
                onClicked: bridge.rotationPreference = bridge.rotationPreference === "auto" ? "portrait"
                           : bridge.rotationPreference === "portrait" ? "landscape" : "auto"
            }

            Caption { text: qsTr("Synchronization") }
            Item {
                width: parent.width
                height: 110
                Text {
                    x: 30
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Auto Sync")
                    font.pixelSize: Style.bodySize
                    color: Style.ink
                }
                InkSwitch {
                    objectName: "autoSyncSwitch"
                    anchors { right: parent.right; rightMargin: 30; verticalCenter: parent.verticalCenter }
                    text: qsTr("Auto Sync")
                    checked: bridge.syncAfterSave
                    onToggled: function(on) { bridge.syncAfterSave = on }
                }
                RowDivider { anchors.bottom: parent.bottom; x: 30; width: parent.width - 60 }
            }
            Para {
                text: qsTr("When on, the reader syncs a few seconds after you save a change, switching Wi-Fi on if needed. When off, notes sync only with Sync now and at start.")
            }

            Caption { text: qsTr("Account") }
            Para {
                text: bridge.configured ? qsTr("%1 on %2").arg(bridge.username).arg(bridge.serverUrl) : qsTr("No account.")
            }
            InkRow {
                text: qsTr("Forget account")
                enabled: bridge.configured
                onClicked: confirmForget.open()
            }

            Caption { text: qsTr("About") }
            InkRow {
                text: qsTr("About this app")
                chevron: true
                onClicked: page.StackView.view.push(Qt.resolvedUrl("AboutPage.qml"))
            }

            Para {
                visible: page.message.length > 0
                font.bold: true
                text: "✗ " + page.message
            }
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
