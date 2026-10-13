import QtQuick
import QtQuick.Controls
import QtQuick.Window

// The file browser behind "Attach" (specs 002 and 009): the real FilePickerPage, hosted in a StackView, driven with
// real mouse events against the stub `attachments` (stubs.qml), which plays the bridge: the root lists three folders,
// /root/Photo holds a subfolder, huge.bin, img01..img40.jpg and notes.pdf.
Window {
    id: win
    width: 1404; height: 1872
    visible: true
    property int failures: 0
    property var page: null
    property var picked: []
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
    function tapAt(p) { pointer.press(p.x, p.y); pointer.release(p.x, p.y) }
    function tapItem(item) { tapAt(scene(item, item.width / 2, item.height / 2)) }

    // ---- finders -----------------------------------------------------------------------------------------------
    function buttons(text) {
        return findAll(page, function(i) { return i.text === text && i.clicked !== undefined && i.visible }, [])
    }
    function button(text) { return buttons(text)[0] }
    function texts() {
        return findAll(page, function(i) { return i.text !== undefined && i.clicked === undefined && i.visible }, []).map(function(t) { return String(t.text) })
    }
    function hasText(part) { return texts().some(function(t) { return t.indexOf(part) >= 0 }) }
    // the list of rows (ListView: positionViewAtIndex, no cellWidth) and the grid of tiles (GridView: cellWidth)
    function listView() {
        return findAll(page, function(i) { return typeof i.positionViewAtIndex === "function" && i.cellWidth === undefined }, [])[0]
    }
    function gridView() {
        return findAll(page, function(i) { return typeof i.positionViewAtIndex === "function" && i.cellWidth !== undefined }, [])[0]
    }
    function entryItems(name) {
        return findAll(page, function(i) { return i.entry !== undefined && i.entry.name === name }, [])
    }
    function entryItem(name) { return entryItems(name)[0] }
    function dialog() {
        return findAll(page, function(i) { return i.acceptText !== undefined && i.title !== undefined }, [])[0]
    }
    function pathLine() {
        var t = texts().filter(function(s) { return s.charAt(0) === "/" })
        return t.length ? t[0] : ""
    }
    // the view that is shown: the grid of tiles when it is visible, else the list
    function curView() { var g = gridView(); return g && g.visible ? g : listView() }
    // the Image items (previews) inside `item`
    function images(item) {
        // (the pictures of icons, spec 010, are not previews: an Icon's Image has the Icon as parent)
        return item ? findAll(item, function(i) { return i.sourceSize !== undefined && i.fillMode !== undefined && i.asynchronous !== undefined && !(i.parent && i.parent.ready !== undefined) }, []) : []
    }
    function fileGlyphOf(tile) { return findAll(tile, function(i) { return i.objectName === "fileGlyph" }, [])[0] }
    function folderGlyphOf(tile) { return findAll(tile, function(i) { return i.objectName === "folderGlyph" }, [])[0] }
    function toggle() { return findAll(page, function(i) { return i.objectName === "layoutToggle" }, [])[0] }
    // how many entries lie wholly inside the visible part of `v`
    function shownCount(v) {
        return findAll(v, function(i) { return i.entry !== undefined && i.y >= v.contentY && i.y + i.height <= v.contentY + v.height }, []).length
    }
    // Page down until the entry's row is wholly inside the visible part of the list or grid
    function bringIntoView(name) {
        var v = curView()
        for (var n = 0; n < 20; ++n) {
            var it = entryItem(name)
            if (it && it.y >= v.contentY && it.y + it.height <= v.contentY + v.height) return it
            var down = button("Page down")
            if (!down || !down.enabled) return it
            tapItem(down)
        }
        return entryItem(name)
    }
    function tapEntry(name) {
        var it = bringIntoView(name)
        if (!it) { failures++; console.log("FAIL: no entry " + name); return }
        tapItem(it)
    }

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: Item {}
        // as in Main.qml: no transitions on e-ink
        pushEnter: Transition {}
        pushExit: Transition {}
        popEnter: Transition {}
        popExit: Transition {}
    }

    function openPage() {
        picked = []
        page = stack.push(pageUrl)
        page.picked.connect(function(p) { var l = win.picked.slice(); l.push(p); win.picked = l })
    }

    Component.onCompleted: {
        // ---- today's behavior of the list view (must hold before and after the feature) ----------------------------
        after(450, function() {
            openPage()
        })
        after(450, function() {
            check(page !== null, "the page loads")
            // the top bar of spec 010: Cancel (the back chevron), the title Attach, Up and the layout toggle as icons
            var tbTitle = findAll(page, function(i) { return i.objectName === "topBarTitle" }, [])[0]
            check(tbTitle !== undefined && tbTitle.text === "Attach", "a top bar titled Attach")
            check(button("Cancel") !== undefined && button("Up") !== undefined, "Cancel and Up are controls of the bar")
            check(button("Cancel").mapToItem(page, 0, 0).y < 112 && button("Up").mapToItem(page, 0, 0).y < 112 && toggle().mapToItem(page, 0, 0).y < 112, "all in the top bar")
            eq(findAll(page, function(i) { return i.visible && String(i).indexOf("InkButton") === 0 }, []).length, 0, "no bordered text button is on the page")
            eq(pathLine(), "/root", "it starts at the browser's root")
            check(listView() !== undefined, "the list view is shown")
            check(entryItem("Music") !== undefined && entryItem("Photo") !== undefined && entryItem("screens") !== undefined, "the root lists its folders")
            check(texts().indexOf("Folder") >= 0, "a folder row carries the kind label")
            tapEntry("Photo")
        })
        after(450, function() {
            eq(pathLine(), "/root/Photo", "a tap on a folder opens it")
            check(entryItem("Trips") !== undefined, "its subfolder is listed")
            var trips = entryItem("Trips"), huge = entryItem("huge.bin")
            check(trips.y < huge.y, "folders come before files")
            check(entryItem("img01.jpg") !== undefined, "files are listed")
            // paging by whole rows
            var v = listView()
            eq(v.contentY, 0, "the first page starts at the top")
            tapItem(button("Page down"))
        })
        after(450, function() {
            var v = listView()
            check(v.contentY > 0, "Page down moves the list")
            eq(v.contentY % 104, 0, "by whole rows, none is cut off at the top")
            tapItem(button("Page up"))
        })
        after(450, function() {
            eq(listView().contentY, 0, "Page up returns to the top")
            tapEntry("huge.bin")
        })
        after(450, function() {
            check(hasText("larger than the"), "a file over the limit gives the size message")
            check(!dialog().visible, "and asks nothing")
            eq(picked.length, 0, "and picks nothing")
            tapEntry("notes.pdf")
        })
        after(450, function() {
            check(dialog().visible, "a file within the limit asks for confirmation")
            eq(dialog().title, "Attach this file?", "with the question as its title")
            check(String(dialog().text).indexOf("notes.pdf") >= 0 && String(dialog().text).indexOf("50000 B") >= 0, "naming the file and its size")
            tapItem(findAll(dialog(), function(i) { return i.text === "Cancel" && i.clicked !== undefined && i.visible }, [])[0])
        })
        after(450, function() {
            check(!dialog().visible, "cancelling closes the question")
            eq(picked.length, 0, "and picks nothing")
            eq(stack.depth, 2, "and stays in the browser")
            tapEntry("notes.pdf")
        })
        after(450, function() {
            tapItem(findAll(dialog(), function(i) { return i.text === "Attach" && i.clicked !== undefined && i.visible }, [])[0])
        })
        after(450, function() {
            eq(picked.length, 1, "confirming picks the file")
            eq(picked[0], "/root/Photo/notes.pdf", "by its full path")
            eq(stack.depth, 1, "and leaves the browser")
        })
        // ---- Up, an empty folder, Cancel -------------------------------------------------------------------------
        after(450, function() {
            openPage()
        })
        after(450, function() {
            tapEntry("Music")
        })
        after(450, function() {
            eq(pathLine(), "/root/Music", "another folder opens")
            check(hasText("This folder is empty or cannot be read."), "an empty folder says so")
            tapItem(button("Up"))
        })
        after(450, function() {
            eq(pathLine(), "/root", "Up returns to the parent")
            tapItem(button("Cancel"))
        })
        after(450, function() {
            eq(stack.depth, 1, "Cancel leaves the browser")
        })
        // ---- spec 009 US1: the root is the top; the reader cannot go above it ------------------------------------
        // (waits of 450 ms between folder changes: the page ignores taps right after a change, see US2)
        after(450, function() {
            openPage()
        })
        after(450, function() {
            eq(pathLine(), "/root", "a fresh page starts at the root")
            eq(button("Up").enabled, false, "Up is not available at the root")
            tapItem(button("Up"))
        })
        after(450, function() {
            eq(pathLine(), "/root", "a tap on the disabled Up changes nothing")
            tapEntry("Photo")
        })
        after(450, function() {
            eq(pathLine(), "/root/Photo", "a first-level folder opens")
            eq(button("Up").enabled, true, "and Up is available inside it")
            tapEntry("Trips")
        })
        after(450, function() {
            eq(pathLine(), "/root/Photo/Trips", "a second-level folder opens")
            tapItem(button("Up"))
        })
        after(450, function() {
            eq(pathLine(), "/root/Photo", "Up goes to the folder above")
            tapItem(button("Up"))
        })
        after(450, function() {
            eq(pathLine(), "/root", "Up from a first-level folder returns to the root, not above it")
            eq(button("Up").enabled, false, "where Up is unavailable again")
            check(entryItem("Photo") !== undefined && entryItem("Music") !== undefined, "and the folders are listed again")
            tapEntry("Photo")
        })
        after(450, function() {
            tapItem(button("Cancel"))
        })
        after(450, function() {
            openPage()
        })
        after(450, function() {
            eq(pathLine(), "/root", "a page opened after browsing starts at the root again")
        })
        after(450, function() {
            tapItem(button("Cancel"))
            attachments.rootIsEmpty = true
        })
        after(450, function() {
            openPage()
        })
        after(450, function() {
            check(hasText("Nothing to browse here"), "an empty root says there is nothing to browse")
            check(!hasText("empty or cannot be read"), "and does not use the text for an empty folder")
            check(button("Cancel") !== undefined && button("Cancel").enabled, "Cancel is available")
            tapItem(button("Cancel"))
            attachments.rootIsEmpty = false
        })
        after(450, function() {
            eq(stack.depth, 1, "Cancel leaves the empty browser")
        })
        // ---- spec 009 US2: List and Tile view ----------------------------------------------------------------------
        var listRows = 0, quickPoint = null
        after(450, function() {
            attachments.pickerView = "list"
            attachments.pickerViewWrites = 0
            openPage()
        })
        after(450, function() {
            check(toggle() !== undefined, "there is a layout toggle at the top")
            eq(toggle().glyph, "grid", "it shows the grid symbol while the list is shown")
            check(listView().visible, "the list is shown")
            check(gridView() === undefined || !gridView().visible, "and no grid")
            tapEntry("Photo")
        })
        after(450, function() {
            listRows = shownCount(listView())
            check(listRows >= 10, "the list shows a page of rows (" + listRows + ")")
            eq(listView().height % 104, 0, "the list ends on a whole row: no row is cut off at the bottom of a page")
            tapItem(toggle())
        })
        after(450, function() {
            check(gridView() !== undefined && gridView().visible, "a tap on the toggle shows tiles")
            check(!listView().visible, "instead of the list")
            eq(toggle().glyph, "list", "the toggle now shows the list symbol")
            eq(pathLine(), "/root/Photo", "the folder stays")
            eq(attachments.pickerView, "tile", "the choice is stored")
            check(attachments.pickerViewWrites >= 1, "by writing it once")
            eq(gridView().columns, 5, "tiles: 5 columns at 1404 px")
            eq(gridView().cellHeight, 250, "each tile row is 250 px")
            eq(gridView().height % 250, 0, "the grid ends on a whole tile row: no tile is cut off at the bottom of a page")
            var tiles = shownCount(gridView())
            check(tiles >= 2 * listRows, "at least twice as many entries per page as the list shows (" + tiles + " vs " + listRows + ")")
            eq(gridView().contentY, 0, "the first page is shown")
            if (typeof arg_shot !== "undefined") pointer.shot(arg_shot + "-tiles.png")   // for looking at it by hand
            tapItem(button("Page down"))
        })
        after(450, function() {
            var g = gridView()
            check(g.contentY > 0, "Page down moves the grid")
            eq(g.contentY % 250, 0, "by whole tile rows")
            tapItem(button("Page up"))
        })
        after(450, function() {
            eq(gridView().contentY, 0, "Page up returns to the first page")
            win.width = 1872; win.height = 1404
        })
        after(450, function() {
            eq(gridView().columns, 6, "in landscape the grid has 6 columns")
            eq(pathLine(), "/root/Photo", "and the folder and the view stay")
            win.width = 1404; win.height = 1872
        })
        after(450, function() {
            eq(gridView().columns, 5, "back in portrait it has 5 again")
            tapEntry("huge.bin")
        })
        after(450, function() {
            check(hasText("larger than the"), "a tile of a file over the limit gives the size message")
            check(!dialog().visible, "and asks nothing")
            tapEntry("img03.jpg")
        })
        after(450, function() {
            check(dialog().visible, "a tile of a file asks for confirmation")
            check(String(dialog().text).indexOf("img03.jpg") >= 0 && String(dialog().text).indexOf("120000 B") >= 0, "naming the file and its size")
            tapItem(findAll(dialog(), function(i) { return i.text === "Attach" && i.clicked !== undefined && i.visible }, [])[0])
        })
        after(450, function() {
            eq(picked.length, 1, "confirming picks the file")
            eq(picked[0], "/root/Photo/img03.jpg", "by its full path")
            eq(stack.depth, 1, "and leaves the browser")
            // a new page starts in the stored view
            openPage()
        })
        after(450, function() {
            check(gridView() !== undefined && gridView().visible, "a new page opens in the stored view: tiles")
            eq(toggle().glyph, "list", "with the list symbol on the toggle")
            tapEntry("Photo")
        })
        after(450, function() {
            tapEntry("Trips")
        })
        after(450, function() {
            eq(pathLine(), "/root/Photo/Trips", "a folder tile opens the folder")
            var longTile = entryItem("a-really-long-file-name-that-goes-on-and-on-and-on-1234567890.pdf")
            check(longTile !== undefined, "the tile with a long name is shown")
            var name = findAll(longTile, function(i) { return i.text === longTile.entry.name }, [])[0]
            eq(name.elide, Text.ElideRight, "a long name is shortened with an ellipsis")
            eq(name.maximumLineCount, 2, "to at most two lines")
            check(name.width <= longTile.width, "and never wider than its tile")
            tapEntry("a-really-long-file-name-that-goes-on-and-on-and-on-1234567890.pdf")
        })
        after(450, function() {
            check(String(dialog().text).indexOf("a-really-long-file-name-that-goes-on-and-on-and-on-1234567890.pdf") >= 0, "the question shows the full name")
            tapItem(findAll(dialog(), function(i) { return i.text === "Cancel" && i.clicked !== undefined && i.visible }, [])[0])
        })
        after(450, function() {
            // the guard: a second tap right after a folder change is ignored, Up and the toggle are not delayed
            tapItem(button("Up"))
        })
        after(450, function() {
            eq(pathLine(), "/root/Photo", "Up returns to Photo")
            quickPoint = scene(entryItem("Trips"), entryItem("Trips").width / 2, entryItem("Trips").height / 2)
            tapAt(quickPoint)
            tapAt(quickPoint)   // the same spot, in the new folder, a moment later
        })
        after(60, function() {
            eq(pathLine(), "/root/Photo/Trips", "two quick taps enter the folder once")
            check(!dialog().visible, "the second tap opened nothing in it")
            tapItem(toggle())
        })
        after(60, function() {
            check(listView().visible, "the toggle is not delayed by the guard")
            tapItem(button("Up"))
        })
        after(60, function() {
            eq(pathLine(), "/root/Photo", "and neither is Up")
        })
        after(450, function() {
            tapEntry("Trips")
        })
        after(450, function() {
            eq(pathLine(), "/root/Photo/Trips", "a tap after the guard time is accepted")
            tapItem(button("Cancel"))
            attachments.pickerView = "list"
        })
        after(450, function() {
            openPage()
        })
        after(450, function() {
            check(listView().visible, "with the stored view list, a new page opens as a list")
            eq(toggle().glyph, "grid", "with the grid symbol on the toggle")
            tapItem(button("Cancel"))
        })
        // ---- spec 009 US3: previews in the tile view ---------------------------------------------------------------
        var imagesOnPage = 0
        after(450, function() {
            attachments.pickerView = "tile"
            openPage()
        })
        after(450, function() {
            tapEntry("Photo")
        })
        after(600, function() {
            var tile = entryItem("img01.jpg")
            check(tile !== undefined, "the first image tile is shown")
            var img = images(tile)[0]
            check(img !== undefined, "an image tile has an Image")
            eq(String(img.source), "image://filepreview/1", "its source is the preview id of the entry")
            eq(img.asynchronous, true, "it loads asynchronously")
            eq(img.cache, false, "without Qt's own cache (the provider keeps a bounded one)")
            check(img.sourceSize.width > 0, "and asks for a width, not the full picture")
            eq(img.fillMode, Image.PreserveAspectFit, "keeping the proportions")
            eq(img.status, Image.Ready, "the preview has loaded")
            check(!fileGlyphOf(tile).visible, "a preview that has loaded hides the file symbol")
            check(images(entryItem("huge.bin")).length === 0, "a file that is not an image has no Image")
            check(images(entryItem("Trips")).length === 0, "and neither has a folder")
            check(folderGlyphOf(entryItem("Trips")).visible, "the folder has its symbol")
            check(fileGlyphOf(entryItem("huge.bin")).visible, "the other file its file symbol")
            if (typeof arg_shot !== "undefined") pointer.shot(arg_shot + "-previews.png")   // for looking at it by hand
            imagesOnPage = images(page).length
            var visibleImageTiles = findAll(gridView(), function(i) { return i.entry !== undefined && i.entry.image && i.y >= gridView().contentY && i.y + i.height <= gridView().contentY + gridView().height }, []).length
            eq(imagesOnPage, visibleImageTiles, "only the tiles of the page have an Image (" + imagesOnPage + ")")
            check(imagesOnPage > 0 && imagesOnPage <= 30, "of at most one page of tiles")
            tapItem(button("Page down"))
        })
        after(600, function() {
            check(entryItem("img01.jpg") === undefined, "the tiles of the first page are gone after paging, with their previews")
            var bad = bringIntoView("img31.jpg")
            check(bad !== undefined, "a tile of the next page exists")
            eq(images(bad)[0].status, Image.Error, "a picture without a preview ends in an error state")
            check(fileGlyphOf(bad).visible, "and keeps the file symbol, so the file can still be picked")
            check(images(entryItem("notes.pdf")).length === 0, "the document tile has no Image")
            var pageImages = images(page).length
            var g2 = gridView()
            var wantedOnPage = findAll(g2, function(i) { return i.entry !== undefined && i.entry.image && i.y >= g2.contentY && i.y + i.height <= g2.contentY + g2.height }, []).length
            eq(pageImages, wantedOnPage, "the Images on this page are those of its image tiles")
            tapItem(toggle())
        })
        after(450, function() {
            check(listView().visible, "the list is shown")
            eq(images(page).length, 0, "the list view has no Image at all: it prepares no preview")
            tapItem(toggle())
        })
        after(600, function() {
            check(gridView().visible, "tiles again")
            check(images(page).length > 0, "with their previews")
            tapItem(button("Cancel"))
            attachments.pickerView = "list"
        })
        runner.interval = queue[0].ms
        runner.start()
    }
}
