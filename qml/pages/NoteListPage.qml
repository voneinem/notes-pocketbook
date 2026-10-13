import QtQuick
import QtQuick.Controls
import ".."

// The note list in the look of the reader's own Notes app (spec 010): a top bar (home, menu, NOTES, search, new), a row for
// the view and the sort order with the layout toggle, a count line with the sync state, then the notes as rows or as cards.
// Scrolling is by page buttons (no kinetic scrolling).
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
    readonly property bool tiles: bridge.listLayout === "tile"
    function countText() {
        var n = rows.length
        return n === 1 ? qsTr("1 note") : qsTr("%1 notes").arg(n)
    }

    property bool searchOpen: false
    function openSearch() {
        searchOpen = true
        searchField.forceActiveFocus()
    }
    function closeSearch() {
        searchTimer.stop()
        searchField.text = ""
        bridge.setSearch("")
        Qt.inputMethod.hide()
        searchOpen = false
    }

    // "Sync now" is a deliberate request: if Wi-Fi is off, ask the reader to connect first.
    property bool connectingWifi: false
    property string wifiMessage: ""
    function syncWithWifi() {
        if (bridge.syncActive || connectingWifi) return
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

    function openNote(id) { page.StackView.view.push(Qt.resolvedUrl("ReaderPage.qml"), { noteId: id }) }

    // ---- the top bar ----
    TopBar {
        id: topBar
        title: qsTr("Notes")
        leftItems: [
            IconButton { text: qsTr("Home"); icon: "home"; onClicked: device.quit() },
            IconButton {
                id: menuButton
                text: qsTr("Menu"); icon: "menu"
                onClicked: { menuDialog.choices = [{ label: qsTr("Sync now"), value: "sync" }, { label: qsTr("Settings"), value: "settings" }]; menuDialog.open() }
            }
        ]
        rightItems: [
            IconButton { text: qsTr("Search"); icon: "search"; onClicked: page.searchOpen ? page.closeSearch() : page.openSearch() },
            IconButton {
                text: qsTr("New note"); icon: "note-plus"
                onClicked: page.StackView.view.push(Qt.resolvedUrl("EditorPage.qml"), { noteId: -1 })
            }
        ]
    }

    // ---- the search row (opens from the bar) ----
    Item {
        id: searchRow
        anchors { top: topBar.bottom; left: parent.left; right: parent.right }
        height: page.searchOpen ? Style.filterRowHeight : 0
        visible: page.searchOpen
        TextField {
            id: searchField
            objectName: "searchField"
            x: 30
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 60 - closeSearchButton.width - 12
            height: 78
            font.pixelSize: Style.bodySize
            color: Style.ink
            placeholderText: qsTr("Search")
            inputMethodHints: Qt.ImhNoPredictiveText
            verticalAlignment: TextInput.AlignVCenter
            background: Rectangle { color: Style.paper; radius: Style.radiusField; border.width: 2; border.color: Style.ink }
            onTextEdited: searchTimer.restart()
            onAccepted: { searchTimer.stop(); bridge.setSearch(text); Qt.inputMethod.hide() }
            Timer { id: searchTimer; interval: 500; onTriggered: bridge.setSearch(searchField.text) }
        }
        IconButton {
            id: closeSearchButton
            anchors { right: parent.right; rightMargin: 14; verticalCenter: parent.verticalCenter }
            text: qsTr("Close search"); icon: "close"
            onClicked: page.closeSearch()
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: Style.hairlineWidth; color: Style.hairline }
    }

    // ---- the view and sort row, with the layout toggle ----
    Item {
        id: filterRow
        anchors { top: searchRow.bottom; left: parent.left; right: parent.right }
        height: Style.filterRowHeight
        Row {
            x: 30
            height: parent.height
            spacing: 56
            Item {
                id: viewSelector
                objectName: "viewSelector"
                height: parent.height
                width: viewRow.width
                Row {
                    id: viewRow
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 12
                    Icon { anchors.verticalCenter: parent.verticalCenter; source: "filter"; size: 48 }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: page.viewLabel(bridge.view)
                        font.pixelSize: Style.labelSize
                        color: Style.ink
                        elide: Text.ElideRight
                        width: Math.min(implicitWidth, 360)
                    }
                    Icon { anchors.verticalCenter: parent.verticalCenter; source: "chevron-down"; size: 32 }
                }
                MouseArea { anchors.fill: parent; onClicked: { viewDialog.choices = page.viewChoices(); viewDialog.open() } }
            }
            Item {
                id: sortSelector
                objectName: "sortSelector"
                height: parent.height
                width: sortRow.width
                Row {
                    id: sortRow
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 12
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: bridge.sortOrder === "title" ? qsTr("A–Z") : qsTr("Recently changed")
                        font.pixelSize: Style.labelSize
                        color: Style.ink
                    }
                    Icon { anchors.verticalCenter: parent.verticalCenter; source: "chevron-down"; size: 32 }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        sortDialog.choices = [{ label: qsTr("Recently changed"), value: "modified" }, { label: qsTr("A\u2013Z"), value: "title" }]
                        sortDialog.open()
                    }
                }
            }
        }
        IconButton {
            id: layoutToggle
            objectName: "layoutToggle"
            anchors { right: parent.right; rightMargin: 14; verticalCenter: parent.verticalCenter }
            text: qsTr("Layout")
            // shows the layout a tap switches to
            icon: page.tiles ? "list" : "grid"
            onClicked: bridge.listLayout = page.tiles ? "list" : "tile"
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: Style.hairlineWidth; color: Style.hairline }
    }

    // ---- the count line and the sync state ----
    Item {
        id: countRow
        anchors { top: filterRow.bottom; left: parent.left; right: parent.right }
        height: Style.countRowHeight
        Text {
            id: countLabel
            x: 30
            anchors.verticalCenter: parent.verticalCenter
            text: page.countText()
            font.pixelSize: Style.captionSize
            font.capitalization: Font.AllUppercase
            color: Style.ink
        }
        Text {
            anchors { left: countLabel.right; leftMargin: 24; right: parent.right; rightMargin: 30; verticalCenter: parent.verticalCenter }
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
            font.pixelSize: Style.captionSize
            color: Style.ink
            text: (page.wifiMessage.length > 0 ? page.wifiMessage : bridge.syncStatus)
                  + (bridge.pendingCount > 0 ? qsTr("  |  %1 change(s) not synced yet").arg(bridge.pendingCount) : "")
        }
    }

    // ---- the notes as rows ----
    ListView {
        id: list
        objectName: "noteList"
        anchors { left: parent.left; right: parent.right; top: countRow.bottom; bottom: footer.top }
        visible: !page.tiles
        clip: true
        model: page.rows
        interactive: false
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: false

        delegate: Item {
            width: list.width
            height: Style.rowHeight
            readonly property var note: modelData

            Column {
                x: 30
                width: parent.width - 60
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6
                Row {
                    width: parent.width
                    spacing: 12
                    Icon { visible: note.favorite; width: visible ? 40 : 0; size: 40; source: "star-filled"; anchors.verticalCenter: parent.verticalCenter }
                    Text {
                        width: parent.width - (note.favorite ? 52 : 0)
                        anchors.verticalCenter: parent.verticalCenter
                        text: note.title
                        font.pixelSize: Style.rowTitleSize
                        font.bold: true
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        color: Style.ink
                    }
                }
                Text {
                    width: parent.width
                    elide: Text.ElideRight
                    font.pixelSize: Style.captionSize
                    color: Style.ink
                    text: (note.category.length > 0 ? note.category + "  •  " : "")
                          + bridge.formatTime(note.modified)
                          + (note.pending ? "  •  " + qsTr("not synced yet") : "")
                }
            }
            RowDivider { anchors.bottom: parent.bottom; x: 30; width: parent.width - 60 }
            Rectangle { anchors.fill: parent; color: Style.pressed; visible: tap.pressed; z: -1 }
            MouseArea { id: tap; anchors.fill: parent; onClicked: page.openNote(note.localId) }
        }
    }

    // ---- the notes as cards ----
    GridView {
        id: grid
        objectName: "noteGrid"
        anchors { left: parent.left; right: parent.right; top: countRow.bottom; bottom: footer.top }
        visible: page.tiles
        clip: true
        model: page.rows
        interactive: false
        boundsBehavior: Flickable.StopAtBounds
        readonly property int columns: Math.max(2, Math.floor(width / Style.tileMinWidth))
        cellWidth: width / columns
        cellHeight: Style.tileHeight

        delegate: Item {
            width: grid.cellWidth
            height: grid.cellHeight
            readonly property var note: modelData

            Rectangle {
                anchors { fill: parent; margins: 8 }
                radius: Style.radiusField
                color: tapCard.pressed ? Style.pressed : Style.paper
                border.width: 2
                border.color: Style.ink
                Column {
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
                    spacing: 6
                    Text {
                        width: parent.width - (note.favorite ? 48 : 0)
                        text: note.title
                        font.pixelSize: Style.bodySize
                        font.bold: true
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        color: Style.ink
                    }
                    Text {
                        width: parent.width
                        elide: Text.ElideRight
                        font.pixelSize: Style.captionSize
                        color: Style.ink
                        text: (note.category.length > 0 ? note.category + "  •  " : "") + bridge.formatTime(note.modified)
                    }
                    Text {
                        visible: note.pending
                        width: parent.width
                        elide: Text.ElideRight
                        font.pixelSize: Style.captionSize
                        color: Style.ink
                        text: qsTr("not synced yet")
                    }
                }
                Icon { visible: note.favorite; anchors { right: parent.right; top: parent.top; margins: 12 } size: 36; source: "star-filled" }
                MouseArea { id: tapCard; anchors.fill: parent; onClicked: page.openNote(note.localId) }
            }
        }
    }

    Text {
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 40 }
        visible: page.rows.length === 0
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: Style.bodySize
        color: Style.ink
        text: bridge.syncRunning ? qsTr("Loading your notes...")
              : bridge.syncOutcome === "never" ? qsTr("No notes yet. Open the menu and choose Sync now to load your notes.")
              : (bridge.searchText.length > 0 || bridge.view !== "all") ? qsTr("No notes match. Change the view or clear the search.")
              : qsTr("No notes to show.")
    }

    Item {
        id: footer
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: Style.touchMin
        PageButtons {
            anchors.centerIn: parent
            target: page.tiles ? grid : list
            snap: page.tiles ? grid.cellHeight : Style.rowHeight
        }
    }

    ChoiceDialog {
        id: viewDialog
        title: qsTr("Show notes")
        anchor: viewSelector
        current: bridge.view
        onChosen: function (value) { bridge.setView(value) }
    }
    ChoiceDialog {
        id: sortDialog
        title: qsTr("Sort")
        anchor: sortSelector
        current: bridge.sortOrder
        onChosen: function (value) { bridge.setSortOrder(value) }
    }
    ChoiceDialog {
        id: menuDialog
        title: qsTr("Menu")
        anchor: menuButton
        current: ""
        onChosen: function (value) {
            if (value === "sync") page.syncWithWifi()
            else if (value === "settings") page.StackView.view.push(Qt.resolvedUrl("SettingsPage.qml"))
        }
    }
}
