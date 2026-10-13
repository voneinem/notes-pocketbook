import QtQuick
import QtQuick.Controls
import QtQuick.Window

// Spec 010 (US2): the real NoteListPage, hosted in a StackView and driven with real mouse events against the stub `bridge`
// of stubs.qml (60 notes, every 7th a favorite, every 5th in "Work", every 11th in "Home", every 13th pending). Checks the
// structure of the reference (top bar, filter/sort row, count line, rows with hairlines), every function of the old list
// (views, sort, search, new, sync, settings, paging) and the Tile layout with its remembered setting.
Window {
    id: win
    width: 1404; height: 1872
    visible: true
    property int failures: 0
    property var page: null
    property var queue: []
    // the number of whole note rows on the first page of the 1.5.0 list (checklists/baseline.md)
    readonly property int baselineRows: 11

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
    function tapItem(item) { tapAt(scene(item, item.width / 2, item.height / 2)) }

    // ---- finders ------------------------------------------------------------------------------------------------
    function controls(text) {
        return findAll(page, function(i) { return i.text === text && i.clicked !== undefined && i.visible }, [])
    }
    function control(text) { return controls(text)[0] }
    function byName(name) { return findAll(page, function(i) { return i.objectName === name }, [])[0] }
    function texts() {
        return findAll(page, function(i) { return i.text !== undefined && i.clicked === undefined && i.visible }, [])
    }
    function hasText(part) { return texts().some(function(t) { return String(t.text).indexOf(part) >= 0 }) }
    function textItem(part) { return texts().filter(function(t) { return String(t.text).indexOf(part) >= 0 })[0] }
    function listView() { return byName("noteList") }
    function gridView() { return byName("noteGrid") }
    function noteItems(view, id) {
        return findAll(view, function(i) { return i.note !== undefined && i.note.localId === id && i.visible }, [])
    }
    function iconNamed(item, name) { return findAll(item, function(i) { return i.ready !== undefined && i.source === name }, [])[0] }
    // a visible choice dialog (view picker / menu) and its row for a label
    function dialog() { return findAll(page, function(i) { return i.choices !== undefined && i.visible }, [])[0] }
    function dialogRow(label) {
        var d = dialog()
        return d ? findAll(d, function(i) { return i.choice !== undefined && String(i.choice.label).indexOf(label) >= 0 }, [])[0] : undefined
    }
    function inkButtons() {
        return findAll(page, function(i) { return i.visible && String(i).indexOf("InkButton") === 0 }, [])
    }
    function reset() {
        bridge.noteCount = 60
        bridge.syncOutcome = "ok"
        bridge.view = "all"
        bridge.sortOrder = "modified"
        bridge.searchText = ""
        bridge.notesChanged()
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
    function openPage() { page = stack.push(pageUrl) }

    Component.onCompleted: {
        bridge.noteToOpen = { title: "", blocks: [] }

        // ---- the structure of the top ------------------------------------------------------------------------
        after(450, function() { openPage() })
        after(450, function() {
            check(page !== null, "the page loads")
            var title = byName("topBarTitle")
            check(title !== undefined, "there is a top bar with a title")
            eq(title.text, "Notes", "the title is Notes")
            eq(title.font.capitalization, Font.AllUppercase, "shown in capitals")
            var c = title.mapToItem(page, title.width / 2, 0).x
            check(Math.abs(c - page.width / 2) <= 2, "the title is centred")
            check(control("Home") !== undefined && control("Menu") !== undefined && control("Search") !== undefined && control("New note") !== undefined,
                  "Home, Menu, Search and New note are icon controls of the bar")
            eq(inkButtons().length, 0, "no bordered text button is on the page")
            check(!hasText("Quit") && !hasText("View:") && !hasText("Sort:"), "the old text buttons Quit, View: and Sort: are gone")

            // ---- the filter/sort row and the count line ------------------------------------------------------
            check(byName("viewSelector") !== undefined && byName("sortSelector") !== undefined, "the filter/sort row has a view and a sort control")
            check(hasText("All"), "it shows the view label All")
            check(hasText("Recently changed"), "it shows the sort label Recently changed")
            check(control("Layout") !== undefined && byName("layoutToggle") !== undefined, "the layout toggle is there")
            var count = textItem("60 notes")
            check(count !== undefined, "the count line reads 60 notes")
            if (count) eq(count.font.capitalization, Font.AllUppercase, "and is shown in capitals")
            check(hasText("Synced."), "the sync state is shown")
            check(hasText("3 change(s) not synced yet"), "and the count of unsynced changes")

            // ---- rows ----------------------------------------------------------------------------------------
            var list = listView()
            check(list !== undefined && list.visible, "the list view is shown")
            var fav = noteItems(list, 7)[0]
            check(fav !== undefined, "the favorite note 7 has a row")
            if (fav) {
                check(iconNamed(fav, "star-filled") !== undefined, "a favorite shows the star icon")
                var titleText = findAll(fav, function(i) { return i.text === "Note 07" }, [])[0]
                check(titleText !== undefined && titleText.font.bold === true, "the title is bold")
                check(findAll(fav, function(i) { return String(i.text).indexOf("Today") >= 0 }, []).length > 0, "the second line carries the date")
            }
            var work = noteItems(list, 5)[0]
            check(work !== undefined && findAll(work, function(i) { return String(i.text).indexOf("Work") >= 0 }, []).length > 0, "a note in Work shows its category")
        })

        // ---- opening notes, creating, home -------------------------------------------------------------------
        after(200, function() {
            var row = noteItems(listView(), 3)[0]
            tapItem(row)
        })
        after(300, function() {
            eq(stack.depth, 3, "a tap on a row opens a page")
            eq(stack.currentItem.noteId, 3, "it is the reader of that note")
            stack.pop()
        })
        after(300, function() { tapItem(control("New note")) })
        after(300, function() {
            eq(stack.depth, 3, "New note opens a page")
            eq(stack.currentItem.noteId, -1, "it is the editor of a new note")
            stack.pop()
        })
        after(300, function() {
            tapItem(control("Home"))
            eq(device.quitCalls, 1, "Home leaves the app")
        })

        // ---- the menu: Sync now and Settings -----------------------------------------------------------------
        after(200, function() { tapItem(control("Menu")) })
        after(200, function() {
            var d = dialog()
            check(d !== undefined, "Menu opens a choice list")
            check(dialogRow("Sync now") !== undefined && dialogRow("Settings") !== undefined, "with Sync now and Settings")
            win.syncBefore = bridge.syncNowCalls
            tapItem(dialogRow("Sync now"))
        })
        after(200, function() {
            eq(bridge.syncNowCalls, win.syncBefore + 1, "Sync now starts a sync")
            tapItem(control("Menu"))
        })
        after(200, function() { tapItem(dialogRow("Settings")) })
        after(300, function() {
            eq(stack.depth, 3, "Settings opens a page")
            check(stack.currentItem.message !== undefined, "it is the settings page")
            stack.pop()
        })

        // ---- view, sort ----------------------------------------------------------------------------------------
        after(300, function() { tapItem(byName("viewSelector")) })
        after(200, function() {
            check(dialog() !== undefined, "the view label opens the list of views")
            check(dialogRow("All") !== undefined && dialogRow("Favorites") !== undefined && dialogRow("Uncategorized") !== undefined, "All, Favorites, Uncategorized")
            check(dialogRow("Work") !== undefined && dialogRow("(12)") !== undefined && dialogRow("Home") !== undefined, "and each category with its count")
            tapItem(dialogRow("Favorites"))
        })
        after(300, function() {
            eq(bridge.viewCalls[bridge.viewCalls.length - 1], "favorites", "choosing Favorites sets the view")
            check(hasText("8 notes"), "the count line follows the view (8 favorites)")
            check(hasText("Favorites"), "the label shows the view")
            bridge.setView("all"); bridge.notesChanged()
        })
        after(300, function() { tapItem(byName("sortSelector")) })
        after(200, function() {
            check(dialog() !== undefined, "the sort label opens a pop-up with the orders")
            check(dialogRow("Recently changed") !== undefined && dialogRow("A\u2013Z") !== undefined, "Recently changed and A-Z")
            tapItem(dialogRow("A\u2013Z"))
        })
        after(300, function() {
            eq(bridge.sortOrder, "title", "choosing A-Z switches the order")
            check(hasText("A–Z"), "and its label says so")
            bridge.setSortOrder("modified"); bridge.notesChanged()
        })

        // ---- search --------------------------------------------------------------------------------------------
        after(300, function() { check(byName("searchField") === undefined || !byName("searchField").visible, "the search field is closed at first"); tapItem(control("Search")) })
        after(300, function() {
            var f = byName("searchField")
            check(f !== undefined && f.visible, "Search opens the field")
            check(control("Close search") !== undefined, "with a control to close it")
            f.forceActiveFocus()
            pointer.type("Note 5")
        })
        after(900, function() {
            eq(bridge.searchCalls[bridge.searchCalls.length - 1], "Note 5", "typing sets the search text after the debounce")
            check(hasText("10 notes"), "and the count follows (10 notes match)")
            tapItem(control("Close search"))
        })
        after(300, function() {
            eq(bridge.searchCalls[bridge.searchCalls.length - 1], "", "closing the search clears it")
            var f = byName("searchField")
            check(f === undefined || !f.visible, "and hides the field")
            bridge.notesChanged()
        })

        // ---- paging in the list --------------------------------------------------------------------------------
        after(300, function() {
            var list = listView()
            var perPage = Math.floor(list.height / 132)
            check(perPage >= win.baselineRows, "the list shows at least the 1.5.0 number of whole rows per page (" + perPage + " >= " + win.baselineRows + ")")
            var up = control("Page up"), down = control("Page down")
            check(up !== undefined && down !== undefined, "Page up and Page down are there")
            check(!up.enabled && down.enabled, "Page up is off at the top, Page down is on")
            tapItem(down)
        })
        after(200, function() {
            var list = listView()
            eq(list.contentY % 132, 0, "a page starts on a whole row (no row is cut)")
            check(list.contentY > 0, "Page down moved the list")
            var p = noteItems(list, 13)[0]
            check(p !== undefined && findAll(p, function(i) { return String(i.text).indexOf("not synced yet") >= 0 }, []).length > 0, "a pending note says not synced yet")
            tapItem(control("Page up"))
        })
        after(200, function() { eq(listView().contentY, 0, "Page up returns to the first page") })

        // ---- the Tile layout ----------------------------------------------------------------------------------
        after(200, function() {
            check(gridView() === undefined || !gridView().visible, "the grid is not shown while the layout is list")
            eq(iconNamed(byName("layoutToggle"), "grid") !== undefined, true, "the toggle shows the grid icon while the list is shown")
            tapItem(byName("layoutToggle"))
        })
        after(300, function() {
            eq(bridge.listLayout, "tile", "the toggle switches the layout to tile")
            eq(bridge.listLayoutWrites, 1, "and stores it once")
            var g = gridView()
            check(g !== undefined && g.visible, "the grid of cards is shown")
            check(!listView().visible, "and the list is not")
            eq(Math.round(g.width / g.cellWidth), Math.max(2, Math.floor(g.width / 420)), "columns = max(2, floor(width / 420))")
            eq(g.cellHeight, 190, "cards are 190 px high")
            eq(g.count, listView().count, "the same number of notes")
            eq(g.model[0].localId, listView().model[0].localId, "in the same order")
            eq(iconNamed(byName("layoutToggle"), "list") !== undefined, true, "the toggle now shows the list icon")
            var card = noteItems(g, 7)[0]
            check(card !== undefined, "the favorite note 7 has a card")
            if (card) {
                check(iconNamed(card, "star-filled") !== undefined, "a favorite card shows the star")
                check(findAll(card, function(i) { return i.text === "Note 07" }, []).length > 0, "a card shows the title")
            }
            var longCard = noteItems(g, 12)[0]
            check(longCard !== undefined, "the note with the 120-character title has a card")
            if (longCard) {
                var lt = findAll(longCard, function(i) { return String(i.text).indexOf("A very long title") === 0 }, [])[0]
                check(lt !== undefined && lt.width <= longCard.width, "its title stays inside the card")
            }
            // a tap on a card opens the same reader
            tapItem(noteItems(g, 3)[0])
        })
        after(300, function() {
            eq(stack.depth, 3, "a tap on a card opens a page")
            eq(stack.currentItem.noteId, 3, "the reader of that note")
            stack.pop()
        })
        after(300, function() {
            // paging by whole rows of cards; at least as many notes per page as in the list
            var g = gridView()
            var cols = Math.round(g.width / g.cellWidth)
            check(Math.floor(g.height / 190) * cols >= Math.floor(listView().height / 132), "a tile page holds at least as many notes as a list page")
            tapItem(control("Page down"))
        })
        after(200, function() {
            eq(gridView().contentY % 190, 0, "a tile page starts on a whole row of cards")
            check(gridView().contentY > 0, "Page down moved the grid")
            tapItem(control("Page up"))
        })

        // ---- the layout is remembered -------------------------------------------------------------------------
        after(200, function() { stack.pop(null); page = null })
        after(200, function() { openPage() })
        after(400, function() {
            var g = gridView()
            check(g !== undefined && g.visible, "a new page starts in the stored layout (tile)")
            tapItem(byName("layoutToggle"))
        })
        after(300, function() {
            eq(bridge.listLayout, "list", "the toggle returns to the list")
            check(listView().visible, "the list is shown again")
        })

        // ---- empty states ---------------------------------------------------------------------------------------
        after(200, function() {
            bridge.noteCount = 0
            bridge.syncOutcome = "never"
            bridge.notesChanged()
        })
        after(300, function() {
            check(hasText("No notes yet"), "no notes after nothing was ever synced says so")
            check(hasText("Sync now"), "and says where Sync now is")
            check(hasText("0 notes"), "the count line reads 0 notes")
            bridge.syncOutcome = "ok"
            bridge.view = "favorites"
            bridge.notesChanged()
        })
        after(300, function() {
            check(hasText("No notes match"), "an empty filtered view says nothing matches")
            reset()
        })
        after(300, function() { check(hasText("60 notes"), "and the list comes back") })

        runner.interval = queue[0].ms
        runner.start()
    }
    property int syncBefore: 0
}
