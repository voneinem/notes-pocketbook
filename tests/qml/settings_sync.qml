import QtQuick
import QtQuick.Controls
import QtQuick.Window

// Spec 008 (US2): the real Settings page shows "Auto Sync" with its state, a tap flips it, and the page says in
// words what it does.
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
    function tapAt(p) { pointer.press(p.x, p.y); pointer.release(p.x, p.y) }
    function syncButton() {
        return findAll(page, function(i) { return i.text !== undefined && i.clicked !== undefined && String(i.text).indexOf("Auto Sync") === 0 }, [])[0]
    }
    function texts() {
        return findAll(page, function(i) { return i.text !== undefined && i.clicked === undefined && i.visible }, []).map(function(t) { return String(t.text) })
    }

    Loader { id: ld; anchors.fill: parent; source: pageUrl }

    Component.onCompleted: {
        after(300, function() {
            page = ld.item
            check(page !== null, "the settings page loads")
            var b = syncButton()
            check(b !== undefined, "there is an Auto Sync button")
            eq(b.text, "Auto Sync: on", "it reads on by default")
            check(texts().some(function(t) { return t.indexOf("syncs a few seconds after you save") >= 0 }), "the page explains what it does")
            check(texts().indexOf("Synchronization") >= 0, "the section is titled Synchronization")
            var b2 = syncButton()
            tapAt(scene(b2, b2.width / 2, b2.height / 2))
        })
        after(100, function() {
            eq(bridge.syncAfterSave, false, "a tap switches it off")
            eq(syncButton().text, "Auto Sync: off", "and the button says so")
            var b = syncButton()
            tapAt(scene(b, b.width / 2, b.height / 2))
        })
        after(100, function() {
            eq(bridge.syncAfterSave, true, "a second tap switches it on again")
            eq(syncButton().text, "Auto Sync: on", "and the button says so")
            check(syncButton().enabled, "the button is usable (also without an account)")
        })
        runner.interval = queue[0].ms
        runner.start()
    }
}
