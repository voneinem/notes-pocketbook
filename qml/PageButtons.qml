import QtQuick
import QtQuick.Controls

// "Page up" / "Page down" for a Flickable or ListView: the view jumps by one screen at once. Sliding
// animations and kinetic scrolling look like smeared frames on e-ink, so scrolling is by page.
Row {
    id: bar
    property Item target: null
    // Row height of a list: pages then move by whole rows, so no row is ever cut off at the top.
    property real snap: 0
    // Blocks that must not be cut by a page break, [{top, bottom}] in content coordinates (table rows,
    // attachments). When the list is not empty the target comes from the core's page_target.
    property var unsplittable: []
    // "n / total" between the two arrows (the reader shows it, spec 010). The total is an estimate from the distance one page
    // moves (a page with a block that must not be cut can move a little less); the number follows the position.
    property bool showIndicator: false
    readonly property real pageStep: !target ? 1 : (snap > 0 ? Math.max(1, Math.floor(target.height / snap)) * snap : Math.max(100, target.height - 80))
    readonly property int pageCount: !target ? 1 : Math.max(1, Math.ceil(Math.max(0, target.contentHeight - target.height - 1) / pageStep) + 1)
    readonly property int pageNumber: !target ? 1 : Math.min(pageCount, Math.max(1, Math.floor((target.contentY - (target.originY || 0)) / pageStep + 0.5) + 1))
    spacing: 24

    function pageBy(direction) {
        if (!target) return
        var top = target.originY !== undefined ? target.originY : 0
        var maxY = Math.max(top, top + target.contentHeight - target.height)
        var step = Math.max(100, target.height - 80)
        if (snap <= 0 && unsplittable.length > 0) {
            target.contentY = bridge.pageTarget(target.contentY, target.height, target.contentHeight, step, direction, unsplittable)
            return
        }
        if (snap > 0) {
            step = Math.max(1, Math.floor(target.height / snap)) * snap
            // the last page may end below the content, but its rows are whole
            maxY = top + Math.ceil((maxY - top) / snap) * snap
        }
        target.contentY = Math.min(maxY, Math.max(top, target.contentY + direction * step))
    }
    readonly property bool canUp: target && target.contentY > (target.originY || 0) + 1
    readonly property bool canDown: target && target.contentY < (target.originY || 0) + target.contentHeight - target.height - 1

    IconButton { text: qsTr("Page up"); icon: "chevron-up"; enabled: bar.canUp; onClicked: bar.pageBy(-1) }
    Text {
        objectName: "pageIndicator"
        visible: bar.showIndicator
        width: visible ? 150 : 0
        height: Style.touchMin
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: Style.bodySize
        color: Style.ink
        text: bar.pageNumber + " / " + bar.pageCount
    }
    IconButton { text: qsTr("Page down"); icon: "chevron-down"; enabled: bar.canDown; onClicked: bar.pageBy(1) }
}
