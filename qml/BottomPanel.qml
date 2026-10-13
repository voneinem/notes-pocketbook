import QtQuick

// A row of tools under a hairline (spec 010): the tools share the width in equal cells, so they are spread evenly whatever
// their number; a tool that is not visible leaves its cell to the others.
Item {
    id: root
    property alias tools: row.data

    width: parent ? parent.width : 0
    height: Style.touchMin + Style.hairlineWidth

    property bool relayouting: false
    property var watched: []
    function relayout() {
        if (relayouting) return
        relayouting = true
        var shown = []
        for (var i = 0; i < row.children.length; ++i) {
            var c = row.children[i]
            if (watched.indexOf(c) < 0 && c.visibleChanged) { watched.push(c); c.visibleChanged.connect(relayout) }
            if (c.visible) shown.push(c)
        }
        for (var j = 0; j < shown.length; ++j) shown[j].width = shown.length > 0 ? root.width / shown.length : 0
        relayouting = false
    }
    onWidthChanged: relayout()
    Component.onCompleted: relayout()

    Rectangle { objectName: "bottomPanelRule"; y: 0; width: parent.width; height: Style.hairlineWidth; color: Style.hairline }
    Row {
        id: row
        y: Style.hairlineWidth
        width: parent.width
        height: Style.touchMin
        onChildrenChanged: root.relayout()
    }
}
