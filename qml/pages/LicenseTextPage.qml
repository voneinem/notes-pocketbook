import QtQuick
import QtQuick.Controls
import ".."

// One license text (spec 003): a title, a few "Label: value" lines, then the text as paragraphs, paged with the
// page buttons. Used for the app's license and for every third-party component. `content` is the map
// {title, header, paragraphs, available} from the About bridge. Rotating the reader keeps the place.
Item {
    id: page
    property var content: ({ title: "", header: [], paragraphs: [], available: false })

    // Index of the paragraph at the top of the view; restored after the window changes size.
    property int topIndex: 0
    property bool restoring: false

    TopBar {
        id: topBar
        title: page.content.title
        leftItems: [ IconButton { text: qsTr("Back"); icon: "chevron-left"; onClicked: page.StackView.view.pop() } ]
    }

    ListView {
        id: list
        anchors { left: parent.left; right: parent.right; top: topBar.bottom; bottom: footer.top; topMargin: 16; leftMargin: 40; rightMargin: 40 }
        clip: true
        model: page.content.paragraphs
        interactive: false
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: false
        spacing: 18

        header: Column {
            width: list.width
            spacing: 6
            bottomPadding: 18
            Repeater {
                model: page.content.header
                Text {
                    width: list.width
                    wrapMode: Text.Wrap
                    font.pixelSize: 28
                    color: "#000000"
                    // "Label: value" with the label in bold
                    textFormat: Text.StyledText
                    text: {
                        var s = String(modelData)
                        var i = s.indexOf(": ")
                        var esc = function (t) { return t.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;") }
                        return i < 0 ? esc(s) : "<b>" + esc(s.substring(0, i + 1)) + "</b> " + esc(s.substring(i + 2))
                    }
                }
            }
        }

        delegate: Text {
            width: list.width
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            font.pixelSize: 26
            color: "#000000"
            text: modelData
        }

        onContentYChanged: {
            if (page.restoring) return
            var i = indexAt(10, contentY + 4)
            if (i >= 0) page.topIndex = i
            else if (contentY <= originY + 1) page.topIndex = 0
        }
        onWidthChanged: restoreTimer.restart()
        onHeightChanged: restoreTimer.restart()
    }

    // After a rotation the layout changes under the reader; go back to the paragraph they were reading.
    Timer {
        id: restoreTimer
        interval: 50
        onTriggered: {
            if (page.topIndex <= 0) return
            page.restoring = true
            list.positionViewAtIndex(page.topIndex, ListView.Beginning)
            page.restoring = false
        }
    }

    Item {
        id: footer
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: Style.touchMin
        PageButtons { anchors.centerIn: parent; target: list }
    }
}
