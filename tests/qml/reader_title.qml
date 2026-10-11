import QtQuick
import QtQuick.Controls
import QtQuick.Window

// Spec 005 US3: the reading page shows the stored title above the text, in full, once, and nothing for a note
// without a title.
Window {
    id: win
    width: 1080; height: 1335
    visible: true
    property int failures: 0

    function check(cond, what) {
        if (cond) console.log("ok:   " + what)
        else { failures++; console.log("FAIL: " + what) }
    }
    function eq(actual, expected, what) {
        if (actual === expected) console.log("ok:   " + what)
        else { failures++; console.log("FAIL: " + what + "  (got " + JSON.stringify(actual) + ", expected " + JSON.stringify(expected) + ")") }
    }
    function findByName(item, name) {
        if (!item) return null
        if (item.objectName === name) return item
        var kids = item.children || []
        for (var i = 0; i < kids.length; ++i) { var r = findByName(kids[i], name); if (r) return r }
        if (item.contentItem && item.contentItem !== item) return findByName(item.contentItem, name)
        return null
    }

    function note(title, hasTitle, firstLine) {
        return {
            localId: 1, title: hasTitle ? title : "(untitled)", titleText: title, hasTitle: hasTitle,
            category: "", favorite: false, modified: 0, pending: false,
            blocks: [ { kind: "heading", line: 0, level: 1, ordered: false, number: 0, depth: 0, task: false, checked: false, html: firstLine, links: [] },
                      { kind: "paragraph", line: 1, level: 0, ordered: false, number: 0, depth: 0, task: false, checked: false, html: "body text", links: [] } ]
        }
    }
    Loader { id: ld; anchors.fill: parent; active: false; source: pageUrl; onLoaded: item.noteId = 1 }

    function show(n) {
        ld.active = false
        bridge.noteToOpen = n
        ld.active = true
    }
    function titleBlock() { return findByName(ld.item, "titleBlock") }
    function bodyBlocks() { return ld.item.note.blocks }

    function run() {
        // a short title
        show(note("Groceries", true, "Not the title"))
        var tb = titleBlock()
        check(tb !== null, "the title block exists")
        check(tb.visible && tb.height > 0, "a note with a title shows it")
        eq(tb.title, "Groceries", "it shows the stored title")
        var oneLine = tb.height
        eq(bodyBlocks().length, 2, "the text blocks are unchanged")

        // the first line repeats the title: both are shown (no de-duplication)
        show(note("Plan", true, "Plan"))
        tb = titleBlock()
        check(tb.visible, "the title block is shown although the first line repeats it")
        eq(bodyBlocks()[0].html, "Plan", "and the first line is still in the text")

        // a 255-character title wraps in full
        var lengthy = new Array(256).join("W")
        show(note(lengthy, true, "x"))
        tb = titleBlock()
        eq(tb.title.length, 255, "a 255-character title is passed on whole")
        check(tb.height > oneLine * 3, "it wraps onto several lines instead of being cut off")
        check(tb.implicitHeight === tb.height, "the block is as tall as its text")

        // the title follows the font size
        var before = tb.height
        bridge.fontPx = 30
        check(tb.height < before, "a smaller reading size gives a smaller title")
        bridge.fontPx = 38

        // no title: nothing, and no gap
        show(note("", false, "x"))
        tb = titleBlock()
        check(!tb.visible && tb.height === 0, "a note without a title shows no title block and leaves no gap")

        Qt.exit(win.failures)
    }

    Timer { interval: 300; running: true; onTriggered: win.run() }
}
