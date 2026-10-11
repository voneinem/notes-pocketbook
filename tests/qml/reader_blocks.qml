import QtQuick
import QtQuick.Controls
import QtQuick.Window

// Spec 006: how the reading page shows what feature 006 adds (code boxes, headings, quotes, info boxes, details),
// driven with real mouse events like reader_select.qml.
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
        var b = { kind: kind, line: 0, level: 0, ordered: false, number: 0, depth: 0, task: false, checked: false,
                  html: html, links: [], quote: 0, quoteFirst: false, quoteLast: false, codeFirst: false, codeLast: false,
                  callout: "", detailsId: -1, detailsOpen: false }
        for (var k in extra) b[k] = extra[k]
        return b
    }
    function note(blocks) {
        return { localId: 1, title: "", titleText: "", hasTitle: false, category: "", favorite: false, modified: 0,
                 pending: false, blocks: blocks }
    }
    function visibleByName(item, name) {
        return findAll(item, function(i) { return i.objectName === name && i.visible }, [])
    }
    FontMetrics { id: fm }
    Loader { id: ld; anchors.fill: parent; active: false; source: pageUrl; onLoaded: item.noteId = 1 }

    function codeNote() {
        return note([
            text("paragraph", "before the code"),
            text("fence", "", { codeFirst: true, unsplittable: true }),
            text("code", "hello world", { unsplittable: true }),
            text("code", "  indented: 1", { unsplittable: true }),
            text("fence", "", { codeLast: true, unsplittable: true }),
            text("paragraph", "after the code")
        ])
    }

    Component.onCompleted: {
        // ---- US1: the code box ----
        after(300, function() {
            bridge.noteToOpen = codeNote()
            ld.active = true
            page = ld.item
            check(page !== null, "the reading page loads")
            eq(page.unitCount, 6, "six units")
            var sel = findAll(page.unitItem(2), function(i) { return i.objectName === "selectable" }, [])[0]
            check(sel !== undefined, "a code line has a text item")
            fm.font = sel.font
            check(fm.advanceWidth("i") === fm.advanceWidth("M") && fm.advanceWidth("i") > 0, "code is set in a fixed-pitch font")
            eq(sel.font.pixelSize, Math.round(38 * 0.9), "code is 0.9 times the reading size")
            var para = findAll(page.unitItem(0), function(i) { return i.objectName === "selectable" }, [])[0]
            fm.font = para.font
            check(fm.advanceWidth("i") !== fm.advanceWidth("M"), "ordinary text is not fixed-pitch")
            eq(visibleByName(page.unitItem(1), "codeTop").length, 1, "the opening fence draws the top edge")
            eq(visibleByName(page.unitItem(2), "codeTop").length, 0, "a code line inside the box has no top edge")
            eq(visibleByName(page.unitItem(2), "codeBottom").length, 0, "and no bottom edge")
            eq(visibleByName(page.unitItem(4), "codeBottom").length, 1, "the closing fence draws the bottom edge")
            eq(visibleByName(page.unitItem(0), "codeTop").length, 0, "text outside the box has no edge")
            check(visibleByName(page.unitItem(2), "codeSide").length >= 1, "every line of the box has its sides")
            check(visibleByName(page.unitItem(1), "codeSide").length >= 1 && visibleByName(page.unitItem(4), "codeSide").length >= 1,
                  "and so do the fence strips")
            check(page.unitItem(1).height < 30, "a fence strip is thin")
            if (typeof arg_shot !== "undefined") pointer.shot(arg_shot + "_code.png")
        })
        after(300, function() {
            eq(page.unsplittable.length, 4, "the fences and code lines are listed as not to be cut by a page break")
        })

        // ---- a fence is selected whole by a long press ----
        after(100, function() { var it = page.unitItem(1); longPressAt(scene(it, it.width / 2, it.height / 2)) })
        after(200, function() {
            check(page.selection !== null, "a long press on the fence strip selects")
            eq(page.selection.fromUnit + ":" + page.selection.toUnit, "1:1", "the fence unit")
            check(page.unitItem(1).selected, "and shows it selected whole")
            page.clearSelection()
        })

        // ---- a word inside a code line is selected as a character range ----
        after(100, function() { longPressAt(charPoint(2, 7)) })
        after(200, function() {
            eq(page.selection.fromUnit + ":" + page.selection.toUnit, "2:2", "a long press inside code selects in that line")
            eq(page.selection.fromOffset + "-" + page.selection.toOffset, "6-11", "the word under the finger")
            chooseInMenu("Copy")
        })
        after(100, function() {
            eq(bridge.lastCopyArgs, "1,2,6,2,11", "Copy asks the bridge for exactly that range")
            page.clearSelection()
        })

        // ---- Select all covers the fences too ----
        after(100, function() { page.selectAll() })
        after(100, function() {
            eq(page.selection.fromUnit + ":" + page.selection.toUnit, "0:5", "Select all runs over the whole box")
            check(page.unitItem(1).selected && page.unitItem(4).selected, "both fence strips are shown selected")
            page.clearSelection()
        })

        // ---- US2: headings ----
        after(100, function() {
            page.clearSelection()
            var blocks = []
            for (var lvl = 1; lvl <= 6; ++lvl) {
                blocks.push(text("heading", "Heading " + lvl, { level: lvl }))
                blocks.push(text("paragraph", "text under " + lvl))
            }
            blocks.push(text("heading", "Last heading", { level: 2 }))
            bridge.noteToOpen = note(blocks)
            page.load()
        })
        after(300, function() {
            var scale = [1.9, 1.55, 1.3, 1.15, 1.1, 1.05]
            var sizes = []
            for (var lvl = 1; lvl <= 6; ++lvl) {
                var it = page.unitItem((lvl - 1) * 2)
                var t = findAll(it, function(i) { return i.objectName === "selectable" }, [])[0]
                sizes.push(t.font.pixelSize)
                eq(t.font.pixelSize, Math.round(38 * scale[lvl - 1]), "level " + lvl + " has its own size")
                check(t.font.bold === true, "level " + lvl + " is bold")
            }
            check(sizes[0] > sizes[1] && sizes[1] > sizes[2] && sizes[2] > sizes[3], "levels 1 to 4 get smaller")
            check(sizes[1] - sizes[2] >= 8, "levels 2 and 3 differ clearly")
            var body = findAll(page.unitItem(1), function(i) { return i.objectName === "selectable" }, [])[0]
            check(sizes[5] > body.font.pixelSize, "even level 6 is larger than body text")
        })
        after(100, function() {
            bridge.fontPx = 44
        })
        after(200, function() {
            var scale = [1.9, 1.55, 1.3, 1.15, 1.1, 1.05]
            for (var lvl = 1; lvl <= 6; ++lvl) {
                var t = findAll(page.unitItem((lvl - 1) * 2), function(i) { return i.objectName === "selectable" }, [])[0]
                eq(t.font.pixelSize, Math.round(44 * scale[lvl - 1]), "after a larger reading size level " + lvl + " keeps its relation")
            }
            bridge.fontPx = 38
        })

        // ---- keep with next: a page break never falls between a heading and the line below it ----
        after(300, function() {
            var fl = findAll(page, function(i) { return i.objectName === "flick" }, [])[0]
            check(fl !== undefined, "the reading page has its flickable")
            var intervals = page.unsplittable
            for (var u = 0; u < page.unitCount; ++u) {
                var it = page.unitItem(u)
                if (page.note.blocks[u].kind !== "heading") continue
                var top = it.mapToItem(fl.contentItem, 0, 0).y
                var found = false
                for (var k = 0; k < intervals.length; ++k) {
                    var iv = intervals[k]
                    if (Math.abs(iv.top - top) < 1 && iv.bottom >= top + it.height + Math.round(38 * 1.4) - 1) found = true
                }
                check(found, "heading " + u + " has a keep-with-next interval")
            }
        })

        // ---- US4: quote and info box ----
        after(100, function() {
            var blocks = [
                text("paragraph", "a quote", { quote: 1, quoteFirst: true, quoteLast: true }),
                text("paragraph", "between"),
                text("callout", "", { quote: 2, quoteFirst: true, callout: "note" }),
                text("paragraph", "info text", { quote: 2 }),
                text("paragraph", "last info line", { quote: 2, quoteLast: true }),
                text("callout", "", { quote: 2, quoteFirst: true, callout: "tip" }),
                text("callout", "", { quote: 2, callout: "warning" }),
                text("callout", "", { quote: 2, callout: "important" }),
                text("callout", "", { quote: 2, quoteLast: true, callout: "caution" })
            ]
            bridge.noteToOpen = note(blocks)
            page.load()
        })
        after(300, function() {
            eq(visibleByName(page.unitItem(0), "quoteBar").length, 1, "a quote has a bar at its left")
            eq(visibleByName(page.unitItem(1), "quoteBar").length, 0, "a plain paragraph has none")
            eq(visibleByName(page.unitItem(3), "quoteBar").length, 0, "an info box has no quote bar")
            check(visibleByName(page.unitItem(3), "infoSide").length >= 1, "every line of an info box has its sides")
            eq(visibleByName(page.unitItem(2), "infoTop").length, 1, "the header draws the top edge")
            eq(visibleByName(page.unitItem(3), "infoTop").length, 0, "a middle line does not")
            eq(visibleByName(page.unitItem(4), "infoBottom").length, 1, "the last line draws the bottom edge")
            eq(visibleByName(page.unitItem(3), "infoBottom").length, 0, "a middle line draws no bottom edge")
            var names = ["Note", "Tip", "Warning", "Important", "Caution"]
            var units = [2, 5, 6, 7, 8]
            for (var i = 0; i < 5; ++i) {
                var lab = visibleByName(page.unitItem(units[i]), "calloutLabel")[0]
                check(lab !== undefined && lab.text === names[i], "the box kind " + names[i] + " is written out")
                check(lab !== undefined && lab.font.bold === true, names[i] + " is bold (no colour needed)")
            }
            check(!page.unitIsText(2), "an info header is a whole unit")
            if (typeof arg_shot !== "undefined") pointer.shot(arg_shot + "_quote.png")
        })

        // ---- US4: details ----
        after(100, function() {
            var blocks = [
                text("paragraph", "before"),
                text("marker", ""),
                text("summary", "Sum", { detailsId: 1 }),
                text("paragraph", "hidden one", { detailsId: 1 }),
                text("paragraph", "hidden two", { detailsId: 1 }),
                text("marker", ""),
                text("paragraph", "after the section")
            ]
            bridge.noteToOpen = note(blocks)
            page.load()
        })
        after(300, function() {
            var mark = visibleByName(page.unitItem(2), "summaryMark")[0]
            check(mark !== undefined && mark.text === "\u25B6", "a closed section shows the closed marker")
            check(!page.unitItem(3).visible && !page.unitItem(4).visible, "its content is not shown")
            eq(page.unitItem(1).height, 0, "a structural marker takes no room")
            check(!page.unitIsText(1), "and is a whole unit")
            var top6 = page.unitItem(6).mapToItem(null, 0, 0).y
            var top2 = page.unitItem(2).mapToItem(null, 0, 0).y
            var fl = findAll(page, function(i) { return i.objectName === "flick" }, [])[0]
            eq(page.unitAtY(page.unitItem(6).mapToItem(fl.contentItem, 0, 0).y + 1), 6, "hidden units are skipped when a marker is dragged")
            eq(page.unitAtY(page.unitItem(2).mapToItem(fl.contentItem, 0, 0).y + 1), 2, "the summary is found at its place")
            var it = page.unitItem(2)
            tapAt(scene(it, 120, it.height / 2))
        })
        after(100, function() {
            check(page.unitItem(3).visible && page.unitItem(4).visible, "a tap on the summary opens the section")
            var mark = visibleByName(page.unitItem(2), "summaryMark")[0]
            check(mark.text === "\u25BC", "and shows the open marker")
            page.load()
        })
        after(200, function() {
            check(page.unitItem(3).visible, "the open state survives a reload of the page")
            var it = page.unitItem(2)
            tapAt(scene(it, 120, it.height / 2))
        })
        after(100, function() {
            check(!page.unitItem(3).visible && !page.unitItem(4).visible, "a second tap closes it again")
            // a tap on the summary while a selection exists only ends the selection
            page.setSelection({ fromUnit: 6, fromOffset: 0, toUnit: 6, toOffset: 3 })   // below the summary: no marker or bubble in the way
            var it = page.unitItem(2)
            tapAt(scene(it, 120, it.height / 2))
        })
        after(100, function() {
            check(page.selection === null, "that tap ended the selection")
            check(!page.unitItem(3).visible, "and did not open the section")
            var blocks = [
                text("marker", ""),
                text("summary", "Already open", { detailsId: 3, detailsOpen: true }),
                text("paragraph", "shown", { detailsId: 3, detailsOpen: true }),
                text("marker", "")
            ]
            bridge.noteToOpen = note(blocks)
            page.openDetails = ({})
            page.load()
        })
        after(300, function() {
            check(page.unitItem(2).visible, "a section written open starts open")
            if (typeof arg_shot !== "undefined") pointer.shot(arg_shot + "_details.png")
            check(visibleByName(page.unitItem(1), "summaryMark")[0].text === "\u25BC", "with the open marker")
        })
        runner.interval = queue[0].ms
        runner.start()
    }
}
