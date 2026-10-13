import QtQuick

// Stand-ins for the objects the QML pages use, so a page can be loaded and driven without a store or a server.
QtObject {
    property var bridge: QtObject {
        signal errorOccurred(string messageKey, string text)
        property string fontSize: "medium"
        property int fontPx: fontSize === "small" ? 30 : (fontSize === "large" ? 50 : 38)   // as the real bridge maps them
        property var savedNote: null
        function loadForEdit(id) { return id === 7 ? ({ title: "Old title", text: "Old body", category: "", favorite: false }) : ({}) }
        function saveNote(id, title, text, category, favorite) { savedNote = { title: title, text: text, category: category, favorite: favorite }; return 1 }
        function categoryChoices(current) {
            return [{ label: "No category", value: "none" }, { label: "Work", value: "c:Work" }, { label: "New category...", value: "new" }]
        }
        function checkCategory(name) { return ({ ok: true, name: name, errorText: "" }) }
        property int deletedId: 0
        function deleteNote(id) { deletedId = id; return true }
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
        // set-up page (spec 010 tests): the entries typed last time, the connection test and saving the account
        signal connectionTested(bool ok, string messageKey, string message, string normalizedUrl)
        property bool busy: false
        function draft() { return ({}) }
        property var draftSaves: []
        function saveDraft(u, n, p) { var l = draftSaves.slice(); l.push([u, n, p]); draftSaves = l }
        property var tests: []
        function testConnection(u, n, p) { var l = tests.slice(); l.push([u, n, p]); tests = l; connectionTested(true, "ok", "Connected.", u) }
        property var saved: []
        function saveAccount(u, n, p) { var l = saved.slice(); l.push([u, n, p]); saved = l; return true }
        // note list (spec 010): 60 notes, every 7th a favorite, every 5th in "Work", every 11th in "Home", every 13th pending;
        // note 12 has a 120-character title, note 14 an emoji-only one; `modified` descends with the id
        signal notesChanged
        property string view: "all"
        property string sortOrder: "modified"
        property string searchText: ""
        property bool syncActive: false
        property bool syncRunning: false
        property string syncOutcome: "ok"
        property string syncStatus: "Synced."
        property int pendingCount: 3
        property int syncNowCalls: 0
        function syncNow() { syncNowCalls++ }
        property var viewCalls: []
        function setView(v) { var l = viewCalls.slice(); l.push(v); viewCalls = l; view = v; notesChanged() }
        property var sortCalls: []
        function setSortOrder(o) { var l = sortCalls.slice(); l.push(o); sortCalls = l; sortOrder = o; notesChanged() }
        property var searchCalls: []
        function setSearch(t) { var l = searchCalls.slice(); l.push(t); searchCalls = l; searchText = t; notesChanged() }
        property var categories: [{ name: "Work", count: 12 }, { name: "Home", count: 6 }]
        property int noteCount: 60
        function formatTime(t) { return "Today" }
        property string listLayout: "list"
        property int listLayoutWrites: 0
        onListLayoutChanged: listLayoutWrites++
        function listNotes() {
            var out = []
            for (var i = 1; i <= noteCount; ++i) {
                var title = i === 12 ? "A very long title that goes on and on and on and on and on and on and on and on and on and on and on and on and on 1234567890"
                          : i === 14 ? "😀😀😀" : "Note " + (i < 10 ? "0" : "") + i
                var cat = i % 11 === 0 ? "Home" : (i % 5 === 0 ? "Work" : "")
                var n = { localId: i, title: title, category: cat, favorite: i % 7 === 0, pending: i % 13 === 0, modified: 2000000 - i * 100 }
                if (view === "favorites" && !n.favorite) continue
                if (view === "uncategorized" && cat !== "") continue
                if (view.indexOf("category:") === 0 && cat !== view.substring(9)) continue
                if (searchText.length > 0 && title.toLowerCase().indexOf(searchText.toLowerCase()) < 0) continue
                out.push(n)
            }
            if (sortOrder === "title") out.sort(function(a, b) { return a.title < b.title ? -1 : a.title > b.title ? 1 : 0 })
            return out
        }
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
    property var about: QtObject {
        property string appName: "Notes Pocketbook"
        property string version: "1.10.0"
        property string author: "A. Author"
        property string copyright: "Copyright (c) 2026 A. Author"
        property string licenseName: "MIT License"
        property int unreadableEntries: 0
        property var components: [
            { id: "alpha", name: "Alpha", purpose: "A library", license: "MIT License", origin: "bundled" },
            { id: "beta", name: "Beta", purpose: "A font", license: "SIL OFL 1.1", origin: "device" },
            { id: "gamma", name: "Gamma", purpose: "A data file", license: "BSD", origin: "bundled" }
        ]
        function appLicense() {
            return ({ title: "MIT License", header: ["License: MIT License", "Holder: A. Author"], paragraphs: ["Permission is hereby granted.", "The software is provided as is."], available: true })
        }
        function componentDetail(id) {
            return ({ title: id, header: ["License: " + id], paragraphs: ["Text of " + id], available: true })
        }
    }
    property var device: QtObject {
        signal networkResult(bool connected)
        function wifiConnected() { return true }
        function connectNetwork() {}
        property int quitCalls: 0
        function quit() { quitCalls++ }
        property int opened: 0
        function openUrl(url) { opened++; return true }
    }
}
