import QtQuick
import QtQuick.Controls
import QtQuick.Window

// Spec 006 (US3, US5): the "/" menu, the "//" date-time picker and the emoji grid of the real editor page, driven with
// real key and mouse events.
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
    function popup(name) { return findAll(page, function(i) { return i.objectName === name }, [])[0] }
    function slashMenu() { return popup("slashMenu") }
    function menuOpen() { var m = slashMenu(); return m !== undefined && m.visible }
    function slashLabels() {
        var m = slashMenu()
        if (!m || !m.visible) return []
        return findAll(m, function(i) { return i.text !== undefined && i.clicked !== undefined && i.visible }, []).map(function(b) { return b.text })
    }
    function chooseSlash(label) {
        var b = findAll(slashMenu(), function(i) { return i.text === label && i.clicked !== undefined }, [])[0]
        if (!b) { failures++; console.log("FAIL: no slash menu button " + label); return }
        tapAt(scene(b, b.width / 2, b.height / 2))
    }
    property string window_emoji: ""
    // the scene point of a button inside whichever picker is open, by objectName or by its text
    function button(name) {
        var b = findAll(page, function(i) { return i.objectName === name && i.visible }, [])[0]
        if (!b) { failures++; console.log("FAIL: no button " + name); return Qt.point(1, 1) }
        return scene(b, b.width / 2, b.height / 2)
    }
    function buttonText(label) {
        var b = findAll(page, function(i) { return i.text === label && i.clicked !== undefined && i.visible }, [])[0]
        if (!b) { failures++; console.log("FAIL: no button " + label); return Qt.point(1, 1) }
        return scene(b, b.width / 2, b.height / 2)
    }
    function area() { return popup("textArea") }
    function focusText() { page.activeField = "text"; area().forceActiveFocus() }
    function setText(t) { page.programmatic = true; area().text = t; area().cursorPosition = t.length; page.programmatic = false }

    Loader { id: ld; anchors.fill: parent; source: pageUrl }

    Component.onCompleted: {
        after(300, function() {
            page = ld.item
            check(page !== null, "the editor page loads")
            focusText()
            check(!menuOpen(), "no menu to begin with")
        })

        // ---- "/" on an empty line opens the list with the twelve entries in order ----
        after(100, function() { pointer.type("/") })
        after(100, function() {
            check(menuOpen(), "typing / at the start opens the menu")
            eq(slashLabels().join("|"),
               "Bullet list|Table|Info box|Link|Bold|Italic|Underline|Strikethrough|Details|Quote|Code block|Emoji",
               "the twelve entries in the specified order")
            check(area().activeFocus, "the text area keeps the focus (the keyboard stays)")
            if (typeof arg_shot !== "undefined") pointer.shot(arg_shot + "_slash.png")
        })

        // ---- choosing Code block inserts the fence with the caret on the empty line; one Undo reverses it ----
        after(100, function() { chooseSlash("Code block") })
        after(100, function() {
            eq(area().text, "```\n\n```", "Code block replaces the slash with a fence, an empty line and a fence")
            eq(area().cursorPosition, 4, "the caret is on the empty line")
            check(!menuOpen(), "the menu closes")
            check(area().activeFocus, "the text area still has the focus")
            check(editSession.canUndo, "the insertion can be undone")
            page.undoStep()
        })
        after(100, function() {
            eq(area().text, "", "one Undo restores the text from before the slash was typed")
            check(!editSession.canUndo, "and that was the only step")
            page.redoStep()
        })
        after(100, function() {
            eq(area().text, "```\n\n```", "Redo inserts it again")
        })

        // ---- a slash after a letter, in an address or a fraction opens nothing ----
        after(100, function() { setText(""); editSession.reset("", "", "", false); focusText(); pointer.type("abc/") })
        after(100, function() {
            eq(area().text, "abc/", "the slash after a letter is an ordinary character")
            check(!menuOpen(), "and opens no menu")
            setText(""); pointer.type("see https://x and 1/2")
        })
        after(100, function() {
            eq(area().text, "see https://x and 1/2", "addresses and fractions type normally")
            check(!menuOpen(), "and open no menu")
        })

        // ---- after a space it opens; any other character closes it and leaves the text ----
        after(100, function() { setText("abc "); pointer.type("/") })
        after(100, function() { check(menuOpen(), "a slash after a space opens the menu"); pointer.type("x") })
        after(100, function() {
            check(!menuOpen(), "another character closes the menu")
            eq(area().text, "abc /x", "and the slash and the character stay")
        })

        // ---- a tap outside closes it and leaves the slash ----
        after(100, function() { setText(""); pointer.type("/") })
        after(100, function() {
            check(menuOpen(), "the menu is open")
            var scrim = popup("slashScrim")
            check(scrim !== undefined && scrim.visible, "a scrim catches taps outside the menu")
            tapAt(Qt.point(40, 1300))
        })
        after(100, function() {
            check(!menuOpen(), "a tap outside closes the menu")
            eq(area().text, "/", "and leaves the slash as typed")
        })

        // ---- Back closes only the menu ----
        after(100, function() { setText(""); pointer.type("/") })
        after(100, function() {
            check(menuOpen(), "open again")
            eq(page.backHandled(), true, "Back is taken by an open menu")
            check(!menuOpen(), "and closes it")
            eq(area().text, "/", "the slash stays")
            eq(page.backHandled(), false, "with nothing open Back is not taken (the page may leave)")
        })

        // ---- a block entry after text starts a new line; the typed slash is part of one undo step ----
        after(100, function() { setText("abc "); editSession.reset("", "abc ", "", false); focusText(); pointer.type("/") })
        after(100, function() { chooseSlash("Bullet list") })
        after(100, function() {
            eq(area().text, "abc \n- ", "a block entry after text on the line starts a new line")
            eq(area().cursorPosition, 7, "the caret is after the list marker")
            page.undoStep()
        })
        after(100, function() { eq(area().text, "abc ", "one Undo removes the line break and the marker too") })

        // ---- inline entry: bold puts the caret between the marks ----
        after(100, function() { setText("x "); pointer.type("/") })
        after(100, function() { chooseSlash("Bold") })
        after(100, function() {
            eq(area().text, "x ****", "Bold inserts two pairs of asterisks")
            eq(area().cursorPosition, 4, "with the caret between them")
        })

        // ---- the Title field never opens the menu ----
        after(100, function() { page.activeField = "title"; page.activeItem.forceActiveFocus(); pointer.type("/ /") })
        after(100, function() {
            check(!menuOpen(), "typing slashes in the Title field opens no menu")
            eq(page.activeItem.text, "/ /", "they are ordinary title characters")
            focusText()
        })

        // ---- text that the editor sets itself never opens it ----
        after(100, function() {
            setText("a ")
            clipboard.copy("/")
            page.pasteIntoActive()
            eq(area().text, "a /", "Paste inserts the slash")
            check(!menuOpen(), "a pasted slash opens no menu")
        })

        // ---- another command closes the menu and ends its history group ----
        after(100, function() { setText(""); editSession.reset("", "", "", false); focusText(); pointer.type("/") })
        after(100, function() {
            check(menuOpen(), "open")
            page.fieldHeld("text", area(), 40, 20)
        })
        after(100, function() {
            check(!menuOpen(), "the long-press menu closes the slash menu")
            check(editSession.canUndo, "and the typed slash became an ordinary step")
        })

        // ---- rotation (a width change) closes it ----
        after(100, function() { page.dismissSelection(); setText(""); focusText(); pointer.type("/") })
        after(100, function() { check(menuOpen(), "open before the rotation"); win.width = 900 })
        after(100, function() {
            check(!menuOpen(), "a change of the page width closes the menu")
            eq(area().text, "/", "and leaves the text")
            win.width = 1080
        })

        // ---- US5: "//" opens the date-time picker ----
        after(100, function() {
            page.dismissSelection(); win.width = 1080
            setText(""); editSession.reset("", "", "", false); focusText()
            page.now = new Date(2026, 9, 11, 14, 30)
            pointer.type("/")
        })
        after(100, function() { check(menuOpen(), "the menu is open"); pointer.type("/") })
        after(100, function() {
            var dp = popup("dateTimePicker")
            check(dp !== undefined && dp.visible, "a second slash opens the date-time picker")
            check(!menuOpen(), "and closes the menu")
            eq(dp.year + "-" + dp.month + "-" + dp.day + " " + dp.hour + ":" + dp.minute, "2026-10-11 14:30", "it starts at the current date and time")
            eq(area().text, "//", "the typed slashes are still in the text")
            if (typeof arg_shot !== "undefined") pointer.shot(arg_shot + "_datetime.png")
        })
        // steppers: the day wraps within the month, the month clamps the day
        after(100, function() {
            var dp = popup("dateTimePicker")
            dp.day = 31
            tapAt(button("upDay"))
        })
        after(100, function() {
            eq(popup("dateTimePicker").day, 1, "the day after the 31st of October is the 1st: only days of the month are offered")
            var dp = popup("dateTimePicker")
            dp.month = 3; dp.day = 31
            tapAt(button("downMonth"))
        })
        after(100, function() {
            var dp = popup("dateTimePicker")
            eq(dp.month + "/" + dp.day, "2/28", "going from 31 March to February clamps the day to the 28th (no 31 February)")
            dp.year = 2024; dp.month = 3; dp.day = 31
            tapAt(button("downMonth"))
        })
        after(100, function() {
            var dp = popup("dateTimePicker")
            eq(dp.month + "/" + dp.day, "2/29", "and to the 29th in a leap year")
            dp.hour = 23
            tapAt(button("upHour"))
        })
        after(100, function() {
            eq(popup("dateTimePicker").hour, 0, "the hour wraps from 23 to 0")
            var dp = popup("dateTimePicker")
            dp.year = 2026; dp.month = 10; dp.day = 11; dp.hour = 14; dp.minute = 30
            tapAt(buttonText("Insert"))
        })
        after(100, function() {
            eq(area().text, "2026-10-11 14:30", "Insert replaces the // by the chosen moment")
            check(!popup("dateTimePicker").visible, "the picker closes")
            eq(area().cursorPosition, 16, "the caret is after the inserted text")
            check(area().activeFocus, "the text area keeps the focus")
            page.undoStep()
        })
        after(100, function() {
            eq(area().text, "", "one Undo restores the text from before the first slash")
            check(!editSession.canUndo, "and that was the only step")
            page.redoStep()
        })
        after(100, function() { eq(area().text, "2026-10-11 14:30", "Redo inserts it again") })

        // ---- Cancel and Back leave the // as typed ----
        after(100, function() { setText(""); editSession.reset("", "", "", false); focusText(); pointer.type("//") })
        after(100, function() {
            check(popup("dateTimePicker").visible, "the picker is open again")
            tapAt(buttonText("Cancel"))
        })
        after(100, function() {
            check(!popup("dateTimePicker").visible, "Cancel closes the picker")
            eq(area().text, "//", "and leaves the typed slashes")
            check(editSession.canUndo, "typing them is an ordinary step")
            setText(""); pointer.type("//")
        })
        after(100, function() {
            check(popup("dateTimePicker").visible, "open once more")
            eq(page.backHandled(), true, "Back is taken by the picker")
            check(!popup("dateTimePicker").visible, "and closes it")
            eq(area().text, "//", "the slashes stay")
        })

        // ---- slashes after a letter, in an address, in the title: no picker ----
        after(100, function() { setText(""); pointer.type("a//") })
        after(100, function() {
            check(!popup("dateTimePicker").visible && !menuOpen(), "a// opens nothing")
            setText(""); pointer.type("https://")
        })
        after(100, function() {
            check(!popup("dateTimePicker").visible && !menuOpen(), "https:// opens nothing")
            page.activeField = "title"; page.activeItem.forceActiveFocus(); pointer.type("//")
        })
        after(100, function() {
            check(!popup("dateTimePicker").visible && !menuOpen(), "// in the title opens nothing")
            focusText()
        })

        // ---- the emoji grid ----
        after(100, function() { setText(""); editSession.reset("", "", "", false); focusText(); pointer.type("/") })
        after(100, function() { chooseSlash("Emoji") })
        after(100, function() {
            var ep = popup("emojiPicker")
            check(ep !== undefined && ep.visible, "the Emoji entry opens the emoji grid")
            check(!menuOpen(), "and closes the menu")
            var cells = findAll(ep, function(i) { return i.objectName === "emojiCell" && i.visible }, [])
            eq(cells.length, 48, "the grid has 48 cells")
            var small = cells.filter(function(c) { return c.width < 78 || c.height < 78 })
            eq(small.length, 0, "every cell is at least 78 by 78 pixels")
            if (typeof arg_shot !== "undefined") pointer.shot(arg_shot + "_emoji.png")
            var first = cells[0]
            window_emoji = first.text
            tapAt(scene(first, first.width / 2, first.height / 2))
        })
        after(100, function() {
            eq(area().text, window_emoji, "a tap puts the emoji in place of the slash")
            check(!popup("emojiPicker").visible, "and closes the grid")
            eq(area().cursorPosition, window_emoji.length, "the caret is behind it")
            page.undoStep()
        })
        after(100, function() { eq(area().text, "", "one Undo restores the text from before the slash") })
        after(100, function() { setText(""); editSession.reset("", "", "", false); focusText(); pointer.type("/") })
        after(100, function() { chooseSlash("Emoji") })
        after(100, function() {
            check(popup("emojiPicker").visible, "the grid is open")
            tapAt(buttonText("Cancel"))
        })
        after(100, function() {
            check(!popup("emojiPicker").visible, "Cancel closes the grid")
            eq(area().text, "/", "and leaves the slash")
            setText(""); pointer.type("/")
        })
        after(100, function() { chooseSlash("Emoji") })
        after(100, function() {
            eq(page.backHandled(), true, "Back is taken by the grid")
            check(!popup("emojiPicker").visible, "and closes it")
            eq(area().text, "/", "the slash stays")
        })

        // ---- rotation closes a picker ----
        after(100, function() { setText(""); focusText(); pointer.type("//") })
        after(100, function() { check(popup("dateTimePicker").visible, "open before the rotation"); win.width = 900 })
        after(100, function() {
            check(!popup("dateTimePicker").visible, "a change of the page width closes the picker")
            win.width = 1080
        })
        runner.interval = queue[0].ms
        runner.start()
    }
}
