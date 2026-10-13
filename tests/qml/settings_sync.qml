import QtQuick
import QtQuick.Controls
import QtQuick.Window

// Spec 008 (US2) and spec 010 (US5): the real Settings page shows "Auto Sync" as a pill switch with its state, a tap flips it,
// and the page says in words what it does; rotation and Forget account are rows; the top bar has Back.
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
    function named(name) { return findAll(page, function(i) { return i.objectName === name }, [])[0] }
    function syncSwitch() { return named("autoSyncSwitch") }
    function texts() {
        return findAll(page, function(i) { return i.text !== undefined && i.clicked === undefined && i.visible }, []).map(function(t) { return String(t.text) })
    }
    function control(text) { return findAll(page, function(i) { return i.text === text && i.clicked !== undefined && i.visible }, [])[0] }
    function inkButtons() { return findAll(page, function(i) { return i.visible && String(i).indexOf("InkButton") === 0 }, []) }

    Loader { id: ld; anchors.fill: parent; source: pageUrl }

    Component.onCompleted: {
        after(300, function() {
            page = ld.item
            check(page !== null, "the settings page loads")
            var title = named("topBarTitle")
            check(title !== undefined && title.text === "Settings", "a top bar titled Settings")
            check(control("Back") !== undefined, "with a Back control")
            eq(inkButtons().length, 0, "no bordered text button is on the page")
            var sw = syncSwitch()
            check(sw !== undefined, "there is an Auto Sync switch")
            eq(sw.checked, true, "it is on by default")
            check(findAll(sw, function(i) { return i.text === "on" && i.visible }, []).length > 0, "and shows on")
            check(texts().some(function(t) { return t.indexOf("syncs a few seconds after you save") >= 0 }), "the page explains what it does")
            check(texts().indexOf("Synchronization") >= 0 || texts().indexOf("synchronization") >= 0 || findAll(page, function(i) { return i.text === "Synchronization" }, []).length > 0, "the section is titled Synchronization")
            check(findAll(page, function(i) { return i.objectName === "rowDivider" }, []).length >= 3, "rows are separated by dotted dividers")
            check(sw.enabled, "the switch is usable (also without an account)")
            var p = sw.mapToItem(null, sw.width / 2, sw.height / 2)
            tapAt(p)
        })
        after(100, function() {
            eq(bridge.syncAfterSave, false, "a tap switches it off")
            eq(syncSwitch().checked, false, "and the switch says so")
            check(findAll(syncSwitch(), function(i) { return i.text === "off" && i.visible }, []).length > 0, "off")
            var p = syncSwitch().mapToItem(null, syncSwitch().width / 2, syncSwitch().height / 2)
            tapAt(p)
        })
        after(100, function() {
            eq(bridge.syncAfterSave, true, "a second tap switches it on again")
            eq(syncSwitch().checked, true, "and the switch says so")
        })
        // rotation: a row whose value cycles
        after(100, function() {
            var row = control("Screen rotation")
            check(row !== undefined, "the rotation is a row")
            eq(row.value, "Follow the sensor", "reading Follow the sensor")
            tapAt(row.mapToItem(null, row.width / 2, row.height / 2))
        })
        after(100, function() {
            eq(bridge.rotationPreference, "portrait", "a tap moves to portrait only")
            eq(control("Screen rotation").value, "Portrait only", "and the row says so")
            tapAt(control("Screen rotation").mapToItem(null, 60, 30))
        })
        after(100, function() {
            eq(bridge.rotationPreference, "landscape", "then landscape only")
            tapAt(control("Screen rotation").mapToItem(null, 60, 30))
        })
        after(100, function() { eq(bridge.rotationPreference, "auto", "then back to auto") })
        // forget account and about
        after(100, function() {
            var row = control("Forget account")
            check(row !== undefined && row.enabled, "Forget account is a row, usable with an account")
            tapAt(row.mapToItem(null, 60, 30))
        })
        after(200, function() {
            var d = findAll(page, function(i) { return i.title === "Forget this account?" && i.visible }, [])[0]
            check(d !== undefined, "Forget account asks first")
            var cancel = findAll(d, function(i) { return i.objectName === "footerButton" && i.text === "Cancel" }, [])[0]
            tapAt(cancel.mapToItem(null, cancel.width / 2, cancel.height / 2))
        })
        after(100, function() {
            bridge.configured = false
            check(!control("Forget account").enabled, "Forget account is off without an account")
            bridge.configured = true
        })
        runner.interval = queue[0].ms
        runner.start()
    }
}
