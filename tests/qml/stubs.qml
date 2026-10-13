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
    }
    property var device: QtObject {
        signal networkResult(bool connected)
        function wifiConnected() { return true }
        function connectNetwork() {}
        property int opened: 0
        function openUrl(url) { opened++; return true }
    }
}
