import QtQuick

// One attachment inside a note (contracts/qml-bridge.md `attachment` block):
//   placeholder  name, size when known, "Download" (nothing is downloaded by itself)
//   ready image  the picture in place; a tap opens it full screen
//   ready file   name, size, "Open" when the reader application can show the type
//   pending      added on this reader, waiting for the next sync
//   error        the reason in a plain sentence; "Download" again where retrying can help
// "Download" is a deliberate request: the page switches Wi-Fi on first when it is off (like "Sync now").
// Boxes are plain black outlines, no animation (e-ink). The block is never cut by a page break.
Item {
    id: root
    property var block
    property int fontPx: 38
    signal downloadRequested()
    signal openRequested()
    signal imageTapped()
    property bool selected: false   // a whole unit of the reader's selection (spec 005): drawn with a thicker frame

    readonly property int pad: 14
    readonly property real textPx: Math.max(20, Math.round(fontPx * 0.8))
    property bool waiting: false  // the reader is switching Wi-Fi on for a download
    readonly property bool tooBig: block.size > attachments.maxBytes
    readonly property bool hasImage: block.imageSource !== "" && !(img.status === Image.Error)
    readonly property bool canRetry: block.remotePath !== "" && block.attState !== "ready" && block.attState !== "pending"
                                     && block.errorText !== "This server does not support attachments."
    readonly property string sizeText: block.size >= 0 ? bridge.formatSize(block.size) : ""

    implicitHeight: box.height + 10

    Rectangle {
        id: box
        width: root.width
        height: hasImage ? img.height + 2 * root.pad : info.implicitHeight + 2 * root.pad
        color: "#ffffff"
        border.width: root.selected ? 6 : 2
        border.color: "#000000"

        // the picture in place
        Image {
            id: img
            visible: root.hasImage
            x: root.pad; y: root.pad
            width: parent.width - 2 * root.pad
            // the provider already scales to this width, so the natural size gives the height (no binding loop)
            height: status === Image.Ready && implicitWidth > 0 ? Math.round(width * implicitHeight / implicitWidth) : Math.round(root.textPx * 4)
            source: root.block.imageSource !== "" ? root.block.imageSource + "?w=" + Math.round(width) : ""
            asynchronous: true
            cache: false
            fillMode: Image.PreserveAspectFit
            MouseArea { anchors.fill: parent; onClicked: root.imageTapped() }
        }

        Column {
            id: info
            visible: !root.hasImage
            x: root.pad; y: root.pad
            width: parent.width - 2 * root.pad
            spacing: 10

            Text {
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: root.textPx
                font.bold: true
                color: "#000000"
                text: (root.block.attKind === "image" ? qsTr("Picture: ") : qsTr("File: ")) + root.block.name
            }
            Text {
                visible: root.block.attState === "placeholder" && root.sizeText !== "" && !root.tooBig
                width: parent.width
                font.pixelSize: root.textPx
                color: "#000000"
                text: root.sizeText
            }
            Text {
                visible: root.block.attState === "placeholder" && root.tooBig
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: root.textPx
                color: "#000000"
                text: qsTr("%1 - larger than the %2 limit, it cannot be downloaded.")
                          .arg(root.sizeText).arg(bridge.formatSize(attachments.maxBytes))
            }
            Text {
                visible: root.block.attState === "ready"
                width: parent.width
                font.pixelSize: root.textPx
                color: "#000000"
                text: root.block.size >= 0 ? root.sizeText : ""
            }
            Text {
                visible: root.block.attState === "ready" && root.block.attKind === "image" && img.status === Image.Error
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: root.textPx
                color: "#000000"
                text: qsTr("This picture format cannot be shown.")
            }
            Text {
                visible: root.block.attState === "ready" && root.block.attKind === "file" && !root.block.viewable
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: root.textPx
                color: "#000000"
                text: qsTr("No viewer for this file on this device.")
            }
            Text {
                visible: root.block.attState === "pending"
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: root.textPx
                color: "#000000"
                text: qsTr("Waiting to be sent to the server.")
            }
            Text {
                visible: root.block.errorText !== ""
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: root.textPx
                color: "#000000"
                text: root.block.errorText
            }
            Row {
                spacing: 20
                ToolButton {
                    width: 220
                    visible: (root.block.attState === "placeholder" && !root.tooBig) || (root.block.attState === "error" && root.canRetry)
                    text: root.waiting ? qsTr("Connecting to Wi-Fi...") : qsTr("Download")
                    icon: "download"
                    enabled: !root.waiting
                    onClicked: root.downloadRequested()
                }
                ToolButton {
                    visible: root.block.attState === "ready" && root.block.attKind === "file" && root.block.viewable
                    text: qsTr("Open")
                    icon: "folder"
                    onClicked: root.openRequested()
                }
            }
        }
    }
}
