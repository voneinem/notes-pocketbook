import QtQuick
import QtQuick.Controls
import ".."

// A small file browser for the attach button: a top bar (back, ATTACH, Up and the layout toggle as icons), the current folder's
// path, folders and files as big rows
// (whole rows per page, no kinetic scrolling). Tapping a file shows its name and size and asks before it is
// chosen; a file over the size limit is refused with a message first. Emits picked(path) and goes back.
// The top level (the bridge's root) lists only the folders the bridge offers there (spec 009); "Up" stops at it.
// A button at the top switches between this list and a grid of tiles (a folder or file symbol, the name; spec 009).
Item {
    id: page
    property string path: attachments.rootDirectory
    property var rows: attachments.listDirectory(path)
    property string message: ""
    // "list" or "tile": the reader's last choice (kept by the bridge); a tap on the toggle changes and stores it
    property string view: attachments.pickerView === "tile" ? "tile" : "list"
    property string chosen: ""
    signal picked(string filePath)

    function trimSlash(p) { return p.length > 1 && p.charAt(p.length - 1) === "/" ? p.substring(0, p.length - 1) : p }
    // the top level: "Up" is not available here and the reader never sees the device's root
    readonly property bool atTop: trimSlash(path) === trimSlash(attachments.rootDirectory)

    function join(base, name) { return base.charAt(base.length - 1) === "/" ? base + name : base + "/" + name }
    function parentOf(p) {
        var cut = p.length > 1 && p.charAt(p.length - 1) === "/" ? p.substring(0, p.length - 1) : p
        var i = cut.lastIndexOf("/")
        return i <= 0 ? "/" : cut.substring(0, i)
    }
    // Taps on entries are ignored for 400 ms after the folder changed: a second tap meant for the old folder
    // would otherwise land on whatever is now under the finger. Up, Cancel, the toggle and paging are not delayed.
    property double lastNavigation: 0
    function changedFolder() { lastNavigation = Date.now(); showFirstPage() }
    // the first page of whichever view is shown
    function showFirstPage() { list.contentY = 0; grid.contentY = 0 }
    function setView(v) {
        view = v
        attachments.pickerView = v
        showFirstPage()
    }
    // the extension of a file name in capitals, at most four characters, for the file symbol of a tile
    function extensionOf(name) {
        var i = name.lastIndexOf(".")
        return i <= 0 || i === name.length - 1 ? "" : name.substring(i + 1, i + 5).toUpperCase()
    }
    function open(entry) {
        if (Date.now() - lastNavigation < 400) return
        message = ""
        if (entry.dir) { path = join(path, entry.name); changedFolder(); return }
        if (entry.size > attachments.maxBytes) {
            message = qsTr("This file is larger than the %1 limit.").arg(bridge.formatSize(attachments.maxBytes))
            return
        }
        chosen = join(path, entry.name)
        confirm.title = qsTr("Attach this file?")
        confirm.text = entry.name + "\n" + bridge.formatSize(entry.size)
        confirm.open()
    }

    TopBar {
        id: topBar
        title: qsTr("Attach")
        leftItems: [ IconButton { text: qsTr("Cancel"); icon: "chevron-left"; onClicked: page.StackView.view.pop() } ]
        rightItems: [
            IconButton {
                text: qsTr("Up"); icon: "chevron-up"
                enabled: !page.atTop
                onClicked: { page.message = ""; page.path = page.parentOf(page.path); page.changedFolder() }
            },
            // the layout toggle: shows the layout a tap switches to (a grid of squares while the list is shown, bars while
            // tiles are shown)
            IconButton {
                id: layoutToggle
                objectName: "layoutToggle"
                property string glyph: page.view === "list" ? "grid" : "list"
                text: qsTr("Layout")
                icon: glyph
                onClicked: page.setView(page.view === "list" ? "tile" : "list")
            }
        ]
    }
    Text {
        id: pathLine
        anchors { top: topBar.bottom; topMargin: 12; left: parent.left; right: parent.right; leftMargin: 30; rightMargin: 30 }
        elide: Text.ElideMiddle
        font.pixelSize: 26
        color: "#000000"
        text: page.path
    }
    Text {
        id: notice
        anchors { top: pathLine.bottom; left: parent.left; right: parent.right; leftMargin: 30; rightMargin: 30 }
        visible: page.message.length > 0
        wrapMode: Text.Wrap
        font.pixelSize: 26
        font.bold: true
        color: "#000000"
        text: page.message
    }
    Rectangle { id: rule; anchors.top: notice.visible ? notice.bottom : pathLine.bottom; anchors.topMargin: 8; width: parent.width; height: 2; color: "#111111" }

    // the room between the rule and the page buttons; a view takes whole rows of it, so no row is cut off at a page's end
    readonly property real room: footer.y - (rule.y + rule.height)

    ListView {
        id: list
        anchors { left: parent.left; right: parent.right; top: rule.bottom }
        height: Math.max(104, Math.floor(page.room / 104) * 104)
        clip: true
        visible: page.view === "list"
        model: page.view === "list" ? page.rows : []
        interactive: false
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: false

        delegate: Item {
            width: list.width
            height: 104
            readonly property var entry: modelData
            Row {
                x: 30
                width: parent.width - 60
                anchors.verticalCenter: parent.verticalCenter
                spacing: 14
                Text {
                    width: 90
                    font.pixelSize: 26
                    color: "#000000"
                    text: entry.dir ? qsTr("Folder") : ""
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    width: parent.width - 90 - 14 - sizeText.width - 14
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    font.pixelSize: 34
                    font.bold: entry.dir
                    color: "#000000"
                    text: entry.name
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    id: sizeText
                    font.pixelSize: 26
                    color: "#000000"
                    text: entry.dir ? "" : bridge.formatSize(entry.size)
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#808080" }
            Rectangle { anchors.fill: parent; color: "#d8d8d8"; visible: tap.pressed; z: -1 }
            MouseArea { id: tap; anchors.fill: parent; onClicked: page.open(entry) }
        }
    }
    // Tile view: whole rows of tiles per page like the list's whole rows; only the tiles on the page exist
    GridView {
        id: grid
        anchors { left: parent.left; right: parent.right; top: rule.bottom }
        height: Math.max(cellHeight, Math.floor(page.room / cellHeight) * cellHeight)
        clip: true
        visible: page.view === "tile"
        model: page.view === "tile" ? page.rows : []
        interactive: false
        boundsBehavior: Flickable.StopAtBounds
        cacheBuffer: 0
        reuseItems: false
        readonly property int columns: Math.max(2, Math.floor(width / 270))
        cellWidth: width / columns
        cellHeight: 250

        delegate: Item {
            width: grid.cellWidth
            height: grid.cellHeight
            readonly property var entry: modelData
            // a preview that has loaded replaces the file symbol; until then (and if it cannot be made) the symbol stays
            // the grid also builds the row that starts right below the page; only tiles on the page get a preview
            readonly property bool onPage: y >= grid.contentY - 1 && y + height <= grid.contentY + grid.height + 1
            readonly property bool previewReady: previewLoader.item !== null && previewLoader.item.status === Image.Ready
            Item {   // the symbol area, 170 px high
                x: 10; y: 6
                width: parent.width - 20
                height: 170
                Item {   // folder: a tab on a body
                    objectName: "folderGlyph"
                    visible: entry.dir
                    anchors.centerIn: parent
                    width: 120; height: 92
                    Rectangle { x: 0; y: 0; width: 50; height: 20; color: "#ffffff"; border.width: 3; border.color: "#000000" }
                    Rectangle { x: 0; y: 16; width: 120; height: 76; color: "#ffffff"; border.width: 3; border.color: "#000000" }
                }
                Rectangle {   // file: a page with its extension
                    objectName: "fileGlyph"
                    visible: !entry.dir && !previewReady
                    anchors.centerIn: parent
                    width: 100; height: 130
                    color: "#ffffff"; border.width: 3; border.color: "#000000"
                    Text {
                        anchors.centerIn: parent
                        font.pixelSize: 26
                        font.bold: true
                        color: "#000000"
                        text: page.extensionOf(entry.name)
                    }
                }
                // the picture itself, only for image files and only for the tiles on the page (the grid keeps no others)
                Loader {
                    id: previewLoader
                    anchors.fill: parent
                    active: onPage && entry.image === true && entry.previewId > 0
                    sourceComponent: Image {
                        asynchronous: true
                        cache: false
                        fillMode: Image.PreserveAspectFit
                        sourceSize.width: previewLoader.width
                        source: "image://filepreview/" + entry.previewId
                        visible: status === Image.Ready
                    }
                }
            }
            Text {
                x: 10; y: 180
                width: parent.width - 20
                height: 62
                elide: Text.ElideRight
                maximumLineCount: 2
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 24
                font.bold: entry.dir
                color: "#000000"
                text: entry.name
            }
            Rectangle { anchors.fill: parent; anchors.margins: 3; color: "transparent"; border.width: 1; border.color: "#808080" }
            Rectangle { anchors.fill: parent; color: "#d8d8d8"; visible: tileTap.pressed; z: -1 }
            MouseArea { id: tileTap; anchors.fill: parent; onClicked: page.open(entry) }
        }
    }
    Text {
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 40 }
        visible: page.rows.length === 0
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: 30
        color: "#000000"
        text: page.atTop ? qsTr("Nothing to browse here. None of the folders for attachments were found.")
                         : qsTr("This folder is empty or cannot be read.")
    }

    Item {
        id: footer
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: Style.touchMin
        PageButtons { anchors.centerIn: parent; target: page.view === "list" ? list : grid; snap: page.view === "list" ? 104 : grid.cellHeight }
    }

    ConfirmDialog {
        id: confirm
        acceptText: qsTr("Attach")
        cancelText: qsTr("Cancel")
        onAccepted: { page.picked(page.chosen); page.StackView.view.pop() }
    }
}
