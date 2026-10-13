import QtQuick
import QtQuick.Controls
import ".."

// Third-party licenses (spec 003): every library, code base, font and data file the app uses, with its license.
// Rows have one fixed height so the page buttons move by whole rows; tapping a row opens its license text.
Item {
    id: page

    readonly property var rows: about.components

    TopBar {
        id: topBar
        title: qsTr("Third-party licenses")
        leftItems: [ IconButton { text: qsTr("Back"); icon: "chevron-left"; onClicked: page.StackView.view.pop() } ]
    }

    Text {
        id: unreadable
        anchors { left: parent.left; right: parent.right; top: topBar.bottom; leftMargin: 40; rightMargin: 40; topMargin: 8 }
        visible: about.unreadableEntries > 0
        height: visible ? implicitHeight : 0
        wrapMode: Text.Wrap
        font.pixelSize: 26
        font.bold: true
        color: "#000000"
        text: qsTr("%n entries could not be read.", "", about.unreadableEntries)
    }

    ListView {
        id: list
        anchors { left: parent.left; right: parent.right; top: unreadable.bottom; bottom: footer.top; topMargin: 12 }
        clip: true
        model: page.rows
        interactive: false
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: false

        delegate: Item {
            width: list.width
            height: 150
            readonly property var component: modelData

            Column {
                x: 40
                width: parent.width - 80
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                Text {
                    width: parent.width
                    text: component.name
                    font.pixelSize: 36
                    font.bold: true
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    color: "#000000"
                }
                Text {
                    width: parent.width
                    text: component.purpose
                    font.pixelSize: 26
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    color: "#000000"
                }
                Text {
                    width: parent.width
                    text: component.license + "  |  " + (component.origin === "bundled" ? qsTr("In the app package") : qsTr("Provided by the reader"))
                    font.pixelSize: 24
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    color: "#000000"
                }
            }
            RowDivider { anchors.bottom: parent.bottom; x: 40; width: parent.width - 80 }
            Rectangle { anchors.fill: parent; color: "#d8d8d8"; visible: tap.pressed; z: -1 }
            MouseArea {
                id: tap
                anchors.fill: parent
                onClicked: page.StackView.view.push(Qt.resolvedUrl("LicenseTextPage.qml"), { content: about.componentDetail(component.id) })
            }
        }
    }

    Text {
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 40 }
        visible: page.rows.length === 0
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: 32
        color: "#000000"
        text: qsTr("The list of third-party licenses is unavailable.")
    }

    Item {
        id: footer
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: Style.touchMin
        PageButtons { anchors.centerIn: parent; target: list; snap: 150 }
    }
}
