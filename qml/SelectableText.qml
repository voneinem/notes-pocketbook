import QtQuick

// Read-only styled text whose characters can be selected (spec 005): the reading page's text blocks and its title.
// `html` is the escaped markup the bridge builds (<b>, <i>, <a href="lN"><u>..</u></a>); the whole text sits in a
// pre-wrap span so that the document text equals the note's text byte for byte (no collapsed spaces), which is what
// the offsets reported here and given to Copy rely on. The selection is shown inverted (white on black).
// The page drives it: selFrom/selTo are UTF-16 offsets into the text (-1 = nothing). A press-and-hold selects the
// word under the finger and reports it (wordHeld); a tap on a link is reported (linkTapped), any other tap with its
// offset (tapped). Mouse input (what the reader's touch screen delivers), no gesture handlers.
TextEdit {
    id: edit
    objectName: "selectable"
    property string html: ""
    property int selFrom: -1
    property int selTo: -1
    signal wordHeld(int start, int end)
    signal tapped(int pos)
    signal linkTapped(string link)

    readOnly: true
    selectByMouse: false
    activeFocusOnPress: false
    persistentSelection: true
    wrapMode: TextEdit.Wrap
    textFormat: TextEdit.RichText
    color: "#000000"
    selectionColor: "#000000"
    selectedTextColor: "#ffffff"
    text: "<style>a { color: #000000; }</style><span style=\"white-space: pre-wrap\">" + html + "</span>"

    // (selecting a rich text raises textChanged on its own, so the markup change is watched through `html`)
    property bool applying: false
    function applySelection() {
        if (applying) return
        applying = true
        if (selFrom < 0 || selTo <= selFrom) deselect()
        else select(selFrom, selTo)
        applying = false
    }
    onSelFromChanged: applySelection()
    onSelToChanged: applySelection()
    onHtmlChanged: applySelection()
    Component.onCompleted: applySelection()

    MouseArea {
        anchors.fill: parent
        pressAndHoldInterval: 600
        property bool held: false
        onPressed: held = false
        onPressAndHold: function(mouse) {
            held = true
            edit.cursorPosition = edit.positionAt(mouse.x, mouse.y)
            edit.selectWord()
            edit.wordHeld(edit.selectionStart, edit.selectionEnd)
        }
        onClicked: function(mouse) {
            if (held) return
            var link = edit.linkAt(mouse.x, mouse.y)
            if (link !== "") edit.linkTapped(link)   // a link is followed, it is not a selection tap
            else edit.tapped(edit.positionAt(mouse.x, mouse.y))
        }
    }
}
