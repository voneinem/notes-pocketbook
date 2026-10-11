import QtQuick

// One row of a table in the reader. `block` holds `cols` ([{html, links, align}]), `weights`, `header`
// and `lastRow` (contracts/qml-bridge.md). The columns share the width by weight with a minimum width
// each; cell text wraps and the row is as tall as its tallest cell. Cells have 2 px black borders, the
// header is bold with a 4 px rule below it. No colors, no animation (e-ink).
Item {
    id: root
    property var block
    property int fontPx: 38
    signal linkActivated(int column, string link)
    property bool selected: false   // a whole unit of the reader's selection (spec 005): drawn with a black frame

    readonly property int pad: 10
    readonly property int border: 2
    readonly property int count: block.cols.length
    readonly property int cellPx: Math.max(18, Math.round(fontPx * 0.85))
    property real cellsHeight: 0

    // Column widths: weight x width, at least minWidth each (or width / n when n columns cannot fit),
    // the overshoot taken proportionally from the columns above the minimum.
    readonly property var widths: {
        var n = count
        var avail = width
        if (n === 0 || avail <= 0) return []
        var minW = Math.min(Math.round(cellPx * 3.5), avail / n)
        var w = [], total = 0, slack = 0
        for (var i = 0; i < n; ++i) {
            var x = Math.max(minW, avail * block.weights[i])
            w.push(x)
            total += x
            slack += x - minW
        }
        if (total > avail && slack > 0) {
            var f = (total - avail) / slack
            for (var j = 0; j < n; ++j) w[j] = w[j] - f * (w[j] - minW)
        }
        return w
    }

    function updateHeight() {
        var h = 0
        for (var i = 0; i < cells.count; ++i) {
            var it = cells.itemAt(i)
            if (it) h = Math.max(h, it.textHeight)
        }
        cellsHeight = h
    }

    implicitHeight: cellsHeight + 2 * pad + (block.header ? 4 : border) + (block.header ? border : 0) + (block.lastRow ? 10 : 0)

    Row {
        id: row
        Repeater {
            id: cells
            model: root.block.cols
            delegate: Item {
                id: cell
                readonly property real textHeight: txt.contentHeight
                width: root.widths[index] !== undefined ? root.widths[index] : 0
                height: root.height - (root.block.lastRow ? 10 : 0)
                onTextHeightChanged: root.updateHeight()
                Component.onCompleted: root.updateHeight()

                Rectangle { x: 0; y: 0; width: root.border; height: parent.height; color: "#000000" }          // left edge
                Rectangle {                                                                                    // right edge (last column)
                    visible: index === root.count - 1
                    x: parent.width - root.border; y: 0; width: root.border; height: parent.height; color: "#000000"
                }
                Rectangle {                                                                                    // top edge (header row)
                    visible: root.block.header
                    x: 0; y: 0; width: parent.width; height: root.border; color: "#000000"
                }
                Rectangle {                                                                                    // rule below the row
                    x: 0; y: parent.height - (root.block.header ? 4 : root.border)
                    width: parent.width; height: root.block.header ? 4 : root.border; color: "#000000"
                }
                Text {
                    id: txt
                    x: root.pad + root.border
                    y: root.pad + (root.block.header ? root.border : 0)
                    width: Math.max(1, parent.width - 2 * root.pad - root.border)
                    wrapMode: Text.Wrap
                    textFormat: Text.StyledText
                    color: "#000000"
                    linkColor: "#000000"
                    font.pixelSize: root.cellPx
                    font.bold: root.block.header
                    horizontalAlignment: modelData.align === "right" ? Text.AlignRight
                                       : modelData.align === "center" ? Text.AlignHCenter : Text.AlignLeft
                    text: modelData.html
                    onLinkActivated: function(link) { root.linkActivated(index, link) }
                }
            }
        }
    }

    Rectangle {
        visible: root.selected
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: root.height - (root.block.lastRow ? 10 : 0)
        color: "transparent"
        border.width: 4
        border.color: "#000000"
    }
}
