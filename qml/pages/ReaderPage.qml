import QtQuick
import QtQuick.Controls
import ".."

// Reads one note: headings, bold/italic, lists and task boxes (read-only here), links that open in the
// system browser (after a prompt showing the real address when the link text differs from it). Large type, font size small/medium/large, scrolling by page buttons and edge taps.
Item {
    id: page
    property int noteId: 0
    property var note: ({ title: "", blocks: [] })

    property string message: ""

    function load() { note = bridge.openNote(noteId) }

    // A tapped link: `link` is "l<N>", an index into the block's links list ({url, confirm}).
    property string pendingUrl: ""
    function followLink(block, link) {
        var n = parseInt(String(link).substring(1))
        var target = block.links && block.links[n]
        if (!target) return
        if (target.confirm) {
            pendingUrl = target.url
            linkDialog.text = target.url
            linkDialog.open()
        } else {
            openUrl(target.url)
        }
    }
    // Blocks a page break must not cut, in content coordinates (for PageButtons).
    property var unsplittable: []
    function refreshUnsplittable() {
        var list = []
        for (var i = 0; i < blocks.count; ++i) {
            var it = blocks.itemAt(i)
            if (it && it.b.unsplittable) list.push({ top: body.y + it.y, bottom: body.y + it.y + it.height })
        }
        unsplittable = list
    }
    Connections {
        target: flick
        function onContentHeightChanged() { Qt.callLater(page.refreshUnsplittable) }
    }

    // A download finished or a size became known: reload the blocks, keeping the reading position.
    Connections {
        target: attachments
        function onAttachmentsChanged(changedNote) {
            if (changedNote !== page.noteId) return
            var y = flick.contentY
            page.load()
            flick.contentY = y
        }
        function onAttachmentFailed(failedNote, remotePath, text) {
            if (failedNote !== page.noteId) return
            page.message = text
            var y = flick.contentY
            page.load()
            flick.contentY = y
        }
    }
    // "Download" is a deliberate request: if Wi-Fi is off, ask the reader to connect first (like "Sync now").
    property string wifiWaitPath: ""
    function downloadAttachment(remotePath) {
        if (wifiWaitPath !== "") return
        message = ""
        if (device.wifiConnected()) { attachments.download(noteId, remotePath); return }
        wifiWaitPath = remotePath
        device.connectNetwork()
    }
    Connections {
        target: device
        function onNetworkResult(connected) {
            if (page.wifiWaitPath === "") return
            var path = page.wifiWaitPath
            page.wifiWaitPath = ""
            if (connected) { attachments.download(page.noteId, path); attachments.probeSizes(page.noteId) }
            else page.message = bridge.message("wifi_off")
        }
    }
    function openUrl(url) {
        if (!device.openUrl(url)) message = qsTr("The link could not be opened.")
    }
    Component.onCompleted: { load(); attachments.probeSizes(noteId) }
    // Returning from the picture page (or any page): reload, keeping the reading position.
    StackView.onActivated: { var y = flick.contentY; load(); flick.contentY = y }

    Row {
        id: top
        x: 30; y: 20
        width: parent.width - 60
        spacing: 16
        InkButton { id: back; text: qsTr("Back"); onClicked: page.StackView.view.pop() }
        InkButton {
            text: qsTr("A-")
            enabled: bridge.fontSize !== "small"
            onClicked: bridge.fontSize = bridge.fontSize === "large" ? "medium" : "small"
        }
        InkButton {
            text: qsTr("A+")
            enabled: bridge.fontSize !== "large"
            onClicked: bridge.fontSize = bridge.fontSize === "small" ? "medium" : "large"
        }
        InkButton {
            text: qsTr("Edit")
            onClicked: page.StackView.view.push(Qt.resolvedUrl("EditorPage.qml"), { noteId: page.noteId })
        }
        Text {
            width: Math.max(0, parent.width - 4 * 160)
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
            font.pixelSize: 26
            color: "#000000"
            text: (page.note.favorite ? "★ " : "") + (page.note.category || "")
        }
    }
    Rectangle { id: rule; anchors.top: top.bottom; anchors.topMargin: 12; width: parent.width; height: 2; color: "#111111" }

    Flickable {
        id: flick
        anchors { left: parent.left; right: parent.right; top: rule.bottom; bottom: footer.top }
        contentWidth: width
        contentHeight: body.implicitHeight + 40
        interactive: false
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: body
            x: 50; y: 20
            width: flick.width - 100
            spacing: 0  // blocks bring their own gap; table rows touch

            Repeater {
                id: blocks
                model: page.note.blocks
                delegate: Loader {
                    id: ld
                    readonly property var b: modelData
                    width: body.width
                    sourceComponent: b.kind === "tablerow" ? tableRow : (b.kind === "attachment" ? attachmentBlock : textBlock)
                    Component {
                        id: textBlock
                        BlockText {
                            width: ld.width
                            block: ld.b
                            fontPx: bridge.fontPx
                            onLinkActivated: function(link) { page.followLink(ld.b, link) }
                            onTaskToggled: function(line) {
                                var y = flick.contentY
                                if (bridge.toggleTask(page.noteId, line)) { page.load(); flick.contentY = y }
                            }
                        }
                    }
                    Component {
                        id: attachmentBlock
                        AttachmentView {
                            width: ld.width
                            block: ld.b
                            fontPx: bridge.fontPx
                            waiting: page.wifiWaitPath !== "" && page.wifiWaitPath === ld.b.remotePath
                            onDownloadRequested: page.downloadAttachment(ld.b.remotePath)
                            onOpenRequested: attachments.openAttachment(ld.b.attachmentId)
                            onImageTapped: page.StackView.view.push(Qt.resolvedUrl("ImagePage.qml"), { attachmentId: ld.b.attachmentId })
                        }
                    }
                    Component {
                        id: tableRow
                        TableRowView {
                            width: ld.width
                            block: ld.b
                            fontPx: bridge.fontPx
                            onLinkActivated: function(column, link) { page.followLink(ld.b.cols[column], link) }
                        }
                    }
                }
            }
        }

        // Tap zones at the left and right edge: previous / next page, instantly.
        // (Placed in the viewport, not in the content.)
    }
    MouseArea {
        anchors { left: parent.left; top: flick.top; bottom: flick.bottom }
        width: 50
        onClicked: pager.pageBy(-1)
    }
    MouseArea {
        anchors { right: parent.right; top: flick.top; bottom: flick.bottom }
        width: 50
        onClicked: pager.pageBy(1)
    }

    Text {
        id: notice
        visible: page.message.length > 0
        anchors { left: parent.left; right: parent.right; bottom: footer.top; margins: 30 }
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        font.pixelSize: 28
        color: "#000000"
        text: page.message
        Timer { running: page.message.length > 0; interval: 6000; onTriggered: page.message = "" }
    }

    ConfirmDialog {
        id: linkDialog
        title: qsTr("Open this link?")
        acceptText: qsTr("Open")
        cancelText: qsTr("Cancel")
        onAccepted: page.openUrl(page.pendingUrl)
    }

    Row {
        id: footer
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 16 }
        spacing: 24
        PageButtons { id: pager; target: flick; unsplittable: page.unsplittable }
    }
}
