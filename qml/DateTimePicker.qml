import QtQuick

// The date-time picker of the editor's "//" (spec 006 US5): five columns (year, month, day, hour, minute), each a tall
// "up" button, the value and a "down" button, and Insert / Cancel. It starts at the date and time the page gives it
// (`open(date)`); only days that exist in the chosen month and year can be reached: the day wraps within the month and a
// month or year change clamps it (31 March -> February gives 28 or 29). The minute moves in steps of five from the
// value shown (the exact minute of "now" is what Insert writes if nothing is touched); the hour, day and month wrap.
// Insert reports the text `YYYY-MM-DD HH:MM` (24 hours, from the slash bridge). No button takes focus, nothing animates.
Item {
    id: root
    objectName: "dateTimePicker"
    property int year: 2026
    property int month: 1
    property int day: 1
    property int hour: 0
    property int minute: 0
    property real minY: 0
    property real maxY: 100000
    signal accepted(string text)
    signal cancelled()

    visible: false
    z: 2000
    width: panel.width
    height: panel.height

    readonly property int minYear: 1970
    readonly property int maxYear: 2099
    readonly property int days: slash.daysInMonth(year, month)
    readonly property string preview: slash.timestamp(year, month, day, hour, minute)

    function open(date) {
        year = date.getFullYear(); month = date.getMonth() + 1; day = date.getDate()
        hour = date.getHours(); minute = date.getMinutes()
        visible = true
        place()
    }
    function close() { visible = false }
    function place() {
        x = Math.max(0, ((parent ? parent.width : width) - width) / 2)
        y = Math.max(minY + 10, minY + (maxY - minY - height) / 2)
    }
    onWidthChanged: if (visible) place()
    onHeightChanged: if (visible) place()

    function wrap(v, lo, hi) { return v > hi ? lo : (v < lo ? hi : v) }
    function stepYear(d) { year = Math.min(maxYear, Math.max(minYear, year + d)); day = Math.min(day, days) }
    function stepMonth(d) { month = wrap(month + d, 1, 12); day = Math.min(day, days) }
    function stepDay(d) { day = wrap(day + d, 1, days) }
    function stepHour(d) { hour = wrap(hour + d, 0, 23) }
    function stepMinute(d) {
        var m = d > 0 ? Math.floor(minute / 5) * 5 + 5 : Math.ceil(minute / 5) * 5 - 5
        minute = m > 59 ? 0 : (m < 0 ? 55 : m)
    }

    function pad(v, n) { var s = String(v); while (s.length < n) s = "0" + s; return s }

    Rectangle {
        id: panel
        readonly property real cell: Math.max(96, Math.min(150, ((root.parent ? root.parent.width : 800) - 80 - 4 * 12) / 5))
        width: 5 * cell + 4 * 12 + 2 * 24
        height: content.height + 2 * 24
        radius: 14
        color: "#ffffff"
        border.width: 4
        border.color: "#000000"

        Column {
            id: content
            x: 24; y: 24
            spacing: 14
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 34
                font.bold: true
                color: "#000000"
                text: qsTr("Date and time")
            }
            Row {
                spacing: 12
                Repeater {
                    model: [
                        { key: "Year", label: qsTr("Year") },
                        { key: "Month", label: qsTr("Month") },
                        { key: "Day", label: qsTr("Day") },
                        { key: "Hour", label: qsTr("Hour") },
                        { key: "Minute", label: qsTr("Min") }
                    ]
                    delegate: Column {
                        spacing: 8
                        width: panel.cell
                        function value() {
                            if (modelData.key === "Year") return root.pad(root.year, 4)
                            if (modelData.key === "Month") return root.pad(root.month, 2)
                            if (modelData.key === "Day") return root.pad(root.day, 2)
                            if (modelData.key === "Hour") return root.pad(root.hour, 2)
                            return root.pad(root.minute, 2)
                        }
                        function step(d) {
                            if (modelData.key === "Year") root.stepYear(d)
                            else if (modelData.key === "Month") root.stepMonth(d)
                            else if (modelData.key === "Day") root.stepDay(d)
                            else if (modelData.key === "Hour") root.stepHour(d)
                            else root.stepMinute(d)
                        }
                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: 24
                            color: "#000000"
                            text: modelData.label
                        }
                        InkButton {
                            objectName: "up" + modelData.key
                            width: parent.width
                            text: "▲"
                            focusPolicy: Qt.NoFocus
                            onClicked: parent.step(1)
                        }
                        Text {
                            objectName: "val" + modelData.key
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: 40
                            color: "#000000"
                            text: parent.value()
                        }
                        InkButton {
                            objectName: "down" + modelData.key
                            width: parent.width
                            text: "▼"
                            focusPolicy: Qt.NoFocus
                            onClicked: parent.step(-1)
                        }
                    }
                }
            }
            Text {
                objectName: "preview"
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 34
                color: "#000000"
                text: root.preview
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 24
                InkButton {
                    text: qsTr("Insert")
                    focusPolicy: Qt.NoFocus
                    width: 220
                    onClicked: root.accepted(root.preview)
                }
                InkButton {
                    text: qsTr("Cancel")
                    focusPolicy: Qt.NoFocus
                    width: 220
                    onClicked: root.cancelled()
                }
            }
        }
    }
}
