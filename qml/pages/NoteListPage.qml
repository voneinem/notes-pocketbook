import QtQuick
import QtQuick.Controls
import ".."

// The note list: one row per note (title, category, modification date, favorite marker), the sync
// status line with "Sync now", and Settings. Scrolling is by page buttons (no kinetic scrolling).
Item {
    id: page

    property var rows: bridge.listNotes()
    function reload() { rows = bridge.listNotes() }

    // ---- view picker, sort order, search ----
    function viewLabel(v) {
        if (v === "all") return qsTr("All")
        if (v === "favorites") return qsTr("Favorites")
        if (v === "uncategorized") return qsTr("Uncategorized")
        return v.substring(9)
    }
    function viewChoices() {
        var out = [{ label: qsTr("All"), value: "all" },
                   { label: qsTr("Favorites"), value: "favorites" },
                   { label: qsTr("Uncategorized"), value: "uncategorized" }]
        var cats = bridge.categories
        for (var i = 0; i < cats.length; ++i)
            out.push({ label: cats[i].name + "  (" + cats[i].count + ")", value: "category:" + cats[i].name })
        return out
    }

    // "Sync now" is a deliberate request: if Wi-Fi is off, ask the reader to connect first.
    property bool connectingWifi: false
    property string wifiMessage: ""
    function syncWithWifi() {
        if (bridge.syncRunning || connectingWifi) return
        wifiMessage = ""
        if (device.wifiConnected()) { bridge.syncNow(); return }
        connectingWifi = true
        device.connectNetwork()
    }
    Connections {
        target: device
        function onNetworkResult(connected) {
            if (!page.connectingWifi) return
            page.connectingWifi = false
            if (connected) bridge.syncNow()
            else page.wifiMessage = bridge.message("wifi_off")
        }
    }

    StackView.onActivated: reload()
    Component.onCompleted: {
        // Sync at start, but only if Wi-Fi is already up: starting the app offline is not an error and
        // never switches Wi-Fi on (that is what "Sync now" is for).
        if (device.wifiConnected()) bridge.syncNow()
    }

    Connections {
        target: bridge
        function onNotesChanged() { page.reload() }
    }

    Column {
        id: header
        x: 30; y: 20
        width: parent.width - 60
        spacing: 12

        Row {
            width: parent.width
            spacing: 20
            Text {
                id: titleText
                text: qsTr("Notes")
                font.pixelSize: 54
                font.bold: true
                color: "#000000"
                anchors.verticalCenter: parent.verticalCenter
            }
            Item { width: Math.max(0, parent.width - titleText.width - newButton.width - syncButton.width - settingsButton.width - 80); height: 1 }
            InkButton {
                id: newButton
                text: qsTr("New")
                onClicked: page.StackView.view.push(Qt.resolvedUrl("EditorPage.qml"), { noteId: -1 })
            }
            InkButton {
                id: syncButton
                text: page.connectingWifi ? qsTr("Connecting...") : bridge.syncRunning ? qsTr("Syncing...") : qsTr("Sync now")
                enabled: !bridge.syncRunning && !page.connectingWifi
                onClicked: page.syncWithWifi()
            }
            InkButton { id: settingsButton; text: qsTr("Settings"); onClicked: page.StackView.view.push(Qt.resolvedUrl("SettingsPage.qml")) }
        }
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            font.pixelSize: 24
            color: "#000000"
            text: (page.wifiMessage.length > 0 ? page.wifiMessage : bridge.syncStatus)
                  + (bridge.pendingCount > 0 ? qsTr("  |  %1 change(s) not synced yet").arg(bridge.pendingCount) : "")
        }
        Row {
            id: tools
            width: parent.width
            spacing: 12
            InkButton {
                id: viewButton
                text: qsTr("View: %1").arg(page.viewLabel(bridge.view))
                onClicked: { viewDialog.choices = page.viewChoices(); viewDialog.open() }
            }
            InkButton {
                id: sortButton
                text: bridge.sortOrder === "title" ? qsTr("Sort: A-Z") : qsTr("Sort: newest")
                onClicked: bridge.setSortOrder(bridge.sortOrder === "title" ? "modified" : "title")
            }
            TextField {
                id: searchField
                width: parent.width - viewButton.width - sortButton.width - clearButton.width - 3 * parent.spacing
                height: viewButton.height
                font.pixelSize: 30
                color: "#000000"
                placeholderText: qsTr("Search")
                inputMethodHints: Qt.ImhNoPredictiveText
                verticalAlignment: TextInput.AlignVCenter
                background: Rectangle { color: "#ffffff"; border.width: 2; border.color: "#111111" }
                onTextEdited: searchTimer.restart()
                onAccepted: { searchTimer.stop(); bridge.setSearch(text); Qt.inputMethod.hide() }
                Timer { id: searchTimer; interval: 500; onTriggered: bridge.setSearch(searchField.text) }
            }
            InkButton {
                id: clearButton
                text: qsTr("Clear")
                enabled: searchField.text.length > 0
                onClicked: { searchTimer.stop(); searchField.text = ""; bridge.setSearch(""); Qt.inputMethod.hide() }
            }
        }
        Rectangle { width: parent.width; height: 2; color: "#111111" }
    }

    ListView {
        id: list
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: footer.top; topMargin: 4 }
        clip: true
        model: page.rows
        interactive: false
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: false

        delegate: Item {
            width: list.width
            height: 132
            readonly property var note: modelData

            Column {
                x: 30
                width: parent.width - 60
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6
                Row {
                    width: parent.width
                    spacing: 12
                    Text {
                        text: note.favorite ? "★" : ""
                        visible: note.favorite
                        font.pixelSize: 36
                        color: "#000000"
                    }
                    Text {
                        width: parent.width - (note.favorite ? 50 : 0)
                        text: note.title
                        font.pixelSize: 38
                        font.bold: true
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        color: "#000000"
                    }
                }
                Text {
                    width: parent.width
                    elide: Text.ElideRight
                    font.pixelSize: 26
                    color: "#000000"
                    text: (note.category.length > 0 ? note.category + "  |  " : "")
                          + bridge.formatTime(note.modified)
                          + (note.pending ? qsTr("  |  not synced yet") : "")
                }
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#808080" }
            Rectangle { anchors.fill: parent; color: "#d8d8d8"; visible: tap.pressed; z: -1 }
            MouseArea {
                id: tap
                anchors.fill: parent
                onClicked: page.StackView.view.push(Qt.resolvedUrl("ReaderPage.qml"), { noteId: note.localId })
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
        text: bridge.syncRunning ? qsTr("Loading your notes...")
              : bridge.syncOutcome === "never" ? qsTr("No notes yet. Tap Sync now to load your notes.")
              : (bridge.searchText.length > 0 || bridge.view !== "all") ? qsTr("No notes match. Change the view or clear the search.")
              : qsTr("No notes to show.")
    }

    Row {
        id: footer
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 16 }
        spacing: 24
        PageButtons { target: list; snap: 132 }
        InkButton { text: qsTr("Quit"); onClicked: device.quit() }
    }

    ChoiceDialog {
        id: viewDialog
        title: qsTr("Show notes")
        current: bridge.view
        onChosen: function (value) { bridge.setView(value) }
    }
}
