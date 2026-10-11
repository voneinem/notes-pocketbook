import QtQuick
import QtQuick.Controls
import ".."

// About: the app's name, version, author, copyright and license (spec 003). Read-only; nothing is sent anywhere.
Item {
    id: page

    // An empty value (manifest missing or unreadable) is shown as "unavailable", never as a blank.
    function orUnavailable(value) { return value.length > 0 ? value : qsTr("unavailable") }

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

            Text { text: qsTr("About"); font.pixelSize: 54; font.bold: true; color: "#000000" }

            Text { text: about.appName; font.pixelSize: 44; font.bold: true; color: "#000000"; width: parent.width; wrapMode: Text.Wrap }
            Text { text: qsTr("Version %1").arg(about.version); font.pixelSize: 32; color: "#000000"; width: parent.width; wrapMode: Text.Wrap }
            Text { text: qsTr("Author: %1").arg(page.orUnavailable(about.author)); font.pixelSize: 32; color: "#000000"; width: parent.width; wrapMode: Text.Wrap }
            Text { text: page.orUnavailable(about.copyright); font.pixelSize: 28; color: "#000000"; width: parent.width; wrapMode: Text.Wrap }
            Text { text: qsTr("License: %1").arg(page.orUnavailable(about.licenseName)); font.pixelSize: 32; color: "#000000"; width: parent.width; wrapMode: Text.Wrap }

            InkButton {
                text: qsTr("License")
                onClicked: page.StackView.view.push(Qt.resolvedUrl("LicenseTextPage.qml"), { content: about.appLicense() })
            }

            InkButton {
                text: qsTr("Third-party licenses")
                onClicked: page.StackView.view.push(Qt.resolvedUrl("ThirdPartyPage.qml"))
            }

            InkButton { text: qsTr("Back"); onClicked: page.StackView.view.pop() }
        }
    }
}
