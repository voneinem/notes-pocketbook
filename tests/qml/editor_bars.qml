import QtQuick
import QtQuick.Controls
import QtQuick.Window

// Spec 010 (US4): the real EditorPage in the look of the device's bars: a top bar with Back, Undo, Redo, Favorite and Save as
// icon controls, the title as a labelled field, and a bottom panel above the keyboard with Attach, Category and Delete.
// Hosted in a StackView, driven with real mouse events against the stubs of stubs.qml (note 7 exists, every other id is new).
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
    function tapAt(p) { pointer.press(p.x, p.y); pointer.release(p.x, p.y) }
    function tapItem(item) { tapAt(item.mapToItem(null, item.width / 2, item.height / 2)) }
    function control(text) { return findAll(page, function(i) { return i.text === text && i.clicked !== undefined && i.visible }, [])[0] }
    function named(name) { return findAll(page, function(i) { return i.objectName === name }, [])[0] }
    function inkButtons() { return findAll(page, function(i) { return i.visible && String(i).indexOf("InkButton") === 0 }, []) }
    function iconNamed(item, name) { return findAll(item, function(i) { return i.ready !== undefined && i.source === name }, [])[0] }
    function dialogs(title) {
        return findAll(page, function(i) { return i.title === title && i.footer !== undefined && i.visible }, [])
    }
    function footerButton(dlg, text) {
        return findAll(dlg, function(i) { return i.objectName === "footerButton" && i.text === text }, [])[0]
    }
    function choiceRow(label) {
        return findAll(page, function(i) { return i.choice !== undefined && String(i.choice.label) === label && i.visible }, [])[0]
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
        // ---- a new note: the structure ---------------------------------------------------------------------------
        after(300, function() {
            page = stack.push(pageUrl, { noteId: -1 })
        })
        after(400, function() {
            check(page !== null, "the editor loads")
            var title = named("topBarTitle")
            check(title !== undefined, "there is a top bar with a title")
            eq(title.text, "New note", "the title of a new note")
            eq(title.font.capitalization, Font.AllUppercase, "shown in capitals")
            var barH = 112
            var names = ["Back", "Undo", "Redo", "Favorite", "Save"]
            for (var i = 0; i < names.length; ++i) {
                var c = control(names[i])
                check(c !== undefined, names[i] + " is a control of the page")
                if (c) check(c.mapToItem(page, 0, 0).y < barH, names[i] + " sits in the top bar")
            }
            check(control("Cancel") === undefined, "the old Cancel button is gone (Back takes its place)")
            eq(inkButtons().length, 0, "no bordered text button is on the page")
            check(!control("Undo").enabled && !control("Redo").enabled, "Undo and Redo are off while nothing was changed")

            var panel = named("bottomPanel")
            check(panel !== undefined, "there is a bottom panel")
            check(control("Attach") !== undefined && control("Category") !== undefined, "with Attach and Category")
            check(control("Delete") === undefined, "and no Delete for a new note")
            eq(panel.mapToItem(page, 0, panel.height).y, page.height - page.keyboardHeight, "the panel sits directly above the keyboard (or the bottom)")
            check(!control("Attach").enabled, "Attach is off while the server cannot take attachments")
            eq(control("Category").text, "Category", "the category tool is called Category")

            var field = named("titleField")
            var label = findAll(page, function(i) { return i.text === "Title" && !i.clicked && i.visible }, [])[0]
            check(label !== undefined && field !== undefined && label.mapToItem(page, 0, 0).y < field.mapToItem(page, 0, 0).y, "the title is a field with its label above it")
        })

        // ---- Favorite toggles its icon, Undo and Redo follow --------------------------------------------------------
        after(100, function() {
            var fav = control("Favorite")
            check(iconNamed(fav, "star") !== undefined, "an unmarked note shows the outline star")
            tapItem(fav)
            eq(page.favorite, true, "Favorite marks the note")
            check(iconNamed(control("Favorite"), "star-filled") !== undefined, "and fills the star")
            check(control("Undo").enabled, "Undo is on after a change")
            tapItem(control("Undo"))
            eq(page.favorite, false, "Undo takes the mark back")
            check(control("Redo").enabled, "Redo is on after an Undo")
            tapItem(control("Redo"))
            eq(page.favorite, true, "Redo puts it back")
            tapItem(control("Favorite"))
            eq(page.favorite, false, "a second tap on Favorite unmarks it")
        })

        // ---- category ---------------------------------------------------------------------------------------------
        after(100, function() { tapItem(control("Category")) })
        after(200, function() {
            check(choiceRow("Work") !== undefined, "Category opens the list of categories")
            tapItem(choiceRow("Work"))
        })
        after(200, function() {
            eq(page.category, "Work", "choosing Work sets the category")
            eq(control("Work") !== undefined, true, "and the tool shows it")
        })

        // ---- leaving with changes asks first -----------------------------------------------------------------------
        after(100, function() {
            tapItem(control("Back"))
        })
        after(200, function() {
            var d = dialogs("Discard changes?")[0]
            check(d !== undefined, "Back with changes asks Discard changes?")
            tapItem(footerButton(d, "Keep editing"))
        })
        after(200, function() {
            eq(stack.depth, 2, "Keep editing stays on the page")
            tapItem(control("Back"))
        })
        after(200, function() {
            var d = dialogs("Discard changes?")[0]
            tapItem(footerButton(d, "Discard"))
        })
        after(300, function() {
            eq(stack.depth, 1, "Discard leaves the page")
        })

        // ---- Back without changes leaves at once; Save saves and leaves ----------------------------------------------
        after(100, function() { page = stack.push(pageUrl, { noteId: -1 }) })
        after(400, function() { tapItem(control("Back")) })
        after(300, function() {
            eq(stack.depth, 1, "Back without changes leaves without asking")
            page = stack.push(pageUrl, { noteId: -1 })
        })
        after(400, function() {
            named("titleField").text = "Hello"
            named("textArea").text = "Body"
            tapItem(control("Save"))
        })
        after(300, function() {
            eq(bridge.savedNote.title, "Hello", "Save passes the title")
            eq(bridge.savedNote.text, "Body", "and the text")
            eq(stack.depth, 1, "and leaves the page")
        })

        // ---- Attach ----------------------------------------------------------------------------------------------------
        after(100, function() {
            attachments.supported = true
            page = stack.push(pageUrl, { noteId: -1 })
        })
        after(400, function() {
            check(control("Attach").enabled, "Attach is on when attachments are supported")
            tapItem(control("Attach"))
        })
        after(300, function() {
            eq(stack.depth, 3, "Attach opens the file browser")
            stack.pop()
            attachments.supported = false
        })
        after(300, function() { stack.pop(null) })

        // ---- a saved note: Delete -----------------------------------------------------------------------------------
        after(200, function() { page = stack.push(pageUrl, { noteId: 7 }) })
        after(400, function() {
            eq(named("topBarTitle").text, "Edit note", "the title of a saved note")
            eq(named("titleField").text, "Old title", "its title is in the field")
            check(control("Delete") !== undefined, "a saved note has Delete in the panel")
            tapItem(control("Delete"))
        })
        after(200, function() {
            var d = dialogs("Delete this note?")[0]
            check(d !== undefined, "Delete asks first")
            tapItem(footerButton(d, "Delete"))
        })
        after(300, function() {
            eq(bridge.deletedId, 7, "the note is deleted")
            eq(stack.depth, 1, "and the page closes")
        })

        runner.interval = queue[0].ms
        runner.start()
    }
}
