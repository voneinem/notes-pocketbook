import QtQuick
import QtQuick.Controls
import QtQuick.Window

// Spec 010 (US4): the real ReaderPage in the look of the device's bars: a top bar with Back, the note title and Edit, a caption
// line, a bottom bar with the Text size tool and the page controls with "n / total", and the text size as a slider with round
// minus/plus buttons and its size in points. Hosted in a StackView, driven with real mouse events against the stubs.
Window {
    id: win
    width: 1404; height: 1872
    visible: true
    property int failures: 0
    property var page: null
    property var queue: []
    // the height of the text area of the 1.5.0 reader at 1404 x 1872 (checklists/baseline.md)
    readonly property int baselineFlickHeight: 1666

    function check(cond, what) {
        if (cond) console.log("ok:   " + what)
        else { failures++; console.log("FAIL: " + what) }
    }
    function eq(actual, expected, what) {
        if (actual === expected) console.log("ok:   " + what)
        else { failures++; console.log("FAIL: " + what + "  (got " + JSON.stringify(actual) + ", expected " + JSON.stringify(expected) + ")") }
    }
    function after(ms, fn) { queue.push({ ms: ms, fn: fn }) }
    Timer {
        id: runner
        onTriggered: {
            var s = win.queue.shift()
            try { s.fn() } catch (e) { win.failures++; console.log("FAIL: exception " + e) }
            if (win.queue.length > 0) { runner.interval = win.queue[0].ms; runner.start() }
            else Qt.exit(win.failures)
        }
    }
    function findAll(item, pred, out) {
        if (!item) return out
        if (out.indexOf(item) >= 0) return out
        if (pred(item)) out.push(item)
        var kids = item.children || []
        for (var i = 0; i < kids.length; ++i) findAll(kids[i], pred, out)
        if (item.contentItem && item.contentItem !== item) findAll(item.contentItem, pred, out)
        return out
    }
    function tapAt(p) { pointer.press(p.x, p.y); pointer.release(p.x, p.y) }
    function tapItem(item) { tapAt(item.mapToItem(null, item.width / 2, item.height / 2)) }
    function control(text) { return findAll(page, function(i) { return i.text === text && i.clicked !== undefined && i.visible }, [])[0] }
    function named(name) { return findAll(page, function(i) { return i.objectName === name }, [])[0] }
    function inkButtons() { return findAll(page, function(i) { return i.visible && String(i).indexOf("InkButton") === 0 }, []) }
    function iconNamed(item, name) { return findAll(item, function(i) { return i.ready !== undefined && i.source === name }, [])[0] }
    function textOf(name) { var t = named(name); return t ? t.text : undefined }

    function paragraph(n) {
        return { kind: "paragraph", line: n, level: 0, ordered: false, number: 0, depth: 0, task: false, checked: false,
                 html: "Paragraph " + n + " of the note, long enough to take some room on the page.", links: [] }
    }
    function note() {
        var blocks = []
        for (var i = 0; i < 120; ++i) blocks.push(paragraph(i))
        return { localId: 1, title: "Plan", titleText: "Plan", hasTitle: true, category: "Work", favorite: true, modified: 0, pending: false, blocks: blocks }
    }

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: Item {}
        pushEnter: Transition {}
        pushExit: Transition {}
        popEnter: Transition {}
        popExit: Transition {}
    }

    Component.onCompleted: {
        bridge.noteToOpen = note()
        after(300, function() { page = stack.push(pageUrl, { noteId: 1 }) })
        after(500, function() {
            check(page !== null, "the reader loads")
            var title = named("topBarTitle")
            check(title !== undefined, "there is a top bar with a title")
            eq(title.text, "Plan", "the note's title is in it")
            check(control("Back") !== undefined && control("Edit") !== undefined, "Back and Edit are controls of the bar")
            check(control("Back").mapToItem(page, 0, 0).y < 112 && control("Edit").mapToItem(page, 0, 0).y < 112, "both sit in the top bar")
            eq(inkButtons().length, 0, "no bordered text button is on the page")
            check(!control("A-") && !control("A+"), "the old A- and A+ buttons are gone")

            var cap = named("captionRow")
            check(cap !== undefined && cap.visible, "the caption line is shown for a favorite in a category")
            check(cap !== undefined && findAll(cap, function(i) { return String(i.text).indexOf("Work") >= 0 }, []).length > 0, "it names the category")
            check(cap !== undefined && iconNamed(cap, "star-filled") !== undefined, "and marks the favorite with the star")

            check(control("Text size") !== undefined, "the bottom bar has the Text size tool")
            check(control("Page up") !== undefined && control("Page down") !== undefined, "and Page up and Page down")
            var ind = textOf("pageIndicator")
            check(/^1 \/ \d+$/.test(ind), "the indicator reads 1 / N (" + ind + ")")
            var total = parseInt(String(ind).split(" / ")[1])
            check(total >= 2, "a long note has several pages")
            check(named("flick").height >= win.baselineFlickHeight, "the text area is at least as high as in 1.5.0 (" + named("flick").height + " >= " + win.baselineFlickHeight + ")")
            win.total = total
        })
        after(100, function() { tapItem(control("Page down")) })
        after(200, function() {
            eq(textOf("pageIndicator"), "2 / " + win.total, "after Page down the indicator reads 2 / N")
            tapItem(control("Page up"))
        })
        after(200, function() { eq(textOf("pageIndicator"), "1 / " + win.total, "and back to 1 / N after Page up") })
        after(100, function() {
            // the edge tap zones still page
            tapAt(Qt.point(win.width - 20, 900))
        })
        after(200, function() {
            check(named("flick").contentY > 0, "a tap at the right edge pages forward")
            tapAt(Qt.point(20, 900))
        })
        after(200, function() { eq(named("flick").contentY, 0, "a tap at the left edge pages back") })

        // ---- the text size ----------------------------------------------------------------------------------------
        after(100, function() {
            var panel = named("textSizePanel")
            check(panel !== undefined && !panel.visible, "the text size panel is closed at first")
            tapItem(control("Text size"))
        })
        after(200, function() {
            var panel = named("textSizePanel")
            check(panel.visible, "the Text size tool opens the panel")
            var slider = named("textSizeSlider")
            eq(slider.index, 1, "the slider is on the middle stop for medium")
            eq(slider.stops, 3, "with three stops")
            check(findAll(panel, function(i) { return i.text === "9 pt" && i.visible }, []).length > 0, "the label reads 9 pt")
            tapItem(control("Increase"))
        })
        after(200, function() {
            eq(bridge.fontSize, "large", "Increase sets the large size")
            check(findAll(named("textSizePanel"), function(i) { return i.text === "12 pt" && i.visible }, []).length > 0, "and the label reads 12 pt")
            check(!control("Increase").enabled, "Increase is off at the largest size")
            tapItem(control("Decrease"))
        })
        after(200, function() { tapItem(control("Decrease")) })
        after(200, function() {
            eq(bridge.fontSize, "small", "Decrease twice sets the small size")
            check(findAll(named("textSizePanel"), function(i) { return i.text === "7 pt" && i.visible }, []).length > 0, "and the label reads 7 pt")
            check(!control("Decrease").enabled, "Decrease is off at the smallest size")
            var track = named("sliderTrack")
            var mid = track.mapToItem(null, track.width / 2, track.height / 2)
            tapAt(mid)
        })
        after(200, function() {
            eq(bridge.fontSize, "medium", "a tap on the middle of the track sets the middle size")
            tapItem(control("Text size"))
        })
        after(200, function() {
            check(!named("textSizePanel").visible, "a second tap on the tool closes the panel")
            tapItem(control("Edit"))
        })
        after(300, function() {
            eq(stack.depth, 3, "Edit opens a page")
            eq(stack.currentItem.noteId, 1, "the editor of this note")
            stack.pop()
        })
        after(300, function() {
            tapItem(control("Back"))
        })
        after(300, function() { eq(stack.depth, 1, "Back leaves the reader") })

        runner.interval = queue[0].ms
        runner.start()
    }
    property int total: 0
}
