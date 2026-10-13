import QtQuick

// Stand-ins for the objects the QML pages use, so a page can be loaded and driven without a store or a server.
QtObject {
    property var bridge: QtObject {
        signal errorOccurred(string messageKey, string text)
        property int fontPx: 38
        property string fontSize: "medium"
        property var savedNote: null
        function loadForEdit(id) { return ({}) }
        function saveNote(id, title, text, category, favorite) { savedNote = { title: title, text: text, category: category, favorite: favorite }; return 1 }
        function categoryChoices(current) { return [] }
        function checkCategory(name) { return ({ ok: true, name: name, errorText: "" }) }
        function deleteNote(id) { return true }
        function message(key) { return key }
        property var noteToOpen: ({})
        function openNote(id) { return noteToOpen }
        function toggleTask(id, line) { return true }
        // Settings page (spec 008 adds syncAfterSave; the others are what the page already reads)
        property string caSource: "device"
        property string customCaStatus: "none"
        property int customCaCertificates: 0
        property string customCaPath: "/data/custom-ca.pem"
        property string rotationPreference: "auto"
        property bool configured: true
        property string username: "alice"
        property string serverUrl: "https://notes.example"
        function forgetAccount() {}
        property bool syncAfterSave: true
        property var editorOpenCalls: []
        function setEditorOpen(open) { var l = editorOpenCalls.slice(); l.push(open); editorOpenCalls = l }
        property string lastCopyArgs: ""
        function readerCopy(id, fu, fo, tu, to) { lastCopyArgs = [id, fu, fo, tu, to].join(","); return "COPIED" }
        // file browser (spec 002 / 009)
        function formatSize(bytes) { return bytes + " B" }
    }
    property var attachments: QtObject {
        signal stageFinished(var result)
        signal attachmentsChanged(int noteId)
        signal attachmentFailed(int noteId, string remotePath, string text)
        property bool supported: false
        function stageAttachment(path) {}
        function discardStaged() {}
        function probeSizes(id) {}
        function download(id, path) {}
        function openAttachment(id) {}
        // file browser (spec 009): the stub plays the bridge, so the root already lists only the named folders
        property string rootDirectory: "/root"
        property real maxBytes: 1000000
        property string pickerView: "list"
        property int pickerViewWrites: 0
        onPickerViewChanged: pickerViewWrites++
        property bool rootIsEmpty: false
        function entry(name, dir, size, image, id) { return ({ name: name, dir: dir, size: size, image: image, previewId: id }) }
        function listDirectory(path) {
            if (path === "/root")
                return rootIsEmpty ? [] : [entry("Music", true, 0, false, 0), entry("Photo", true, 0, false, 0), entry("screens", true, 0, false, 0)]
            if (path === "/root/Photo") {
                var out = [entry("Trips", true, 0, false, 0), entry("huge.bin", false, 2000000, false, 0)]
                for (var i = 1; i <= 40; ++i)
                    out.push(entry("img" + (i < 10 ? "0" : "") + i + ".jpg", false, 120000, true, i))
                out.push(entry("notes.pdf", false, 50000, false, 0))
                return out
            }
            if (path === "/root/Photo/Trips") return [entry("beach.jpg", false, 90000, true, 41), entry("map.pdf", false, 30000, false, 0),
                                                 entry("a-really-long-file-name-that-goes-on-and-on-and-on-1234567890.pdf", false, 1000, false, 0)]
            return []
        }
    }
    property var device: QtObject {
        signal networkResult(bool connected)
        function wifiConnected() { return true }
        function connectNetwork() {}
        property int opened: 0
        function openUrl(url) { opened++; return true }
    }
}
