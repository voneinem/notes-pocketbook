import QtQuick
import QtQuick.Window
import "../../qml"

// Spec 010 (US3): every dialog and pop-up has the device's look: a heavy rounded outline, a bold title with a close icon
// over a rule, a footer of full-width buttons split by a rule (dismiss first), a dimmed page that swallows touches, and
// bubbles with a pointer. The interfaces of the old components are kept; the controls still carry `text` and `clicked`.
Window {
    id: win
    width: 1080; height: 1335
    visible: true
    color: "#ffffff"
    property int failures: 0
    property var queue: []
    property int underlayClicks: 0
    property int accepted: 0
    property int rejected: 0
    property string chosenValue: ""
    property string chosenMenu: ""
    property string pickerText: ""
    property int pickerCancelled: 0
    property string emojiChosen: ""
    property var boxClose: 0
    property string boxFooter: ""

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
    function footerButtons(root) {
        return findAll(root, function(i) { return i.objectName === "footerButton" && i.visible }, [])
            .sort(function(a, b) { return a.mapToItem(null, 0, 0).x - b.mapToItem(null, 0, 0).x })
    }
    function named(root, name) { return findAll(root, function(i) { return i.objectName === name && i.visible }, [])[0] }
    function byText(root, text) { return findAll(root, function(i) { return i.text === text && i.clicked !== undefined && i.visible }, [])[0] }

    MouseArea { id: underlay; anchors.fill: parent; onClicked: win.underlayClicks++ }
    Item { id: anchorItem; x: 300; y: 100; width: 200; height: 80 }

    DialogBox {
        id: box
        visible: false
        x: 100; y: 800; width: 600
        title: "Box title"
        footer: [{ text: "Cancel", role: "dismiss" }, { text: "Go", role: "accept" }]
        onCloseRequested: win.boxClose = win.boxClose + 1
        onFooterClicked: function(role) { win.boxFooter = role }
        Text { text: "content" }
    }
    ConfirmDialog {
        id: confirm
        title: "Discard changes?"
        text: "This note has changes that were not saved."
        acceptText: "Discard"
        cancelText: "Keep editing"
        onAccepted: win.accepted++
        onRejected: win.rejected++
    }
    TextEntryDialog {
        id: entry
        title: "New category"
        placeholder: "Name"
        onAccepted: function(text) { win.pickerText = text }
        onRejected: win.rejected++
    }
    ChoiceDialog {
        id: choice
        title: "Show notes"
        current: "b"
        anchor: anchorItem
        onChosen: function(v) { win.chosenValue = v }
    }
    SelectionMenu {
        id: selMenu
        items: [{ id: "copy", label: "Copy" }, { id: "all", label: "Select all" }]
        anchorX: 500; selTop: 400; selBottom: 450; minY: 100; maxY: 1200
        onChosen: function(id) { win.chosenMenu = id }
    }
    SlashMenu {
        id: slashMenu
        entries: [{ id: "bullet_list", label: "Bullet list" }, { id: "table", label: "Table" }, { id: "link", label: "Link" }]
        anchorX: 400; caretTop: 500; caretBottom: 540; minY: 100; maxY: 1200
        onChosen: function(id) { win.chosenMenu = id }
    }
    DateTimePicker {
        id: dtp
        minY: 100; maxY: 1200
        onAccepted: function(text) { win.pickerText = text }
        onCancelled: win.pickerCancelled++
    }
    EmojiPicker {
        id: emoji
        emojis: ["\u{1F600}", "\u{1F44D}", "\u{2764}"]
        minY: 100; maxY: 1200
        onChosen: function(e) { win.emojiChosen = e }
        onCancelled: win.pickerCancelled++
    }

    Component.onCompleted: {
        // ---- DialogBox ----------------------------------------------------------------------------------------
        after(300, function() {
            box.visible = true
            var title = findAll(box, function(i) { return i.text === "Box title" }, [])[0]
            check(title !== undefined && title.font.bold === true, "the box has a bold title")
            var close = byText(box, "Close")
            check(close !== undefined, "and a close control")
            check(named(box, "dialogRule") !== undefined, "a rule under the title")
            eq(box.border.width, 4, "a heavy 4 px outline")
            eq(box.radius, 18, "with rounded corners")
            var fb = footerButtons(box)
            eq(fb.length, 2, "a footer with two cells")
            eq(fb[0].text, "Cancel", "the dismissing cell is first")
            eq(fb[1].text, "Go", "the accepting cell is second")
            eq(fb[0].font.capitalization, Font.AllUppercase, "footer labels are in capitals")
            check(named(box, "footerRule") !== undefined, "the cells are split by a rule")
            tapItem(close)
            eq(win.boxClose, 1, "the close control asks to close")
            tapItem(fb[1])
            eq(win.boxFooter, "accept", "the accepting cell reports its role")
            tapItem(fb[0])
            eq(win.boxFooter, "dismiss", "the dismissing cell reports its role")
            box.visible = false
        })

        // ---- ConfirmDialog ------------------------------------------------------------------------------------
        after(200, function() {
            eq(confirm.visible, false, "a confirmation is hidden until it is opened")
            confirm.open()
            check(confirm.visible, "open() shows it")
            var fb = footerButtons(confirm)
            eq(fb.length, 2, "two footer buttons")
            eq(fb[0].text, "Keep editing", "KEEP EDITING is on the left")
            eq(fb[1].text, "Discard", "DISCARD is on the right")
            tapAt(Qt.point(20, 20))
            eq(win.underlayClicks, 0, "the dimmed page swallows a tap outside the box")
            tapItem(fb[0])
            eq(win.rejected, 1, "the left button rejects")
            check(!confirm.visible, "and closes the dialog")
        })
        after(200, function() {
            confirm.open()
            tapItem(footerButtons(confirm)[1])
            eq(win.accepted, 1, "the right button accepts")
            check(!confirm.visible, "and closes the dialog")
            confirm.open()
            tapItem(byText(confirm, "Close"))
            eq(win.rejected, 2, "the close control rejects")
            confirm.showCancel = false
            confirm.open()
            eq(footerButtons(confirm).length, 1, "without a cancel button the footer has one cell")
            confirm.close()
            confirm.showCancel = true
        })

        // ---- TextEntryDialog ----------------------------------------------------------------------------------
        after(200, function() {
            entry.open("Work")
            check(entry.visible, "the text entry opens")
            var f = named(entry, "inkField")
            check(f !== undefined, "it has a field")
            eq(f.text, "Work", "which starts with the given text")
            var eb = named(entry, "dialogBox")
            check(eb !== undefined && eb.mapToItem(null, 0, 0).y <= 140, "the box sits in the upper part of the window")
            entry.errorText = "Too long"
            check(findAll(entry, function(i) { return String(i.text).indexOf("Too long") >= 0 && i.visible }, []).length > 0, "the error text shows under the field")
            var fb = footerButtons(entry)
            eq(fb.length, 2, "a footer with two cells")
            tapItem(fb[1])
            eq(win.pickerText, "Work", "the accepting cell reports the text and leaves the dialog open")
            check(entry.visible, "the dialog stays open until the caller closes it")
            tapItem(fb[0])
            eq(win.rejected, 3, "the dismissing cell rejects")
            check(!entry.visible, "and closes it")
        })

        // ---- ChoiceDialog as a bubble -------------------------------------------------------------------------
        after(200, function() {
            choice.choices = [{ label: "A", value: "a" }, { label: "B", value: "b" }, { label: "C", value: "c" }]
            choice.open()
        })
        after(150, function() {
            check(choice.visible, "the choice bubble opens")
            var p = named(choice, "bubblePointer")
            check(p !== undefined, "it has a pointer")
            var pc = p.mapToItem(null, p.width / 2, 0).x
            check(Math.abs(pc - 400) <= 24, "pointing at the centre of its anchor (within 24 px)")
            var rowB = findAll(choice, function(i) { return i.choice !== undefined && i.choice.value === "b" }, [])[0]
            check(rowB !== undefined && findAll(rowB, function(i) { return i.source === "check" }, []).length > 0, "the current row carries a check mark")
            var rowC = findAll(choice, function(i) { return i.choice !== undefined && i.choice.value === "c" }, [])[0]
            check(findAll(choice, function(i) { return i.objectName === "rowDivider" }, []).length >= 2, "rows are separated by dotted dividers")
            tapItem(rowC)
            eq(win.chosenValue, "c", "a tap on a row chooses it")
            check(!choice.visible, "and closes the bubble")
        })
        after(200, function() {
            var many = []
            for (var i = 1; i <= 30; ++i) many.push({ label: "Item " + i, value: "v" + i })
            choice.choices = many
            choice.open()
        })
        after(150, function() {
            var down = byText(choice, "Page down")
            check(down !== undefined && down.enabled, "a long list is paged inside the bubble")
            tapAt(Qt.point(1070, 1320))
            check(!choice.visible, "a tap outside closes the bubble")
            eq(win.underlayClicks, 0, "and does not reach the page")
        })

        // ---- SelectionMenu ------------------------------------------------------------------------------------
        after(200, function() {
            selMenu.open()
            check(selMenu.visible, "the selection menu opens")
            var copy = byText(selMenu, "Copy")
            var all = byText(selMenu, "Select all")
            check(copy !== undefined && all !== undefined, "its entries are controls with their label as text")
            check(findAll(copy, function(i) { return i.source === "copy" }, []).length > 0, "an entry draws an icon, not a boxed text button")
            check(named(selMenu, "bubblePointer") !== undefined, "the menu has a pointer")
            tapItem(copy)
            eq(win.chosenMenu, "copy", "a tap chooses the entry")
            selMenu.close()
        })

        // ---- SlashMenu ----------------------------------------------------------------------------------------
        after(200, function() {
            slashMenu.open()
            check(slashMenu.visible, "the slash menu opens")
            var labels = findAll(slashMenu, function(i) { return i.text !== undefined && i.clicked !== undefined && i.visible }, []).map(function(b) { return b.text })
            eq(labels.join("|"), "Bullet list|Table|Link", "its rows keep the order of the entries")
            eq(findAll(slashMenu, function(i) { return i.objectName === "rowDivider" && i.visible }, []).length, 1, "a dotted divider under the cells of the first of two rows")
            check(findAll(slashMenu, function(i) { return i.source === "slash-bullet_list" }, []).length > 0, "a row shows its icon")
            tapItem(byText(slashMenu, "Table"))
            eq(win.chosenMenu, "table", "a tap chooses the row")
            slashMenu.close()
        })

        // ---- DateTimePicker -----------------------------------------------------------------------------------
        after(200, function() {
            dtp.open(new Date(2026, 9, 11, 14, 30))
            check(dtp.visible, "the date picker opens")
            var title = findAll(dtp, function(i) { return i.text === "Date and time" }, [])[0]
            check(title !== undefined && title.font.bold === true, "with a bold title")
            check(byText(dtp, "Close") !== undefined, "a close control")
            var fb = footerButtons(dtp)
            eq(fb.map(function(b) { return b.text }).join("|"), "Cancel|Insert", "a footer CANCEL | INSERT")
            tapItem(fb[1])
            eq(win.pickerText, "2026-10-11 14:30", "Insert reports the moment")
            tapItem(byText(dtp, "Close"))
            eq(win.pickerCancelled, 1, "the close control cancels")
            dtp.close()
        })

        // ---- EmojiPicker --------------------------------------------------------------------------------------
        after(200, function() {
            emoji.open()
            check(emoji.visible, "the emoji grid opens")
            var cells = findAll(emoji, function(i) { return i.objectName === "emojiCell" && i.visible }, [])
            eq(cells.length, 3, "one cell per emoji")
            check(byText(emoji, "Close") !== undefined, "a close control")
            tapItem(cells[1])
            eq(win.emojiChosen, "\u{1F44D}", "a tap chooses the emoji")
            tapItem(byText(emoji, "Cancel"))
            eq(win.pickerCancelled, 2, "the footer Cancel cancels")
            emoji.close()
        })

        runner.interval = queue[0].ms
        runner.start()
    }
}
