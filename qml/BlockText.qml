import QtQuick

// One text block of a note in the reader: a heading, a paragraph or a list item (bullet, number or a
// drawn task box), with its styled text. Emits taskToggled(line) when the task box is tapped and
// linkActivated(link) when a link in the text is tapped; the reader decides what happens then. The text can be
// selected (spec 005): selFrom/selTo (UTF-16 offsets, -1 = none) are set by the page; wordHeld(start, end) reports a
// press-and-hold, tapped(pos) a tap that is not on a link. A blank block is selected whole (`selected`).
// A code line ("code") and a fence line ("fence") are the lines and the edges of a code box (spec 006): the blocks of a
// box touch each other, the first one draws the top edge (codeFirst), the last one the bottom edge (codeLast), every
// one the sides; the text is set in a fixed-width font. A fence strip is selected whole like a blank line.
// A block in a quote (block.quote 1) has a bar at its left, one in an info box (2) sits in a box that the blocks of the
// group draw together (quoteFirst / quoteLast give the edges); the "callout" row names the kind of the box in words.
// A "summary" row is the toggle of a <details> section (`open` is set by the page, summaryToggled() reports a tap); a
// "marker" is a structural line that shows nothing.
Item {
    id: root
    property var block
    property int fontPx: 38
    property bool open: false              // a summary row: its section is open
    signal taskToggled(int line)
    signal linkActivated(string link)
    signal wordHeld(int start, int end)
    signal tapped(int pos)
    signal summaryToggled()
    property int selFrom: -1
    property int selTo: -1
    property bool selected: false          // a blank line inside the selection
    readonly property int textLength: text.length
    // a character's rectangle and the character under a point, in this item's coordinates (for the reader's markers)
    function rectAt(offset) {
        var r = text.positionToRectangle(offset)
        var p = text.mapToItem(root, r.x, r.y)
        return Qt.rect(p.x, p.y, r.width, r.height)
    }
    function offsetAt(x, y) {
        var p = root.mapToItem(text, x, y)
        return text.positionAt(p.x, p.y)
    }

    readonly property bool code: block.kind === "code"
    readonly property bool fence: block.kind === "fence"
    readonly property bool boxed: code || fence
    readonly property bool callout: block.kind === "callout"
    readonly property bool summary: block.kind === "summary"
    readonly property bool marker: block.kind === "marker"
    readonly property bool quoted: (block.quote || 0) === 1
    readonly property bool info: (block.quote || 0) === 2
    readonly property int boxPad: 16     // text inset inside a code box
    readonly property int edgePx: 3      // border of a code box
    readonly property int fenceHeight: 14
    readonly property int quoteBarPx: 6
    readonly property int quoteIndent: 24     // text inset behind a quote bar
    readonly property int infoPad: 18         // text inset inside an info box
    // how far a quote bar or an info box pushes the block's content in
    readonly property int leftInset: quoted ? quoteIndent : (info ? infoPad : 0)
    readonly property int rightInset: info ? infoPad : 0
    // the gap to the next block: none inside a code box so that its lines touch
    readonly property int gap: marker ? 0 : ((boxed && block.codeLast !== true) ? 0 : 10)
    // the height a quote bar or an info box frame covers: the block and, inside a group, the gap to the next block
    readonly property real frameHeight: implicitHeight - (block.quoteLast === true ? gap : 0)

    function headingPx(level) {
        var scale = [1.9, 1.55, 1.3, 1.15, 1.1, 1.05]
        return Math.round(fontPx * scale[Math.min(5, Math.max(0, level - 1))])
    }
    function calloutLabel(kind) {
        if (kind === "tip") return qsTr("Tip")
        if (kind === "warning") return qsTr("Warning")
        if (kind === "important") return qsTr("Important")
        if (kind === "caution") return qsTr("Caution")
        return qsTr("Note")
    }

    // The block's own height plus the 10 px gap to the next block (table rows must touch each other, so the
    // reader's Column has no spacing of its own).
    implicitHeight: marker ? 0
                  : (block.kind === "blank" ? Math.round(fontPx * 0.5)
                  : (fence ? fenceHeight : (callout ? calloutText.implicitHeight : row.implicitHeight))
                     + (info && block.quoteLast === true ? edgePx + 6 : 0)) + gap

    // a quote's bar and an info box's frame, continuous over the blocks of the group
    Rectangle { objectName: "quoteBar"; visible: root.quoted; x: 0; width: root.quoteBarPx; height: root.frameHeight; color: "#000000" }
    Item {
        visible: root.info
        anchors.fill: parent
        Rectangle { objectName: "infoSide"; x: 0; width: root.edgePx; height: root.frameHeight; color: "#000000" }
        Rectangle { objectName: "infoSide"; anchors.right: parent.right; width: root.edgePx; height: root.frameHeight; color: "#000000" }
        Rectangle { objectName: "infoTop"; visible: root.block.quoteFirst === true; width: parent.width; height: root.edgePx; color: "#000000" }
        Rectangle {
            objectName: "infoBottom"
            visible: root.block.quoteLast === true
            y: root.frameHeight - root.edgePx
            width: parent.width; height: root.edgePx; color: "#000000"
        }
    }

    // the code box behind a code line or a fence strip
    Item {
        id: box
        visible: root.boxed
        anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: root.leftInset; rightMargin: root.rightInset }
        height: root.implicitHeight - root.gap
        Rectangle { anchors.fill: parent; color: "#f2f2f2" }
        Rectangle { objectName: "codeSide"; x: 0; width: root.edgePx; height: parent.height; color: "#000000" }
        Rectangle { objectName: "codeSide"; anchors.right: parent.right; width: root.edgePx; height: parent.height; color: "#000000" }
        Rectangle { objectName: "codeTop"; visible: root.block.codeFirst === true; width: parent.width; height: root.edgePx; color: "#000000" }
        Rectangle {
            objectName: "codeBottom"
            visible: root.block.codeLast === true
            anchors.bottom: parent.bottom
            width: parent.width; height: root.edgePx; color: "#000000"
        }
    }

    // the header of an info box names its kind in words (no colour on e-ink)
    Text {
        id: calloutText
        objectName: "calloutLabel"
        visible: root.callout
        x: root.leftInset
        width: parent.width - root.leftInset - root.rightInset
        topPadding: root.edgePx + 4
        font.pixelSize: root.fontPx
        font.bold: true
        color: "#000000"
        text: root.calloutLabel(root.block.callout)
    }

    // a summary row: the whole row toggles its section (the text reports its own taps too)
    MouseArea {
        anchors.fill: parent
        enabled: root.summary
        onClicked: root.summaryToggled()
    }

    Row {
        id: row
        visible: root.block.kind !== "blank" && !root.fence && !root.callout && !root.marker
        x: root.leftInset + (root.block.kind === "listitem" ? root.block.depth * 36 : 0) + (root.code ? root.boxPad : 0)
        spacing: 14
        width: parent.width - x - root.rightInset - (root.code ? root.boxPad : 0)

        // the open / closed marker of a summary row
        Text {
            objectName: "summaryMark"
            visible: root.summary
            width: root.summary ? Math.round(root.fontPx * 1.1) : 0
            font.pixelSize: root.fontPx
            color: "#000000"
            text: root.open ? "▼" : "▶"
        }

        // list marker: bullet, number, or a drawn checkbox
        Item {
            visible: root.block.kind === "listitem"
            width: root.block.kind === "listitem" ? Math.round(root.fontPx * 1.1) : 0
            height: root.fontPx * 1.3
            Rectangle {
                visible: root.block.task
                width: Math.round(root.fontPx * 0.8); height: width
                anchors.verticalCenter: parent.verticalCenter
                color: "#ffffff"; border.width: 3; border.color: "#000000"
                Rectangle {
                    visible: root.block.checked
                    anchors.centerIn: parent
                    width: parent.width - 12; height: width
                    color: "#000000"
                }
                // generous touch target around the drawn box; saves at once (offline edit)
                MouseArea {
                    anchors.centerIn: parent
                    width: parent.width + 40; height: parent.height + 40
                    onClicked: root.taskToggled(root.block.line)
                }
            }
            Text {
                visible: !root.block.task
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: root.fontPx
                color: "#000000"
                text: root.block.ordered ? root.block.number + "." : "•"
            }
        }
        SelectableText {
            id: text
            width: parent.width - (root.block.kind === "listitem" ? Math.round(root.fontPx * 1.1) + 14 : 0)
                   - (root.summary ? Math.round(root.fontPx * 1.1) + 14 : 0)
            font.pixelSize: root.block.kind === "heading" ? root.headingPx(root.block.level)
                          : (root.code ? Math.round(root.fontPx * 0.9) : root.fontPx)
            font.bold: root.block.kind === "heading"
            html: root.block.html
            selFrom: root.selFrom
            selTo: root.selTo
            onLinkTapped: function(link) { root.linkActivated(link) }
            onWordHeld: function(start, end) { root.wordHeld(start, end) }
            onTapped: function(pos) { root.tapped(pos); if (root.summary) root.summaryToggled() }
            // code is set in the device's fixed-width face (a missing one falls back to the default face)
            Binding { target: text; property: "font.family"; value: "monospace"; when: root.code }
        }
    }

    // a blank line has no text to tap: the whole line is a selection unit
    MouseArea {
        anchors.fill: parent
        enabled: root.block.kind === "blank"
        pressAndHoldInterval: 600
        property bool held: false
        onPressed: held = false
        onPressAndHold: { held = true; root.wordHeld(0, 0) }
        onClicked: if (!held) root.tapped(0)
    }
    Rectangle {
        visible: root.selected
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: root.implicitHeight - root.gap
        color: "transparent"
        border.width: 4
        border.color: "#000000"
    }
}
