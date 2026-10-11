import QtQuick
import QtQuick.Controls
import QtQuick.Window

// The single window. The system rotates (device.orientation), Qt resizes the window; this stage only
// compensates the panel's devicePixelRatio so that sizes authored in panel pixels are 1:1.
// No page sets a font family: every text inherits the application font, which carries the emoji fallback
// (app/main.cpp).
ApplicationWindow {
    id: win
    visible: true
    visibility: Window.FullScreen
    color: "#ffffff"

    // True once a page is shown (checked by the UI self-test: a blank screen must never go unnoticed).
    readonly property bool pageLoaded: nav.depth > 0

    Item {
        id: stage
        readonly property real dpr: Screen.devicePixelRatio
        width: win.width * dpr
        height: win.height * dpr
        anchors.centerIn: parent
        scale: 1 / dpr

        StackView {
            id: nav
            anchors.fill: parent
            // initialItem with a binding silently showed nothing (measured on the desktop and the reader):
            // push the first page explicitly. `startPage` is a desktop-only override for screenshots.
            Component.onCompleted: push(Qt.resolvedUrl(startPage.length > 0 ? startPage
                                          : (bridge.configured ? "pages/NoteListPage.qml" : "pages/SetupPage.qml")),
                                        startNote > 0 ? { noteId: startNote } : {})
            // E-ink: no transitions.
            pushEnter: Transition {}
            pushExit: Transition {}
            popEnter: Transition {}
            popExit: Transition {}
            replaceEnter: Transition {}
            replaceExit: Transition {}
        }

        Connections {
            target: device
            function onBackRequested() {
                // a page with an open popup (the editor's "/" menu) takes Back for itself (spec 006 FR-009)
                var page = nav.currentItem
                if (page && page.backHandled && page.backHandled()) return
                if (nav.depth > 1) {
                    Qt.inputMethod.hide()
                    nav.pop()
                }
            }
        }

        // Told once after a sync that kept both versions of a note, or brought back a note that was
        // deleted here but changed on the server (nothing is ever overwritten or deleted silently).
        ConfirmDialog {
            id: conflictDialog
            title: qsTr("Notes changed in two places")
            acceptText: qsTr("OK")
            showCancel: false
            Connections {
                target: bridge
                function onConflictsDetected(conflicts, restored) {
                    var lines = []
                    if (conflicts > 0)
                        lines.push(qsTr("%n note(s) were changed on the reader and on the server. Both versions are kept: the server's version replaced the note, and your text is saved as a new note with \"(conflict ...)\" in its title.", "", conflicts))
                    if (restored > 0)
                        lines.push(qsTr("%n note(s) deleted here were changed on the server, so they were not deleted and are back in the list.", "", restored))
                    conflictDialog.text = lines.join("\n\n")
                    conflictDialog.open()
                }
            }
        }

        // Setting up or forgetting the account switches between the setup screen and the home page.
        Connections {
            target: bridge
            function onAccountChanged() {
                Qt.inputMethod.hide()
                nav.clear()
                nav.push(Qt.resolvedUrl(bridge.configured ? "pages/NoteListPage.qml" : "pages/SetupPage.qml"))
            }
        }
    }
}
