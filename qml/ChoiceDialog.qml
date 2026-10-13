import QtQuick
import QtQuick.Controls

// A list of choices as a pop-up bubble over the current page (spec 010): rows of a label with the current one marked,
// split by dotted dividers; paged by arrows inside the bubble when there are more rows than fit. `choices` is a list of
// {label, value}. With `anchor` (an Item) the bubble hangs under it with its pointer on the anchor's centre, or above it when
// there is no room below; without one it is centred. A tap outside the bubble closes it. No animations (e-ink).
// `title` names the choice (accessible name; not drawn: the control the bubble hangs from already says what it is).
Item {
    id: root
    property string title
    property var choices: []
    property string current
    property Item anchor: null
    signal chosen(string value)

    anchors.fill: parent
    visible: false
    z: 1000

    readonly property int rowH: 96
    readonly property real pad: 14
    property bool paged: false          // more rows than fit: the bubble carries the page arrows

    function open() { place(); visible = true }
    function close() { visible = false }
    onChoicesChanged: if (visible) place()
    onWidthChanged: if (visible) place()
    onHeightChanged: if (visible) place()

    // Where the bubble goes: below the anchor, above it when it does not fit below, centred without an anchor.
    function place() {
        var w = Math.min(width - 60, 700)
        var need = choices.length * rowH
        var chrome = 2 * pad + 2 * Style.heavyWidth
        var pager = Math.floor((height - 2 * chrome) / rowH) * rowH < need ? 110 : 0   // room for the arrows when it must page
        paged = pager > 0
        var cx = width / 2, top = 40, below = true, gapPx = 22
        if (anchor) {
            var a = anchor.mapToItem(root, 0, 0)
            cx = a.x + anchor.width / 2
            var spaceBelow = height - (a.y + anchor.height + gapPx) - 30
            var spaceAbove = a.y - gapPx - 30
            below = spaceBelow >= Math.min(need + chrome + pager, 6 * rowH) || spaceBelow >= spaceAbove
            var avail = below ? spaceBelow : spaceAbove
            var rows = Math.max(1, Math.min(choices.length, Math.floor((avail - chrome - pager) / rowH)))
            bubble.height = rows * rowH + chrome + pager
            top = below ? a.y + anchor.height + gapPx : a.y - gapPx - bubble.height
        } else {
            var avail2 = height - 120
            var rows2 = Math.max(1, Math.min(choices.length, Math.floor((avail2 - chrome - pager) / rowH)))
            bubble.height = rows2 * rowH + chrome + pager
            top = (height - bubble.height) / 2
        }
        bubble.width = w
        bubble.x = Math.max(20, Math.min(cx - w / 2, width - w - 20))
        bubble.y = top
        bubble.pointerX = cx - bubble.x
        bubble.pointerUp = below
        bubble.showPointer = anchor !== null
    }

    // a tap outside the bubble closes it and goes no further
    MouseArea { anchors.fill: parent; onClicked: root.close() }

    PopupBubble {
        id: bubble
        MouseArea { anchors.fill: parent }   // taps inside the bubble are not "outside"

        ListView {
            id: list
            x: root.pad + Style.heavyWidth
            y: root.pad + Style.heavyWidth
            width: parent.width - 2 * (root.pad + Style.heavyWidth)
            height: Math.floor((bubble.height - 2 * (root.pad + Style.heavyWidth) - (root.paged ? 110 : 0)) / root.rowH) * root.rowH
            clip: true
            interactive: false
            boundsBehavior: Flickable.StopAtBounds
            model: root.choices
            delegate: Item {
                id: row
                width: list.width
                height: root.rowH
                readonly property var choice: modelData
                property string text: choice.label
                signal clicked()
                onClicked: { root.close(); root.chosen(choice.value) }
                Rectangle { anchors.fill: parent; color: Style.pressed; visible: tap.pressed }
                Icon {
                    id: mark
                    x: 6
                    anchors.verticalCenter: parent.verticalCenter
                    size: 40
                    source: choice.value === root.current ? "check" : ""
                }
                Text {
                    anchors { left: parent.left; leftMargin: 64; right: parent.right; rightMargin: 6; verticalCenter: parent.verticalCenter }
                    elide: Text.ElideRight
                    font.pixelSize: Style.bodySize
                    font.bold: choice.value === root.current
                    color: Style.ink
                    text: choice.label
                }
                RowDivider { objectName: "rowDivider"; anchors.bottom: parent.bottom; width: parent.width }
                MouseArea { id: tap; anchors.fill: parent; onClicked: row.clicked() }
            }
        }
        Item {
            id: pager
            visible: root.paged
            x: root.pad + Style.heavyWidth
            width: parent.width - 2 * (root.pad + Style.heavyWidth)
            y: parent.height - Style.heavyWidth - root.pad - 96
            height: 96
            PageButtons { anchors.centerIn: parent; target: list; snap: root.rowH }
        }
    }
}
