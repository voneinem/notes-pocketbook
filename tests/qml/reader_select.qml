import QtQuick
import QtQuick.Controls
import QtQuick.Window

// Spec 005 US4: selecting and copying on the reading page with real mouse events: a long press selects a word and shows
// two markers and a bubble (Copy, Select all); the markers move the ends of the selection; a tap outside ends it; Copy
// hands the range to the bridge. The footer only has the page buttons.
Window {
    id: win
    width: 1080; height: 1335
    visible: true
    property int failures: 0
    property var queue: []
    property var page: null

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
    function scene(item, x, y) { return item.mapToItem(null, x, y) }
    function tapAt(p) { pointer.press(p.x, p.y); pointer.release(p.x, p.y) }
    function longPressAt(p) {
        queue.unshift({ ms: 1000, fn: function() { pointer.release(p.x, p.y) } })
        queue.unshift({ ms: 20, fn: function() { pointer.press(p.x, p.y) } })
    }
    // the scene point of character `offset` of a text unit
    function charPoint(u, offset) {
        var it = page.unitItem(u)
        var r = it.rectAt(offset)
        return scene(it, r.x + 3, r.y + r.height / 2)
    }
    // a point on the text of one of `units` that the open bubble (with its pointer) does not cover: the bubble's size is not
    // what these tests are about, so they tap wherever it is not
    function pointOutsideMenu(units) {
        var m = menu()
        for (var i = 0; i < units.length; ++i) {
            var c = charPoint(units[i], 2)
            var cands = [c, Qt.point(900, c.y)]
            for (var j = 0; j < cands.length; ++j) {
                var p = cands[j]
                if (!m || !m.visible) return p
                var tl = m.mapToItem(null, 0, 0)
                var covered = p.x >= tl.x - 40 && p.x <= tl.x + m.width + 40 && p.y >= tl.y - 40 && p.y <= tl.y + m.height + 40
                if (!covered) return p
            }
        }
        return charPoint(units[0], 2)
    }
    function menu() { return findAll(page, function(i) { return i.objectName === "selectionMenu" }, [])[0] }
    function menuLabels() {
        var m = menu()
        if (!m || !m.visible) return []
        return findAll(m, function(i) { return i.text !== undefined && i.clicked !== undefined && i.visible }, []).map(function(b) { return b.text })
    }
    function chooseInMenu(label) {
        var b = findAll(menu(), function(i) { return i.text === label && i.clicked !== undefined }, [])[0]
        if (!b) { failures++; console.log("FAIL: no menu button " + label); return }
        tapAt(scene(b, b.width / 2, b.height / 2))
    }
    function handles() { return findAll(page, function(i) { return i.objectName === "selectionHandle" && i.visible }, []) }
    function selectedTexts() {
        return findAll(page, function(i) { return i.objectName === "selectable" }, []).map(function(i) { return i.selectedText })
    }

    function text(kind, html, extra) {
        var b = { kind: kind, line: 0, level: 0, ordered: false, number: 0, depth: 0, task: false, checked: false, html: html, links: [] }
        for (var k in extra) b[k] = extra[k]
        return b
    }
    function note() {
        return {
            localId: 1, title: "Plan", titleText: "Plan", hasTitle: true, category: "", favorite: false, modified: 0, pending: false,
            blocks: [
                text("heading", "Head", { level: 1 }),
                text("paragraph", "some <b>bold</b> words and a <a href=\"l0\"><u>link</u></a>", { links: [{ url: "https://example.org", confirm: false }] }),
                text("blank", ""),
                { kind: "tablerow", line: 3, unsplittable: true, header: true, lastRow: true, weights: [1],
                  cols: [{ html: "cell", links: [], align: "left" }] },
                text("paragraph", "the last paragraph of the note")
            ]
        }
    }
    Loader { id: ld; anchors.fill: parent; active: false; source: pageUrl; onLoaded: item.noteId = 1 }

    Component.onCompleted: {
        after(300, function() {
            bridge.noteToOpen = note()
            ld.active = true
            page = ld.item
            check(page !== null, "the reading page loads")
            // units: 0 title, 1 heading, 2 paragraph, 3 blank, 4 table row, 5 last paragraph
            eq(page.unitCount, 6, "the title and five blocks are six units")
            var labels = findAll(page, function(i) { return i.text !== undefined && i.clicked !== undefined && i.visible }, []).map(function(b) { return b.text })
            console.log("buttons: " + labels.join(" | "))
            for (var bad of ["All", "Extend", "Copy", "Select all"]) check(labels.indexOf(bad) < 0, "no " + bad + " button in the footer")
            check(labels.indexOf("Page up") >= 0 && labels.indexOf("Page down") >= 0, "the page buttons are there")
            var footer = findAll(page, function(i) { return i.objectName === "footer" }, [])[0]
            // the bottom bar (spec 010) spans the page; what it holds must fit inside it
            var inside = findAll(footer, function(i) { return i.text !== undefined && i.clicked !== undefined && i.visible }, [])
            check(inside.length >= 3 && inside.every(function(i) { var x = i.mapToItem(page, 0, 0).x; return x >= 0 && x + i.width <= page.width }), "the bottom bar's controls fit the page width")
        })

        // ---- a long press selects the word and shows the markers and the bubble ----
        after(100, function() { longPressAt(charPoint(2, 7)) })
        after(200, function() {
            eq(page.selection.fromOffset + "-" + page.selection.toOffset, "5-9", "a long press selects the word")
            check(selectedTexts().indexOf("bold") >= 0, "the shown selection is exactly the word")
            eq(handles().length, 2, "two markers are shown")
            eq(menuLabels().join("|"), "Copy|Select all", "the bubble offers Copy and Select all")
            if (typeof arg_shot !== "undefined") pointer.shot(arg_shot + "_reader.png")
        })

        // ---- Copy from the bubble ----
        after(100, function() { chooseInMenu("Copy") })
        after(100, function() {
            eq(bridge.lastCopyArgs, "1,2,5,2,9", "Copy gives the range to the bridge")
            eq(clipboard.text(), "COPIED", "and puts what it returns on the clipboard")
            check(page.selection === null, "the selection ends after Copy")
            eq(menuLabels().length, 0, "the bubble closes")
            eq(handles().length, 0, "and the markers go")
        })

        // ---- drag the end marker to take in more text ----
        after(100, function() { longPressAt(charPoint(2, 7)) })
        after(200, function() {
            var end = handles().filter(function(h) { return !h.atStart })[0]
            check(end !== undefined, "there is an end marker")
            var it = page.unitItem(2)
            var r = it.rectAt(15)
            var from = scene(end, end.width / 2, end.height / 2)
            var to = scene(it, r.x + end.width / 2, r.y + r.height + end.height / 2)
            pointer.press(from.x, from.y)
            pointer.move((from.x + to.x) / 2, (from.y + to.y) / 2)
            pointer.move(to.x, to.y)
            eq(menuLabels().length, 0, "the bubble is hidden while a marker is dragged")
            pointer.release(to.x, to.y)
        })
        after(100, function() {
            eq(page.selection.fromOffset, 5, "the start stays")
            check(page.selection.toOffset >= 14 && page.selection.toOffset <= 16, "the end follows the marker (to " + page.selection.toOffset + ")")
            check(menuLabels().length > 0, "the bubble comes back when the marker is let go")
            // the start marker, back to the beginning of the paragraph
            var st = handles().filter(function(h) { return h.atStart })[0]
            var it = page.unitItem(2)
            var r = it.rectAt(0)
            var from = scene(st, st.width / 2, st.height / 2)
            var to = scene(it, r.x - st.width / 2 + 1, r.y + r.height + st.height / 2)
            pointer.press(from.x, from.y)
            pointer.move(to.x, to.y)
            pointer.release(to.x, to.y)
        })
        after(100, function() {
            eq(page.selection.fromUnit + ":" + page.selection.fromOffset, "2:0", "the start marker moves the start")
        })

        // ---- a marker dragged into another unit extends the selection over it ----
        after(100, function() {
            var end = handles().filter(function(h) { return !h.atStart })[0]
            var it = page.unitItem(5)
            var r = it.rectAt(8)
            var from = scene(end, end.width / 2, end.height / 2)
            var to = scene(it, r.x + end.width / 2, r.y + r.height + end.height / 2)
            pointer.press(from.x, from.y)
            pointer.move((from.x + to.x) / 2, (from.y + to.y) / 2)
            pointer.move(to.x, to.y)
            pointer.release(to.x, to.y)
        })
        after(100, function() {
            eq(page.selection.toUnit, 5, "dragging down into a later block selects through it")
            check(page.unitItem(3).selected && page.unitItem(4).selected, "the blank line and the table row in between are selected whole")
        })

        // ---- a tap outside ends it ----
        after(100, function() { tapAt(pointOutsideMenu([1, 0])) })
        after(100, function() {
            check(page.selection === null, "a tap outside ends the selection")
            eq(menuLabels().length, 0, "and the bubble")
        })

        // ---- a tap on empty space (below the text, beside the buttons) ends it too ----
        after(100, function() { longPressAt(charPoint(2, 7)) })
        after(200, function() { check(page.selection !== null, "selected again"); tapAt(Qt.point(540, 900)) })
        after(100, function() {
            check(page.selection === null, "a tap on empty space below the text ends the selection")
            eq(menuLabels().length, 0, "and closes the bubble")
            longPressAt(charPoint(2, 7))
        })
        after(200, function() { tapAt(Qt.point(900, 60)) })
        after(100, function() { check(page.selection === null, "a tap on empty space in the header ends it") })

        // ---- Select all from the bubble ----
        after(100, function() { longPressAt(charPoint(5, 6)) })
        after(200, function() { chooseInMenu("Select all") })
        after(100, function() {
            eq(page.selection.fromUnit + ":" + page.selection.toUnit, "0:5", "Select all runs from the title to the last block")
            eq(menuLabels().join("|"), "Copy", "the bubble then only offers Copy")
            chooseInMenu("Copy")
        })
        after(100, function() { eq(bridge.lastCopyArgs.split(",").slice(0, 3).join(","), "1,0,0", "Copy of everything starts at the title") })

        // ---- a whole unit by long press ----
        after(100, function() {
            var it = page.unitItem(4)
            longPressAt(scene(it, 40, it.height / 2))
        })
        after(200, function() {
            eq(page.selection.fromUnit + ":" + page.selection.toUnit, "4:4", "a long press on a table row selects the row")
            check(page.unitItem(4).selected, "and shows it")
            tapAt(charPoint(5, 3))
        })
        after(100, function() { check(page.selection === null, "a tap elsewhere ends it") })

        // ---- the title ----
        after(100, function() { longPressAt(charPoint(0, 2)) })
        after(200, function() {
            check(selectedTexts().indexOf("Plan") >= 0, "the title can be selected like text")
            page.clearSelection()
        })

        // ---- links: the first tap only ends a selection ----
        after(100, function() {
            var before = device.opened
            page.wordHeld(2, 5, 9)
            page.followLink(page.note.blocks[1], "l0")
            eq(device.opened, before, "a link is not followed while a selection exists")
            check(page.selection === null, "that tap ended the selection")
            page.lastClearMs = 0
            page.followLink(page.note.blocks[1], "l0")
            eq(device.opened, before + 1, "without a selection the link opens as before")
        })
        // a link tapped with the finger opens it
        after(100, function() {
            var before = device.opened
            var it = page.unitItem(2)
            var r = it.rectAt(24)   // inside "link"
            tapAt(scene(it, r.x + 3, r.y + r.height / 2))
            eq(device.opened, before + 1, "tapping a link with the finger follows it")
        })

        // ---- reload and font size end it ----
        after(100, function() {
            page.wordHeld(2, 5, 9)
            bridge.fontSize = "large"
            check(page.selection === null, "changing the font size ends the selection")
            page.wordHeld(2, 5, 9)
            page.load()
            check(page.selection === null, "a reload ends the selection")
        })
        runner.interval = queue[0].ms
        runner.start()
    }
}
