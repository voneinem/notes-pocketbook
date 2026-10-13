import QtQuick
import QtQuick.Controls
import QtQuick.Window

// Spec 005 (US1, US2): drives the real editor page with real mouse events: a tap places the caret, a long press selects
// a word and opens the bubble menu with two draggable markers, the menu's Copy/Cut/Paste/Remove/Select all/Undo/Redo
// work, and the page offers Back, Undo, Redo, Favorite, Save, Attach, Category (and Delete for a saved note) as icon controls.
Window {
    id: win
    width: 1080; height: 1335
    visible: true
    property int failures: 0
    property var page: null
    property var queue: []

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
    // the scene point of the middle of character `pos` of a text field
    function charPoint(item, pos) {
        var r = item.positionToRectangle(pos)
        return scene(item, r.x + 3, r.y + r.height / 2)
    }
    function tapAt(p) { pointer.press(p.x, p.y); pointer.release(p.x, p.y) }
    // called from inside a step: the press and the release go in front of the steps that follow
    function longPressAt(p) {
        queue.unshift({ ms: 1000, fn: function() { pointer.release(p.x, p.y) } })
        queue.unshift({ ms: 20, fn: function() { pointer.press(p.x, p.y) } })
    }
    function menuButtons() {
        var m = findAll(page, function(i) { return i.objectName === "selectionMenu" }, [])[0]
        return m
    }
    function menuLabels() {
        var m = menuButtons()
        if (!m || !m.visible) return []
        return findAll(m, function(i) { return i.text !== undefined && i.clicked !== undefined && i.visible }, []).map(function(b) { return b.text })
    }
    function chooseInMenu(label) {
        var m = menuButtons()
        var b = findAll(m, function(i) { return i.text === label && i.clicked !== undefined }, [])[0]
        if (!b) { failures++; console.log("FAIL: no menu button " + label); return }
        tapAt(scene(b, b.width / 2, b.height / 2))
    }
    function handles() {
        return findAll(page, function(i) { return i.objectName === "selectionHandle" && i.visible }, [])
    }

    Loader { id: ld; anchors.fill: parent; source: pageUrl }

    Component.onCompleted: {
        // ---- start ----
        after(300, function() {
            page = ld.item
            check(page !== null, "the editor page loads")
            // the header offers only Cancel, Save, Favorite, Attach (Delete is for a saved note)
            var names = findAll(page, function(i) { return i.text !== undefined && i.clicked !== undefined && i.visible }, []).map(function(b) { return b.text })
            console.log("buttons: " + names.join(" | "))
            // Undo and Redo are icons of the top bar since spec 010; the editing tools of the menu are not buttons of the page
            for (var bad of ["All", "Extend", "Copy", "Cut", "Paste", "Del", "Remove", "Select all"])
                check(names.indexOf(bad) < 0, "no " + bad + " button in the editor itself")
            check(names.indexOf("Back") >= 0 && names.indexOf("Save") >= 0 && names.indexOf("Favorite") >= 0 && names.indexOf("Attach") >= 0, "Back, Save, Favorite and Attach are there")
            page.activeField = "text"
            page.activeItem.insert(0, "Hello world foo")
            check(menuLabels().length === 0, "no menu to begin with")
        })

        // ---- a tap places the caret and shows no menu ----
        after(100, function() {
            var f = page.activeItem
            tapAt(charPoint(f, 8))
            check(!page.hasSelection, "a tap selects nothing")
            check(f.cursorPosition >= 7 && f.cursorPosition <= 9, "a tap places the caret (at " + f.cursorPosition + ")")
            check(menuLabels().length === 0, "a tap shows no menu")
        })

        // ---- a long press selects the word and opens the menu with markers ----
        after(100, function() { longPressAt(charPoint(page.activeItem, 8)) })
        after(200, function() {
            var f = page.activeItem
            eq(f.selectedText, "world", "a long press selects the word")
            var labels = menuLabels()
            console.log("menu: " + labels.join(" | "))
            check(labels.indexOf("Cut") >= 0 && labels.indexOf("Copy") >= 0 && labels.indexOf("Remove") >= 0 && labels.indexOf("Select all") >= 0, "the menu offers Cut, Copy, Remove and Select all")
            check(labels.indexOf("Paste") < 0, "Paste is not offered while the clipboard is empty")
            eq(handles().length, 2, "two markers are shown")
            if (typeof arg_shot !== "undefined") pointer.shot(arg_shot + "_editor.png")
        })

        // ---- Copy ----
        after(100, function() { chooseInMenu("Copy") })
        after(100, function() {
            eq(clipboard.text(), "world", "Copy remembers the word")
            eq(page.activeItem.text, "Hello world foo", "Copy leaves the text alone")
            check(!page.hasSelection, "Copy ends the selection")
            eq(menuLabels().length, 0, "and closes the menu")
            eq(handles().length, 0, "and hides the markers")
        })

        // ---- Cut, then Undo from the menu of a long press on empty space ----
        after(100, function() { longPressAt(charPoint(page.activeItem, 2)) })
        after(200, function() { eq(page.activeItem.selectedText, "Hello", "a long press on the first word selects it"); chooseInMenu("Cut") })
        after(100, function() {
            eq(page.activeItem.text, " world foo", "Cut removes the word")
            eq(clipboard.text(), "Hello", "Cut remembers it")
        })
        // empty space below the text: the area is tall, a long press there has no word
        after(100, function() {
            var f = page.activeItem
            var r = f.positionToRectangle(f.length)
            longPressAt(scene(f, 200, r.y + r.height + 120))
        })
        after(200, function() {
            check(!page.hasSelection, "a long press on empty space selects nothing")
            var labels = menuLabels()
            console.log("menu: " + labels.join(" | "))
            check(labels.indexOf("Paste") >= 0 && labels.indexOf("Undo") >= 0 && labels.indexOf("Select all") >= 0, "it offers Paste, Select all and Undo")
            check(labels.indexOf("Cut") < 0 && labels.indexOf("Copy") < 0, "but not Cut or Copy")
            chooseInMenu("Undo")
        })
        after(100, function() {
            eq(page.activeItem.text, "Hello world foo", "Undo from the menu brings the cut word back")
            eq(page.activeItem.selectedText, "Hello", "and selects it")
        })

        // ---- Paste at the end from the empty-space menu ----
        after(100, function() { tapAt(scene(page.activeItem, 5, 5)) })   // collapse
        after(100, function() {
            var f = page.activeItem
            var r = f.positionToRectangle(f.length)
            longPressAt(scene(f, 200, r.y + r.height + 120))
        })
        after(200, function() { chooseInMenu("Paste") })
        after(100, function() { eq(page.activeItem.text, "Hello world fooHello", "Paste inserts the remembered text at the caret (the end)") })

        // ---- the markers: drag the end marker to take in more words ----
        after(100, function() { longPressAt(charPoint(page.activeItem, 8)) })
        after(200, function() {
            eq(page.activeItem.selectedText, "world", "select a word again")
            var h = handles()
            var end = h.filter(function(x) { return !x.atStart })[0]
            check(end !== undefined, "there is an end marker")
            var f = page.activeItem
            var target = f.positionToRectangle(15)   // after "foo"
            var from = scene(end, end.width / 2, end.height / 2)
            var to = scene(f, target.x + end.width / 2, target.y + target.height + end.height / 2)
            pointer.press(from.x, from.y)
            pointer.move((from.x + to.x) / 2, (from.y + to.y) / 2)
            pointer.move(to.x, to.y)
            check(menuLabels().length === 0, "the menu is hidden while a marker is dragged")
            pointer.release(to.x, to.y)
        })
        after(100, function() {
            eq(page.activeItem.selectedText, "world foo", "dragging the end marker extends the selection")
            check(menuLabels().length > 0, "the menu comes back when the marker is let go")
            // the start marker
            var h = handles()
            var st = h.filter(function(x) { return x.atStart })[0]
            var f = page.activeItem
            var target = f.positionToRectangle(0)
            var from = scene(st, st.width / 2, st.height / 2)
            var to = scene(f, target.x - st.width / 2 + 10, target.y + target.height + st.height / 2)
            pointer.press(from.x, from.y)
            pointer.move(to.x, to.y)
            pointer.release(to.x, to.y)
        })
        after(100, function() {
            eq(page.activeItem.selectedText, "Hello world foo", "dragging the start marker moves the start")
        })

        // ---- a tap outside ends the selection and the menu ----
        after(100, function() { tapAt(charPoint(page.activeItem, 17)) })
        after(100, function() {
            check(!page.hasSelection, "a tap ends the selection")
            eq(menuLabels().length, 0, "and the menu")
            eq(handles().length, 0, "and the markers")
        })

        // ---- Select all from the menu, then Remove ----
        after(100, function() { longPressAt(charPoint(page.activeItem, 8)) })
        after(200, function() { chooseInMenu("Select all") })
        after(100, function() {
            eq(page.activeItem.selectedText, page.activeItem.text, "Select all selects the whole text")
            check(menuLabels().length > 0 && menuLabels().indexOf("Select all") < 0, "the menu stays for the new selection, without Select all")
            chooseInMenu("Remove")
        })
        after(100, function() {
            eq(page.activeItem.text, "", "Remove deletes the selection")
            check(editSession.canUndo, "and can be undone")
        })

        // ---- Select all, then tap elsewhere: the selection ends wherever the tap lands ----
        after(100, function() { page.activeField = "text"; page.activeItem.insert(0, "Hello world foo"); longPressAt(charPoint(page.activeItem, 8)) })
        after(200, function() { chooseInMenu("Select all") })
        after(100, function() {
            eq(page.activeItem.selectedText, page.activeItem.text, "Select all selects everything")
            tapAt(charPoint(page.activeItem, 3))   // in the text
        })
        after(100, function() {
            check(!page.hasSelection, "a tap in the text ends a Select all")
            eq(menuLabels().length, 0, "and closes the menu")
            eq(handles().length, 0, "and hides the markers")
            longPressAt(charPoint(page.activeItem, 8))
        })
        after(200, function() { chooseInMenu("Select all") })
        after(100, function() {
            var text = page.activeItem
            page.activeField = "title"                 // (so activeItem is the title)
            var t = page.activeItem
            // a tap in the title field (on its right part: the bubble of the selection may hang over the left part)
            tapAt(scene(t, t.width - 40, t.height / 2))
            page.activeField = "text"
            check(text.selectedText === "", "a tap in the title field ends the text's selection")
            eq(menuLabels().length, 0, "and closes the menu")
        })
        after(100, function() { longPressAt(charPoint(page.activeItem, 8)) })
        after(200, function() { chooseInMenu("Select all") })
        after(100, function() {
            tapAt(Qt.point(540, 56))                   // empty space in the top bar, beside the title
        })
        after(100, function() {
            page.activeField = "text"
            check(!page.hasSelection, "a tap on empty space ends the selection")
            eq(menuLabels().length, 0, "and closes the menu")
            eq(handles().length, 0, "and hides the markers")
            page.activeItem.remove(0, page.activeItem.length)
        })

        // ---- the title field works the same way, and a paste there is one line ----
        after(100, function() {
            page.activeField = "title"
            var t = page.activeItem
            t.text = "My title"
            clipboard.copy("a\nb")
        })
        after(100, function() {
            var t = page.activeItem
            tapAt(scene(t, 20, t.height / 2))
            longPressAt(charPoint(t, 4))
        })
        after(200, function() {
            eq(page.activeItem.selectedText, "title", "a long press in the title selects a word")
            eq(handles().length, 2, "with its own markers")
            chooseInMenu("Paste")
        })
        after(100, function() { eq(page.activeItem.text, "My a b", "a pasted line break becomes a space in the title") })

        // ---- undo everything ----
        after(100, function() {
            page.category = "Work"
            page.favorite = true
            var n = 0
            while (editSession.canUndo && n < 100) { page.undoStep(); ++n }
            eq(page.category, "", "the category is back")
            eq(page.favorite, false, "the favorite marker is back")
            page.activeField = "title"
            eq(page.activeItem.text, "", "the title is back")
            page.activeField = "text"
            eq(page.activeItem.text, "", "the text is back")
            check(!page.dirty, "undoing everything clears the unsaved-changes state")
        })
        // ---- spec 008: the editor tells the bridge while it is open, and when it is gone ----
        after(100, function() {
            eq(JSON.stringify(bridge.editorOpenCalls), "[true]", "an open editor reports itself to the bridge")
            ld.active = false
        })
        after(100, function() {
            eq(JSON.stringify(bridge.editorOpenCalls), "[true,false]", "a closed editor reports that it is gone")
        })
        win.queue = win.queue   // (the runner starts below)
        runner.interval = queue[0].ms
        runner.start()
    }
}
