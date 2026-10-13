import QtQuick
import QtQuick.Controls
import QtQuick.Window

// Spec 010 (US5): the real SetupPage in the look of the device: a top bar, three labelled fields, the password hidden until a
// control shows it, a footer of Test connection and Save, and the same behavior as before (Save only after a successful test,
// the entries kept as a draft, the password cleared after saving).
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

    Loader { id: ld; anchors.fill: parent; source: pageUrl }

    Component.onCompleted: {
        after(400, function() {
            page = ld.item
            check(page !== null, "the set-up page loads")
            var title = named("topBarTitle")
            check(title !== undefined && title.text === "Set up", "a top bar titled Set up")
            check(control("Back") === undefined, "without a Back: this is the first screen")
            eq(inkButtons().length, 0, "no bordered text button is on the page")
            check(hasText("Server address") && hasText("User name") && hasText("App password"), "the three fields have their labels")
            check(hasText("stored on this reader without encryption"), "and the warning about the password stays")
            var url = named("urlField"), user = named("userField"), pass = named("passField")
            check(url !== undefined && user !== undefined && pass !== undefined, "there are three fields")
            eq(pass.field.echoMode, TextInput.Password, "the password is hidden at first")
            var show = control("Show password")
            check(show !== undefined, "with a control that shows it")
            tapItem(show)
        })
        after(100, function() {
            eq(named("passField").field.echoMode, TextInput.Normal, "the control shows the password")
            tapItem(control("Hide password"))
        })
        after(100, function() {
            eq(named("passField").field.echoMode, TextInput.Password, "and hides it again")
            var test = control("Test connection"), save = control("Save")
            check(test !== undefined && save !== undefined, "the footer has Test connection and Save")
            check(!save.enabled, "Save is off before a successful test")
            named("urlField").text = "https://cloud.example/"
            named("userField").text = "alice"
            named("passField").text = "app-password"
            tapItem(test)
        })
        after(200, function() {
            eq(bridge.tests.length, 1, "Test connection tests once")
            eq(bridge.tests[0].join("|"), "https://cloud.example/|alice|app-password", "with what was typed")
            check(hasText("Connected."), "and shows the result")
            check(control("Save").enabled, "Save is on after a successful test")
            tapItem(control("Save"))
        })
        after(200, function() {
            eq(bridge.saved.length, 1, "Save saves the account")
            eq(bridge.saved[0].join("|"), "https://cloud.example/|alice|app-password", "with what was tested")
            eq(named("passField").text, "", "and clears the password field")
            named("userField").text = "bob"
        })
        after(100, function() { check(!control("Save").enabled, "changing an entry switches Save off until it is tested again") })
        runner.interval = queue[0].ms
        runner.start()
    }
}
