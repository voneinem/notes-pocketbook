import QtQuick
import QtQuick.Window
import "../../qml"

// Spec 010 (foundation, US2): the Style singleton, the icons and the shared controls (Icon, IconButton, TopBar,
// RowDivider). The driver imports the qml/ directory itself, so the singleton resolves through qml/qmldir.
Window {
    id: win
    width: 1080; height: 1700
    visible: true
    color: "#ffffff"
    property int failures: 0
    property var queue: []
    property int clicks: 0

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
    function tap(item) {
        var p = item.mapToItem(null, item.width / 2, item.height / 2)
        pointer.press(p.x, p.y)
        pointer.release(p.x, p.y)
    }

    // the icons of slice 2 (the file names in qml/icons/)
    readonly property var iconNames: ["home", "menu", "search", "note-plus", "chevron-left", "chevron-up", "chevron-down", "close",
                                      "grid", "list", "filter", "star", "star-filled", "gear", "sync", "row-dots",
                                      // slice 3: pop-up pointer, selection menu, "/"-menu entries
                                      "bubble-pointer", "copy", "cut", "paste", "select-all", "trash", "undo", "redo", "check",
                                      "slash-bullet_list", "slash-table", "slash-info_box", "slash-link", "slash-bold", "slash-italic",
                                      "slash-underline", "slash-strikethrough", "slash-details", "slash-quote", "slash-code_block", "slash-emoji",
                                      // slice 4: reader and editor
                                      "pencil", "save", "attach", "tag", "text-size", "minus-circle", "plus-circle", "info",
                                      // slice 5: remaining screens
                                      "eye", "eye-filled", "folder", "chevron-right", "download"]

    Repeater {
        id: icons
        model: win.iconNames
        delegate: Icon { objectName: "icon_" + modelData; source: modelData; size: 64 }
    }
    IconButton { id: searchButton; x: 10; y: 200; text: "Search"; icon: "search"; onClicked: win.clicks++ }
    IconButton { id: offButton; x: 10; y: 320; text: "Off"; icon: "search"; enabled: false; onClicked: win.clicks += 100 }
    IconButton { id: starButton; x: 10; y: 440; text: "Star"; icon: "star"; toggled: false }
    IconButton { id: starOn; x: 200; y: 440; text: "StarOn"; icon: "star"; toggled: true }
    IconButton { id: noIcon; x: 400; y: 440; text: "Label only"; icon: "" }
    TopBar {
        id: bar
        y: 600
        width: parent.width
        title: "Notes"
        leftItems: [ IconButton { text: "Home"; icon: "home" } ]
        rightItems: [ IconButton { text: "Search2"; icon: "search" }, IconButton { text: "New"; icon: "note-plus" } ]
    }
    RowDivider { id: divider; x: 20; y: 900; width: 500 }
    property var sliderMoves: []
    InkSlider {
        id: slider
        x: 20; y: 1000; width: 800
        stops: 3; index: 1; label: "9 pt"
        onMoved: function(i) { var l = win.sliderMoves.slice(); l.push(i); win.sliderMoves = l; slider.index = i }
    }
    property bool switchOn: true
    InkSwitch { id: sw; x: 500; y: 1120; text: "Auto Sync"; checked: win.switchOn; onToggled: function(on) { win.switchOn = on } }
    InkRow { id: row; x: 20; y: 1340; width: 700; text: "Screen rotation"; value: "Follow the sensor"; chevron: true; onClicked: win.clicks += 100000 }
    InkRow { id: rowOff; x: 20; y: 1460; width: 700; text: "Forget account"; enabled: false; onClicked: win.clicks += 10000000 }
    ToolButton { id: tool; x: 20; y: 1120; text: "Text size"; icon: "text-size"; onClicked: win.clicks += 10 }
    ToolButton { id: toolOff; x: 220; y: 1120; text: "Nope"; icon: "trash"; enabled: false; onClicked: win.clicks += 1000 }
    BottomPanel {
        id: panel
        y: 1230
        width: 1080
        tools: [ ToolButton { text: "Attach"; icon: "attach" }, ToolButton { text: "Category"; icon: "tag" }, ToolButton { text: "Delete"; icon: "trash" } ]
    }

    Component.onCompleted: {
        after(300, function() {
            // ---- Style ----
            eq(Style.hairlineWidth, 1, "Style: hairline width 1")
            eq(Style.ruleWidth, 2, "Style: rule width 2")
            eq(Style.heavyWidth, 4, "Style: heavy outline 4")
            eq(Style.barHeight, 112, "Style: top bar 112")
            eq(Style.bottomBarHeight, 92, "Style: bottom bar 92")
            eq(Style.touchMin, 96, "Style: touch minimum 96")
            eq(Style.rowHeight, 132, "Style: list row 132")
            eq(Style.tileHeight, 190, "Style: tile cell height 190")
            eq(Style.topInset, 0, "Style: top inset 0 until the device says otherwise")
            eq(Style.ptFor(30), 7, "Style: 30 px is 7 pt")
            eq(Style.ptFor(38), 9, "Style: 38 px is 9 pt")
            eq(Style.ptFor(50), 12, "Style: 50 px is 12 pt")

            // ---- icons ----
            for (var i = 0; i < win.iconNames.length; ++i) {
                var ic = findAll(win.contentItem, function(it) { return it.objectName === "icon_" + win.iconNames[i] }, [])[0]
                check(ic !== undefined && ic.ready === true, "the icon " + win.iconNames[i] + " loads")
            }

            // ---- IconButton ----
            eq(searchButton.text, "Search", "IconButton keeps its label as text")
            check(searchButton.width >= 96 && searchButton.height >= 96, "IconButton hit area is at least 96 x 96")
            tap(searchButton)
            eq(win.clicks, 1, "a tap on an IconButton emits clicked once")
            tap(offButton)
            eq(win.clicks, 1, "a disabled IconButton does not emit clicked")
            var starIcon = findAll(starButton, function(it) { return it.ready !== undefined }, [])[0]
            var starOnIcon = findAll(starOn, function(it) { return it.ready !== undefined }, [])[0]
            eq(starIcon.source, "star", "an untoggled star shows the outline")
            eq(starOnIcon.source, "star-filled", "a toggled star shows star-filled")
            var labels = findAll(noIcon, function(it) { return it.text === "Label only" && it.visible && it.width > 0 }, [])
            check(labels.length > 0, "without an icon the control shows its label")
            var hidden = findAll(searchButton, function(it) { return it.text === "Search" && it.visible && it.width > 0 && it !== searchButton }, [])
            check(hidden.length === 0, "with an icon the label is not drawn")

            // ---- TopBar ----
            var title = findAll(bar, function(it) { return it.objectName === "topBarTitle" }, [])[0]
            check(title !== undefined, "the top bar has a title")
            eq(title.text, "Notes", "the title is the given text")
            eq(title.font.capitalization, Font.AllUppercase, "and it is upper-cased by the style, not by the string")
            var c = title.mapToItem(bar, title.width / 2, 0).x
            check(Math.abs(c - bar.width / 2) <= 2, "the title is centred (within 2 px)")
            eq(bar.height, Style.barHeight + Style.topInset, "the bar has the style's height")
            var rule = findAll(bar, function(it) { return it.objectName === "topBarRule" }, [])[0]
            check(rule !== undefined && rule.height === 1 && Math.abs(rule.y + rule.height - bar.height) <= 1, "a 1 px hairline sits at the bottom")
            var home = findAll(bar, function(it) { return it.text === "Home" }, [])[0]
            check(home !== undefined && home.mapToItem(bar, 0, 0).x < 100, "left items sit at the left")
            var neu = findAll(bar, function(it) { return it.text === "New" }, [])[0]
            check(neu !== undefined && neu.mapToItem(bar, neu.width, 0).x > bar.width - 60, "right items sit at the right")

            // ---- RowDivider ----
            eq(divider.width, 500, "RowDivider is as wide as it is given")
            check(divider.height >= 1 && divider.height <= 3, "RowDivider is a hairline")

            // ---- ToolButton ----
            eq(tool.text, "Text size", "ToolButton keeps its label as text")
            check(tool.width >= 96 && tool.height >= 96, "ToolButton hit area is at least 96 x 96")
            var toolIcon = findAll(tool, function(it) { return it.ready !== undefined }, [])[0]
            var toolLabel = findAll(tool, function(it) { return it.text === "Text size" && it !== tool && it.visible }, [])[0]
            check(toolIcon !== undefined && toolIcon.ready, "its icon loads")
            check(toolLabel !== undefined && toolLabel.mapToItem(tool, 0, 0).y >= toolIcon.mapToItem(tool, 0, 0).y + toolIcon.height - 4, "the label sits under the icon")
            tap(tool)
            eq(win.clicks, 11, "a tap on a ToolButton emits clicked")
            tap(toolOff)
            eq(win.clicks, 11, "a disabled ToolButton does not")

            // ---- InkSwitch ----
            check(sw.width >= 150 && sw.height >= 70, "the switch is a pill of a usable size")
            check(findAll(sw, function(it) { return it.text === "on" && it.visible }, []).length > 0, "an on switch shows on")
            check(findAll(sw, function(it) { return it.text === "off" && it.visible }, []).length === 0, "and not off")
            tap(sw)
            eq(win.switchOn, false, "a tap on the pill reports the new state")
            check(findAll(sw, function(it) { return it.text === "off" && it.visible }, []).length > 0, "and the pill shows off")
            tap(sw)
            eq(win.switchOn, true, "a second tap turns it on again")

            // ---- InkRow ----
            eq(row.text, "Screen rotation", "a row has its label as text")
            check(row.height >= 96, "a row is tall enough to hit")
            check(findAll(row, function(it) { return it.text === "Follow the sensor" && it.visible }, []).length > 0, "it shows its value")
            check(findAll(row, function(it) { return it.source === "chevron-right" }, []).length > 0, "and a chevron")
            check(findAll(row, function(it) { return it.objectName === "rowDivider" }, []).length > 0, "a dotted divider closes the row")
            var before = win.clicks
            tap(row)
            eq(win.clicks, before + 100000, "a tap on a row emits clicked")
            tap(rowOff)
            eq(win.clicks, before + 100000, "a disabled row does not")

            // ---- BottomPanel ----
            var prule = findAll(panel, function(it) { return it.objectName === "bottomPanelRule" }, [])[0]
            check(prule !== undefined && prule.height === 1 && prule.y === 0, "a hairline on top of the panel")
            var t1 = findAll(panel, function(it) { return it.text === "Attach" && it.clicked !== undefined }, [])[0]
            var t2 = findAll(panel, function(it) { return it.text === "Category" && it.clicked !== undefined }, [])[0]
            var t3 = findAll(panel, function(it) { return it.text === "Delete" && it.clicked !== undefined }, [])[0]
            var c1 = t1.mapToItem(panel, t1.width / 2, 0).x, c2 = t2.mapToItem(panel, t2.width / 2, 0).x, c3 = t3.mapToItem(panel, t3.width / 2, 0).x
            check(Math.abs((c2 - c1) - (c3 - c2)) <= 2, "the tools are spread evenly")
            check(c1 > 0 && c3 < panel.width, "and lie inside the panel")

            // ---- InkSlider ----
            var minus = findAll(slider, function(it) { return it.text === "Decrease" && it.clicked !== undefined }, [])[0]
            var plus = findAll(slider, function(it) { return it.text === "Increase" && it.clicked !== undefined }, [])[0]
            check(minus !== undefined && plus !== undefined, "the slider has round minus and plus controls")
            check(minus.enabled && plus.enabled, "both are on in the middle")
            check(findAll(slider, function(it) { return it.text === "9 pt" && it.visible }, []).length > 0, "the label shows the size with its unit")
            var knob = findAll(slider, function(it) { return it.objectName === "sliderKnob" }, [])[0]
            var track = findAll(slider, function(it) { return it.objectName === "sliderTrack" }, [])[0]
            check(knob !== undefined && track !== undefined, "a ring knob on a track")
            function stopX(i) { return track.mapToItem(null, track.width * i / 2, 0).x }
            check(Math.abs(knob.mapToItem(null, knob.width / 2, 0).x - stopX(1)) <= 4, "the knob sits on the middle stop")
            tap(plus)
            eq(slider.index, 2, "plus moves to the last stop")
            eq(win.sliderMoves[win.sliderMoves.length - 1], 2, "and reports it")
            check(!plus.enabled, "plus is off at the last stop")
            check(Math.abs(knob.mapToItem(null, knob.width / 2, 0).x - stopX(2)) <= 4, "the knob follows")
            tap(minus); tap(minus)
            eq(slider.index, 0, "minus twice moves to the first stop")
            check(!minus.enabled, "minus is off at the first stop")
            var mid = track.mapToItem(null, track.width / 2, track.height / 2)
            pointer.press(mid.x, mid.y); pointer.release(mid.x, mid.y)
            eq(slider.index, 1, "a tap on the middle of the track moves to the middle stop")
            var near = track.mapToItem(null, track.width * 0.92, track.height / 2)
            pointer.press(near.x, near.y); pointer.release(near.x, near.y)
            eq(slider.index, 2, "a tap near the end moves to the nearest stop")
        })
        runner.interval = queue[0].ms
        runner.start()
    }
}
