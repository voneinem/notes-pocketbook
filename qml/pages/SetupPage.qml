import QtQuick
import QtQuick.Controls
import ".."

// First start: server address, user name and app password, a connection test, then Save.
Item {
    id: page

    property bool testing: bridge.busy
    property bool tested: false      // the last test succeeded for exactly these entries
    property string resultText: ""
    property bool resultOk: false
    property bool connectingWifi: false
    property bool showPassword: false
    function runTest() {
        resultText = ""
        bridge.testConnection(urlField.text, userField.text, passField.text)
    }

    function invalidateTest() { tested = false; resultText = "" }

    // Entries from the last attempt (kept even when the test failed): no retyping.
    Component.onCompleted: {
        var d = bridge.draft()
        if (d.url !== undefined) {
            urlField.text = d.url
            userField.text = d.username
            passField.text = d.password
        }
        tested = false
        resultText = ""
    }
    // Keep what was typed when the field is left or the page goes away (e.g. the app is closed).
    function keepEntries() { if (!bridge.configured) bridge.saveDraft(urlField.text, userField.text, passField.text) }
    Component.onDestruction: keepEntries()

    // The reader's keyboard covers the lower part of the screen: keep the active field visible.
    readonly property real keyboardHeight: Qt.inputMethod.visible ? Qt.inputMethod.keyboardRectangle.height * Screen.devicePixelRatio : 0
    function reveal(item) {
        var y = item.mapToItem(content, 0, 0).y
        var visibleHeight = flick.height - keyboardHeight
        if (y + item.height > flick.contentY + visibleHeight) flick.contentY = y + item.height - visibleHeight + 20
        else if (y < flick.contentY) flick.contentY = Math.max(0, y - 20)
    }
    onKeyboardHeightChanged: {
        if (urlField.activeFocus) reveal(urlField)
        else if (userField.activeFocus) reveal(userField)
        else if (passField.activeFocus) reveal(passField)
    }

    Connections {
        target: device
        function onNetworkResult(connected) {
            if (!page.connectingWifi) return
            page.connectingWifi = false
            if (connected) page.runTest()
            else { page.resultOk = false; page.resultText = bridge.message("wifi_off") }
        }
    }

    Connections {
        target: bridge
        function onConnectionTested(ok, messageKey, message, normalizedUrl) {
            page.resultOk = ok
            page.resultText = message
            page.tested = ok
            if (ok && normalizedUrl.length > 0) urlField.text = normalizedUrl
        }
        function onErrorOccurred(messageKey, message) {
            page.resultOk = false
            page.resultText = message
        }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: content.implicitHeight + 40 + page.keyboardHeight
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Column {
            id: content
            x: 40
            y: 30
            width: flick.width - 80
            spacing: 22

            Text {
                width: parent.width
                text: qsTr("Connect to Nextcloud")
                font.pixelSize: 54
                font.bold: true
                color: "#000000"
            }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: 28
                color: "#000000"
                text: qsTr("Enter the address of your Nextcloud server with the Notes app, your user name, and an app password (create one in Nextcloud under Settings, Security).")
            }

            Text { text: qsTr("Server address"); font.pixelSize: 28; font.bold: true; color: "#000000" }
            TextField {
                id: urlField
                width: parent.width
                font.pixelSize: 34
                placeholderText: "https://cloud.example.com"
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                color: "#000000"
                background: Rectangle { color: "#ffffff"; border.width: 2; border.color: "#111111" }
                onTextChanged: page.invalidateTest()
                onActiveFocusChanged: { if (activeFocus) page.reveal(urlField); else page.keepEntries() }
            }

            Text { text: qsTr("User name"); font.pixelSize: 28; font.bold: true; color: "#000000" }
            TextField {
                id: userField
                width: parent.width
                font.pixelSize: 34
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                color: "#000000"
                background: Rectangle { color: "#ffffff"; border.width: 2; border.color: "#111111" }
                onTextChanged: page.invalidateTest()
                onActiveFocusChanged: { if (activeFocus) page.reveal(userField); else page.keepEntries() }
            }

            Text { text: qsTr("App password"); font.pixelSize: 28; font.bold: true; color: "#000000" }
            Row {
                width: parent.width
                spacing: 12
                TextField {
                    id: passField
                    width: parent.width - showButton.width - parent.spacing
                    font.pixelSize: 34
                    echoMode: page.showPassword ? TextInput.Normal : TextInput.Password
                    inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText | Qt.ImhSensitiveData
                    color: "#000000"
                    background: Rectangle { color: "#ffffff"; border.width: 2; border.color: "#111111" }
                    onTextChanged: page.invalidateTest()
                    onActiveFocusChanged: { if (activeFocus) page.reveal(passField); else page.keepEntries() }
                }
                // Show the password in clear text to catch typos; hidden again by tapping once more.
                InkButton {
                    id: showButton
                    width: 150
                    height: passField.height
                    text: page.showPassword ? qsTr("Hide") : qsTr("Show")
                    onClicked: page.showPassword = !page.showPassword
                }
            }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: 24
                color: "#000000"
                text: qsTr("The password is stored on this reader without encryption. Use an app password, never your main password.")
            }

            Row {
                spacing: 24
                InkButton {
                    text: page.connectingWifi ? qsTr("Connecting to Wi-Fi...")
                                              : page.testing ? qsTr("Testing...") : qsTr("Test connection")
                    enabled: !page.testing && !page.connectingWifi
                    onClicked: {
                        Qt.inputMethod.hide()
                        if (device.wifiConnected()) { page.runTest(); return }
                        // Wi-Fi is off: ask the reader to connect first (it may show its own dialog)
                        page.resultText = ""
                        page.connectingWifi = true
                        device.connectNetwork()
                    }
                }
                InkButton {
                    text: qsTr("Save")
                    enabled: page.tested && !page.testing
                    onClicked: {
                        if (bridge.saveAccount(urlField.text, userField.text, passField.text))
                            passField.text = ""
                    }
                }
            }

            Text {
                width: parent.width
                visible: page.resultText.length > 0
                wrapMode: Text.Wrap
                font.pixelSize: 30
                font.bold: true
                color: "#000000"
                text: (page.resultOk ? "✓ " : "✗ ") + page.resultText
            }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: 22
                color: "#000000"
                text: qsTr("Trusted certificates: %1. Custom CA file: %2.")
                          .arg(bridge.caSource === "device" ? qsTr("the reader's own") : qsTr("shipped with the app"))
                          .arg(bridge.customCaStatus === "loaded" ? qsTr("loaded (%1)").arg(bridge.customCaCertificates)
                               : bridge.customCaStatus === "invalid" ? qsTr("invalid, ignored") : qsTr("none"))
            }
        }
    }
}
