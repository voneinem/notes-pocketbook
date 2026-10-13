import QtQuick

// The context menu of a selection or a long press (spec 005, look: spec 010): a bubble with a heavy outline, a pointer toward
// the selection and a row of icons, like the reader's own selection bubble. Each entry's `label` is its text (the accessible
// name, and what is drawn if its icon is missing). The page fills `items` ([{id, label}]) and sets where it points
// (`anchorX`, `selTop`, `selBottom`: the selection) inside `bounds`; the bubble sits above the
// selection, or below it when there is no room above. Nothing animates (e-ink); no button takes focus.
Item {
    id: root
    objectName: "selectionMenu"
    property var items: []
    property real anchorX: 0
    property real selTop: 0
    property real selBottom: 0
    property real minY: 0          // the bubble must stay below this line (the page's top bars)
    property real maxY: 100000     // and above this one (the keyboard)
    property real gap: 24
    property real below: 80        // extra room below the selection for its markers
    property bool pointerAtTop: false   // the bubble sits below the selection: its pointer is on its top edge
    signal chosen(string id)

    // the icon of an entry (by its id); an id without a picture draws its label
    function iconFor(id) {
        var icons = { cut: "cut", copy: "copy", paste: "paste", remove: "trash", all: "select-all", undo: "undo", redo: "redo" }
        return icons[id] || ""
    }

    visible: false
    z: 2000
    width: Math.min(parent ? parent.width - 40 : 800, bubble.width)
    height: bubble.height

    function open() { place(); visible = items.length > 0 }
    function close() { visible = false }
    function place() {
        var w = bubble.width, h = bubble.height
        x = Math.max(20, Math.min(anchorX - w / 2, (parent ? parent.width : w) - w - 20))
        var above = selTop - h - gap
        pointerAtTop = !(above >= minY)
        y = above >= minY ? above : selBottom + below
        y = Math.max(minY, Math.min(y, maxY - h - 10))
    }
    onItemsChanged: if (visible) place()
    // the bubble's size is only known once its buttons are laid out: place it again then
    onWidthChanged: if (visible) place()
    onHeightChanged: if (visible) place()
    onVisibleChanged: if (visible) place()

    PopupBubble {
        id: bubble
        width: flow.width + 2 * 18
        height: flow.height + 2 * 14
        pointerUp: root.pointerAtTop
        pointerX: root.anchorX - root.x

        Flow {
            id: flow
            x: 18; y: 14
            spacing: 6
            width: Math.min(natural, (root.parent ? root.parent.width : 800) - 40 - 2 * 18)
            readonly property real natural: {
                var w = 0
                for (var i = 0; i < kids.count; ++i) w += kids.itemAt(i) ? kids.itemAt(i).width + spacing : 0
                return Math.max(0, w - spacing)
            }
            Repeater {
                id: kids
                model: root.items
                delegate: IconButton {
                    text: modelData.label
                    icon: root.iconFor(modelData.id)
                    iconSize: Style.iconTool
                    onClicked: root.chosen(modelData.id)
                }
            }
        }
    }
}
