import QtQuick
import QtQuick.Controls
import ".."

// Reads one note: its title, then headings, bold/italic, lists and task boxes (read-only here), links that open in the
// system browser (after a prompt showing the real address when the link text differs from it). Text can be selected
// (a long press selects a word and shows two markers to drag and a bubble with Copy and Select all) and Copy puts the
// note's source text for the range on the clipboard (spec 005).
// Large type, font size small/medium/large, scrolling by page buttons and edge taps.
Item {
    id: page
    property int noteId: 0
    property var note: ({ title: "", blocks: [] })

    property string message: ""
    property bool sizePanelOpen: false   // the text size panel above the bottom bar

    // ---- <details> sections (spec 006): which ones the reader opened or closed this visit, by the section's key ----
    // Absent = the state the note was written in (`<details open>`). Survives the reloads of the page, never stored.
    property var openDetails: ({})
    function isOpen(b) {
        var v = openDetails[b.detailsId]
        return v !== undefined ? v : b.detailsOpen === true
    }
    // the content of a closed section is not shown; the summary and everything outside a section always are
    function shown(b) { return (b.detailsId === undefined || b.detailsId < 0) || b.kind === "summary" || isOpen(b) }
    function toggleDetails(b) {
        if (selectionTapGuard()) return   // a tap while a selection exists only ends the selection
        var next = {}
        for (var k in openDetails) next[k] = openDetails[k]
        next[b.detailsId] = !isOpen(b)
        openDetails = next
    }

    function load() {
        clearSelection()   // a reload ends the selection (spec 005 FR-030)
        note = bridge.openNote(noteId)
    }

    // ---- selecting and copying (spec 005) ----
    // The selection runs over units: the title (when the note has one), then one unit per block. A text unit takes a
    // character range (UTF-16 offsets); a table row, an attachment and a blank line are selected whole.
    property var selection: null     // {fromUnit, fromOffset, toUnit, toOffset}, from <= to; null = none
    property real lastClearMs: 0
    readonly property int titleUnits: note.hasTitle === true ? 1 : 0
    readonly property int unitCount: titleUnits + (note.blocks ? note.blocks.length : 0)

    function unitItem(u) {
        if (u < titleUnits) return titleBlock
        var l = blocks.itemAt(u - titleUnits)
        return l ? l.item : null
    }
    function unitIsText(u) {
        if (u < titleUnits) return true
        var k = note.blocks[u - titleUnits].kind
        return k !== "tablerow" && k !== "attachment" && k !== "blank" && k !== "fence" && k !== "callout" && k !== "marker"
    }
    function unitLength(u) {
        var it = unitItem(u)
        return (it && unitIsText(u)) ? it.textLength : 0
    }
    function before(ua, oa, ub, ob) { return ua < ub || (ua === ub && oa < ob) }

    function applySelection() {
        for (var u = 0; u < unitCount; ++u) {
            var it = unitItem(u)
            if (!it) continue
            var a = -1, b = -1, whole = false
            if (selection && u >= selection.fromUnit && u <= selection.toUnit) {
                if (unitIsText(u)) {
                    a = (u === selection.fromUnit) ? selection.fromOffset : 0
                    b = (u === selection.toUnit) ? selection.toOffset : it.textLength
                } else {
                    whole = true
                }
            }
            if (unitIsText(u)) { it.selFrom = a; it.selTo = b }
            else it.selected = whole
        }
    }
    function setSelection(sel) { selection = sel; applySelection() }
    function clearSelection() {
        if (selection) lastClearMs = Date.now()
        selection = null
        applySelection()
        if (menu) menu.close()
    }
    // A tap on a link, a task box or an attachment while a selection exists only ends the selection (US4-7). The
    // unit's own tap handler may have cleared it a moment earlier, so a clear in the last 300 ms counts as well.
    function selectionTapGuard() {
        if (selection) { clearSelection(); return true }
        return Date.now() - lastClearMs < 300
    }
    function inSelection(u, pos) {
        return selection && !before(u, pos, selection.fromUnit, selection.fromOffset)
                         && !before(selection.toUnit, selection.toOffset, u, pos)
    }
    // A long press: on a word it selects the word; on a table row, an attachment or a blank line the whole unit.
    function wordHeld(u, start, end) {
        if (!unitIsText(u)) { unitHeld(u); return }
        setSelection({ fromUnit: u, fromOffset: start, toUnit: u, toOffset: end })
        openMenu()
    }
    function unitHeld(u) {
        setSelection({ fromUnit: u, fromOffset: 0, toUnit: u, toOffset: 0 })
        openMenu()
    }
    // A tap on a unit (pos = the tapped character; 0 for a whole unit): a tap outside the selection ends it.
    function unitTapped(u, pos) {
        if (selection && !inSelection(u, pos)) clearSelection()
    }
    function selectAll() {
        if (unitCount === 0) return
        var last = unitCount - 1
        setSelection({ fromUnit: 0, fromOffset: 0, toUnit: last, toOffset: unitLength(last) })
    }
    function copySelection() {
        if (!selection) return
        var text = bridge.readerCopy(noteId, selection.fromUnit, selection.fromOffset, selection.toUnit, selection.toOffset)
        if (text.length > 0) clipboard.copy(text)
    }
    Connections {
        target: bridge
        function onFontSizeChanged() { page.clearSelection() }
    }

    // ---- the markers and the menu (positions are in the content of the page's flickable) ----
    // The point a selection edge is at: x, the bottom of its line and the line's height.
    function pointOf(u, offset, atEnd) {
        var it = unitItem(u)
        if (!it) return null
        if (!it.visible || it.height <= 0) {   // closed content or a structural line: stand at the nearest unit that is shown
            for (var v = u + (atEnd ? -1 : 1); v >= 0 && v < unitCount; v += atEnd ? -1 : 1) {
                var w = unitItem(v)
                if (w && w.visible && w.height > 0) { u = v; it = w; offset = atEnd ? unitLength(v) : 0; break }
            }
            if (!it.visible || it.height <= 0) return null
        }
        var content = flick.contentItem
        if (unitIsText(u)) {
            var r = it.rectAt(offset)
            var p = it.mapToItem(content, r.x, r.y)
            return { x: p.x, y: p.y + r.height, h: r.height }
        }
        var q = it.mapToItem(content, atEnd ? it.width : 0, atEnd ? it.height - 10 : 0)
        return { x: q.x, y: atEnd ? q.y : q.y + 30, h: 30 }
    }
    readonly property var startPoint: selection ? pointOf(selection.fromUnit, selection.fromOffset, false) : null
    readonly property var endPoint: selection ? pointOf(selection.toUnit, selection.toOffset, true) : null

    function unitAtY(y) {
        var best = 0
        for (var u = 0; u < unitCount; ++u) {
            var it = unitItem(u)
            if (!it || !it.visible || it.height <= 0) continue   // closed content and structural lines have no place on the page
            if (it.mapToItem(flick.contentItem, 0, 0).y <= y) best = u
        }
        return best
    }
    // A marker was dragged to (x, y) in the flickable's content: move that edge of the selection there.
    function moveHandle(isStart, x, y) {
        if (!selection) return
        var u = unitAtY(y)
        var it = unitItem(u)
        if (!it) return
        var off = 0
        if (unitIsText(u)) {
            var p = flick.contentItem.mapToItem(it, x, y)
            off = it.offsetAt(p.x, p.y)
        }
        if (isStart) {
            if (before(u, off, selection.toUnit, selection.toOffset))
                setSelection({ fromUnit: u, fromOffset: off, toUnit: selection.toUnit, toOffset: selection.toOffset })
        } else if (before(selection.fromUnit, selection.fromOffset, u, off)) {
            setSelection({ fromUnit: selection.fromUnit, fromOffset: selection.fromOffset, toUnit: u, toOffset: off })
        }
    }
    function menuItems() {
        var list = [{ id: "copy", label: qsTr("Copy") }]
        var last = unitCount - 1
        var all = selection && selection.fromUnit === 0 && selection.fromOffset === 0 && selection.toUnit === last
                  && selection.toOffset >= unitLength(last)
        if (!all) list.push({ id: "all", label: qsTr("Select all") })
        return list
    }
    function placeMenu() {
        if (!selection || !startPoint || !endPoint) return
        var a = flick.contentItem.mapToItem(page, startPoint.x, startPoint.y - startPoint.h)
        var b = flick.contentItem.mapToItem(page, endPoint.x, endPoint.y)
        menu.anchorX = (a.x + b.x) / 2
        menu.selTop = a.y
        menu.selBottom = b.y
        menu.minY = topBar.y + topBar.height + 10
        menu.maxY = footer.y
        menu.place()
    }
    function openMenu() {
        if (!selection) return
        menu.items = menuItems()
        placeMenu()
        menu.visible = true
    }
    function menuChosen(id) {
        menu.close()
        if (id === "copy") { copySelection(); clearSelection() }
        else if (id === "all") { selectAll(); openMenu() }
    }

    // A tapped link: `link` is "l<N>", an index into the block's links list ({url, confirm}).
    property string pendingUrl: ""
    function followLink(block, link) {
        if (selectionTapGuard()) return
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
            if (!it || !it.visible) continue
            if (it.b.unsplittable) list.push({ top: body.y + it.y, bottom: body.y + it.y + it.height })
            // keep with next (spec 006 FR-005): a heading and the first line below it stay on one page
            else if (it.b.kind === "heading")
                list.push({ top: body.y + it.y, bottom: body.y + it.y + it.height + Math.round(bridge.fontPx * 1.4) })
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
        if (selectionTapGuard()) return
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

    // empty space (below the text, beside the buttons): a tap there ends a selection
    MouseArea {
        anchors.fill: parent
        onClicked: { page.sizePanelOpen = false; page.clearSelection() }
    }

    TopBar {
        id: topBar
        title: page.note.title || ""
        leftItems: [ IconButton { text: qsTr("Back"); icon: "chevron-left"; onClicked: page.StackView.view.pop() } ]
        rightItems: [
            IconButton {
                text: qsTr("Edit"); icon: "pencil"
                onClicked: page.StackView.view.push(Qt.resolvedUrl("EditorPage.qml"), { noteId: page.noteId })
            }
        ]
    }

    Flickable {
        id: flick
        objectName: "flick"
        anchors { left: parent.left; right: parent.right; top: topBar.bottom; bottom: footer.top }
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

            // The category and the favorite mark (spec 010): one quiet line above the title; it scrolls with the text, so the
            // text area keeps its whole height.
            Row {
                id: captionRow
                objectName: "captionRow"
                visible: page.note.favorite === true || (page.note.category || "").length > 0
                height: visible ? 48 : 0
                spacing: 12
                Icon { visible: page.note.favorite === true; width: visible ? 32 : 0; size: 32; source: "star-filled"; anchors.verticalCenter: parent.verticalCenter }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: page.note.category || ""
                    font.pixelSize: Style.captionSize
                    color: Style.ink
                }
            }

            // The title (spec 005): above the first block, scrolls and pages with the text; none, and no gap, for a
            // note without a title.
            TitleBlock {
                id: titleBlock
                objectName: "titleBlock"
                onWordHeld: function(start, end) { page.wordHeld(0, start, end) }
                onTapped: function(pos) { page.unitTapped(0, pos) }
                visible: page.note.hasTitle === true
                height: visible ? implicitHeight : 0
                width: body.width
                title: page.note.titleText || ""
                fontPx: bridge.fontPx
            }

            Repeater {
                id: blocks
                model: page.note.blocks
                delegate: Loader {
                    id: ld
                    readonly property var b: modelData
                    readonly property int unit: index + page.titleUnits
                    width: body.width
                    visible: page.shown(b)
                    sourceComponent: b.kind === "tablerow" ? tableRow : (b.kind === "attachment" ? attachmentBlock : textBlock)
                    Component {
                        id: textBlock
                        BlockText {
                            width: ld.width
                            block: ld.b
                            fontPx: bridge.fontPx
                            open: ld.b.kind === "summary" && page.isOpen(ld.b)
                            onSummaryToggled: page.toggleDetails(ld.b)
                            onLinkActivated: function(link) { page.followLink(ld.b, link) }
                            onWordHeld: function(start, end) { page.wordHeld(ld.unit, start, end) }
                            onTapped: function(pos) { page.unitTapped(ld.unit, pos) }
                            onTaskToggled: function(line) {
                                if (page.selectionTapGuard()) return
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
                            onOpenRequested: { if (!page.selectionTapGuard()) attachments.openAttachment(ld.b.attachmentId) }
                            onImageTapped: { if (!page.selectionTapGuard()) page.StackView.view.push(Qt.resolvedUrl("ImagePage.qml"), { attachmentId: ld.b.attachmentId }) }
                        }
                    }
                    // a table row, an attachment and a blank line are selected whole: a long press selects it, a tap
                    // outside the selection ends it (the loaded item sits above and keeps its own buttons and links)
                    MouseArea {
                        anchors.fill: parent
                        enabled: !page.unitIsText(ld.unit)
                        pressAndHoldInterval: 600
                        property bool held: false
                        onPressed: held = false
                        onPressAndHold: { held = true; page.unitHeld(ld.unit) }
                        onClicked: if (!held) page.unitTapped(ld.unit, 0)
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

        // the selection's markers scroll with the text
        SelectionHandle {
            atStart: true
            visible: page.selection !== null && page.startPoint !== null
            point: page.startPoint ? Qt.point(page.startPoint.x, page.startPoint.y) : Qt.point(0, 0)
            lineHeight: page.startPoint ? page.startPoint.h : 40
            onPressedDown: menu.close()
            onDragged: function(x, y) { page.moveHandle(true, x, y) }
            onReleased: page.openMenu()
        }
        SelectionHandle {
            atStart: false
            visible: page.selection !== null && page.endPoint !== null
            point: page.endPoint ? Qt.point(page.endPoint.x, page.endPoint.y) : Qt.point(0, 0)
            lineHeight: page.endPoint ? page.endPoint.h : 40
            onPressedDown: menu.close()
            onDragged: function(x, y) { page.moveHandle(false, x, y) }
            onReleased: page.openMenu()
        }
        onContentYChanged: if (menu) menu.close()

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

    // ---- the bottom bar: the Text size tool at the left, the page controls with n / total at the right ----
    Item {
        id: footer
        objectName: "footer"
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: Style.bottomBarHeight
        Rectangle { width: parent.width; height: Style.hairlineWidth; color: Style.hairline }
        ToolButton {
            id: sizeTool
            x: 30
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Text size"); icon: "text-size"
            toggled: false
            onClicked: page.sizePanelOpen = !page.sizePanelOpen
        }
        PageButtons {
            id: pager
            anchors { right: parent.right; rightMargin: 30; verticalCenter: parent.verticalCenter }
            target: flick
            unsplittable: page.unsplittable
            showIndicator: true
        }
    }

    // a tap on the page outside the open panel closes it
    MouseArea {
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: textSizePanel.top }
        enabled: page.sizePanelOpen
        visible: page.sizePanelOpen
        z: 50
        onClicked: page.sizePanelOpen = false
    }
    // ---- the text size as a slider with its size in points (spec 010; the three sizes are the stored setting) ----
    Rectangle {
        id: textSizePanel
        objectName: "textSizePanel"
        visible: page.sizePanelOpen
        z: 51
        anchors { left: parent.left; right: parent.right; bottom: footer.top }
        height: 150
        color: Style.paper
        Rectangle { width: parent.width; height: Style.hairlineWidth; color: Style.hairline }
        MouseArea { anchors.fill: parent }   // the panel keeps its own taps
        InkSlider {
            id: sizeSlider
            objectName: "textSizeSlider"
            anchors { left: parent.left; right: parent.right; leftMargin: 30; rightMargin: 30; verticalCenter: parent.verticalCenter }
            stops: 3
            readonly property var names: ["small", "medium", "large"]
            index: Math.max(0, names.indexOf(bridge.fontSize))
            label: Style.ptFor(bridge.fontPx) + " pt"
            onMoved: function(i) { bridge.fontSize = names[i] }
        }
    }

    SelectionMenu {
        id: menu
        onChosen: function(id) { page.menuChosen(id) }
    }
}
