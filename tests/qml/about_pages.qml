import QtQuick
import QtQuick.Controls
import QtQuick.Window

// Spec 010 (US5): the About page, the third-party list and a license text in the look of the device: each has a top bar with
// Back, hairline rows, and the paging buttons; the rows that were bordered buttons are tappable rows.
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
    function tapItem(item) { var p = item.mapToItem(null, item.width / 2, item.height / 2); pointer.press(p.x, p.y); pointer.release(p.x, p.y) }
    function named(name) { return findAll(page, function(i) { return i.objectName === name }, [])[0] }
    function control(text) { return findAll(page, function(i) { return i.text === text && i.clicked !== undefined && i.visible }, [])[0] }
    function inkButtons() { return findAll(page, function(i) { return i.visible && String(i).indexOf("InkButton") === 0 }, []) }
    function hasText(part) { return findAll(page, function(i) { return i.text !== undefined && i.clicked === undefined && i.visible && String(i.text).indexOf(part) >= 0 }, []).length > 0 }
    function cur() { return stack.currentItem }

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
        after(300, function() { page = stack.push(pageUrl) })
        after(400, function() {
            var title = named("topBarTitle")
            check(title !== undefined && title.text === "About", "the About page has a top bar titled About")
            check(control("Back") !== undefined, "with a Back")
            eq(inkButtons().length, 0, "no bordered text button is on the page")
            check(hasText("Notes Pocketbook") && hasText("Version 1.10.0") && hasText("Author: A. Author") && hasText("License: MIT License"), "name, version, author and license are shown")
            check(hasText("Copyright (c) 2026 A. Author"), "and the copyright")
            check(control("License") !== undefined && control("Third-party licenses") !== undefined, "License and Third-party licenses are rows")
            tapItem(control("License"))
        })
        after(300, function() {
            eq(stack.depth, 3, "License opens the license text")
            page = cur()
            eq(named("topBarTitle").text, "MIT License", "its top bar carries the title of the text")
            check(control("Back") !== undefined, "with a Back")
            eq(inkButtons().length, 0, "no bordered text button on the license page")
            check(hasText("Permission is hereby granted."), "the paragraphs are shown")
            check(hasText("Holder:"), "and the header lines")
            check(control("Page down") !== undefined, "with the page controls")
            tapItem(control("Back"))
        })
        after(300, function() {
            eq(stack.depth, 2, "Back returns to About")
            page = cur()
            tapItem(control("Third-party licenses"))
        })
        after(300, function() {
            eq(stack.depth, 3, "Third-party licenses opens the list")
            page = cur()
            eq(named("topBarTitle").text, "Third-party licenses", "titled Third-party licenses")
            eq(inkButtons().length, 0, "no bordered text button on the list page")
            check(hasText("Alpha") && hasText("Beta") && hasText("Gamma"), "the components are listed")
            check(control("Page down") !== undefined, "with the page controls")
            var row = findAll(page, function(i) { return i.component !== undefined && i.component.id === "beta" && i.visible }, [])[0]
            check(row !== undefined, "a row per component")
            tapItem(row)
        })
        after(300, function() {
            eq(stack.depth, 4, "a row opens its license text")
            page = cur()
            eq(named("topBarTitle").text, "beta", "for that component")
            tapItem(control("Back"))
        })
        after(300, function() {
            page = cur()
            tapItem(control("Back"))
        })
        after(300, function() {
            page = cur()
            eq(stack.depth, 2, "Back from the list returns to About")
            tapItem(control("Back"))
        })
        after(300, function() { eq(stack.depth, 1, "Back from About leaves it") })
        runner.interval = queue[0].ms
        runner.start()
    }
}
