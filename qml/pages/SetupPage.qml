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
        if (urlField.field.activeFocus) reveal(urlField)
        else if (userField.field.activeFocus) reveal(userField)
        else if (passField.field.activeFocus) reveal(passField)
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

    // the same top bar as every screen; this is the first screen, so there is nowhere to go back to
    TopBar { id: topBar; title: qsTr("Set up") }

    Flickable {
        id: flick
        anchors { left: parent.left; right: parent.right; top: topBar.bottom; bottom: footerBar.top }
        contentWidth: width
        contentHeight: content.implicitHeight + 40
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Column {
            id: content
            x: 30
            y: 24
            width: flick.width - 60
            spacing: 22

            Text {
                width: parent.width
                text: qsTr("Connect to Nextcloud")
                font.pixelSize: Style.titleSize
                font.bold: true
                color: Style.ink
            }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: Style.labelSize
                color: Style.ink
                text: qsTr("Enter the address of your Nextcloud server with the Notes app, your user name, and an app password (create one in Nextcloud under Settings, Security).")
            }

            InkField {
                id: urlField
                objectName: "urlField"
                width: parent.width
                label: qsTr("Server address")
                placeholderText: "https://cloud.example.com"
                field.inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                onTextChanged: page.invalidateTest()
                Connections {
                    target: urlField.field
                    function onActiveFocusChanged() { if (urlField.field.activeFocus) page.reveal(urlField); else page.keepEntries() }
                }
            }

            InkField {
                id: userField
                objectName: "userField"
                width: parent.width
                label: qsTr("User name")
                field.inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                onTextChanged: page.invalidateTest()
                Connections {
                    target: userField.field
                    function onActiveFocusChanged() { if (userField.field.activeFocus) page.reveal(userField); else page.keepEntries() }
                }
            }

            Item {
                width: parent.width
                height: passField.height
                InkField {
                    id: passField
                    objectName: "passField"
                    width: parent.width
                    label: qsTr("App password")
                    field.echoMode: page.showPassword ? TextInput.Normal : TextInput.Password
                    field.inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText | Qt.ImhSensitiveData
                    field.rightPadding: 110
                    onTextChanged: page.invalidateTest()
                    Connections {
                        target: passField.field
                        function onActiveFocusChanged() { if (passField.field.activeFocus) page.reveal(passField); else page.keepEntries() }
                    }
                }
                // Show the password in clear text to catch typos; hidden again by tapping once more.
                IconButton {
                    anchors { right: parent.right; rightMargin: 4; bottom: parent.bottom; bottomMargin: -9 }
                    text: page.showPassword ? qsTr("Hide password") : qsTr("Show password")
                    icon: "eye"
                    toggled: page.showPassword
                    onClicked: page.showPassword = !page.showPassword
                }
            }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: Style.captionSize
                color: Style.ink
                text: qsTr("The password is stored on this reader without encryption. Use an app password, never your main password.")
            }

            Text {
                width: parent.width
                visible: page.resultText.length > 0
                wrapMode: Text.Wrap
                font.pixelSize: Style.labelSize
                font.bold: true
                color: Style.ink
                text: (page.resultOk ? "\u2713 " : "\u2717 ") + page.resultText
            }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: Style.captionSize
                color: Style.ink
                text: qsTr("Trusted certificates: %1. Custom CA file: %2.")
                          .arg(bridge.caSource === "device" ? qsTr("the reader's own") : qsTr("shipped with the app"))
                          .arg(bridge.customCaStatus === "loaded" ? qsTr("loaded (%1)").arg(bridge.customCaCertificates)
                               : bridge.customCaStatus === "invalid" ? qsTr("invalid, ignored") : qsTr("none"))
            }
        }
    }

    // the footer: Test connection and Save, directly above the keyboard
    Item {
        id: footerBar
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; bottomMargin: page.keyboardHeight }
        height: Style.footerHeight + Style.hairlineWidth
        Rectangle { width: parent.width; height: Style.hairlineWidth; color: Style.ink }
        FooterButton {
            y: Style.hairlineWidth
            width: parent.width / 2
            text: page.connectingWifi ? qsTr("Connecting to Wi-Fi...") : page.testing ? qsTr("Testing...") : qsTr("Test connection")
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
        Rectangle { x: parent.width / 2; y: Style.hairlineWidth; width: Style.hairlineWidth; height: Style.footerHeight; color: Style.ink }
        FooterButton {
            x: parent.width / 2
            y: Style.hairlineWidth
            width: parent.width / 2
            text: qsTr("Save")
            enabled: page.tested && !page.testing
            onClicked: {
                if (bridge.saveAccount(urlField.text, userField.text, passField.text))
                    passField.text = ""
            }
        }
    }
}
