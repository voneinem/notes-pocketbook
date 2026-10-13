import QtQuick
import QtQuick.Controls
import ".."

// About: the app's name, version, author, copyright and license (spec 003). Read-only; nothing is sent anywhere.
// In the look of the device's own screens (spec 010): a top bar with Back, hairline rows, two tappable rows for the texts.
Item {
    id: page

    // An empty value (manifest missing or unreadable) is shown as "unavailable", never as a blank.
    function orUnavailable(value) { return value.length > 0 ? value : qsTr("unavailable") }

    TopBar {
        id: topBar
        title: qsTr("About")
        leftItems: [ IconButton { text: qsTr("Back"); icon: "chevron-left"; onClicked: page.StackView.view.pop() } ]
    }

    // one line of the app's details, closed by a dotted divider
    component Line: Item {
        property alias text: label.text
        property alias font: label.font
        width: parent.width
        height: label.implicitHeight + 40
        Text {
            id: label
            x: 30
            width: parent.width - 60
            anchors.verticalCenter: parent.verticalCenter
            wrapMode: Text.Wrap
            font.pixelSize: Style.bodySize
            color: Style.ink
        }
        RowDivider { anchors.bottom: parent.bottom; x: 30; width: parent.width - 60 }
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

            Line { text: about.appName; font.pixelSize: Style.rowTitleSize; font.bold: true }
            Line { text: qsTr("Version %1").arg(about.version) }
            Line { text: qsTr("Author: %1").arg(page.orUnavailable(about.author)) }
            Line { text: page.orUnavailable(about.copyright); font.pixelSize: Style.labelSize }
            Line { text: qsTr("License: %1").arg(page.orUnavailable(about.licenseName)) }

            InkRow {
                text: qsTr("License")
                chevron: true
                onClicked: page.StackView.view.push(Qt.resolvedUrl("LicenseTextPage.qml"), { content: about.appLicense() })
            }
            InkRow {
                text: qsTr("Third-party licenses")
                chevron: true
                onClicked: page.StackView.view.push(Qt.resolvedUrl("ThirdPartyPage.qml"))
            }
        }
    }
}
