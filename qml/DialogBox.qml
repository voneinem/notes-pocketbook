import QtQuick

// The box of every dialog (spec 010), in the look of the reader's own dialogs: a heavy rounded outline, a bold title with a
// close icon over a rule, the content, and a footer of full-width upper-case cells split by a rule, the dismissing cell
// first. `footer` is [{text, role}] with the role "dismiss" or "accept"; `footerClicked(role)` reports a tap. Put the content
// inside it. The owner sets `width` and places it (DialogFrame centres it over a dimmed page; the pickers place it
// themselves). Nothing animates (e-ink).
Rectangle {
    id: root
    objectName: "dialogBox"
    property string title
    property var footer: []
    default property alias contents: body.data
    signal closeRequested()
    signal footerClicked(string role)

    height: 2 * Style.heavyWidth + header.height + body.height + 2 * Style.dialogPadding + (footer.length > 0 ? Style.footerHeight + Style.hairlineWidth : 0)
    color: Style.paper
    radius: Style.radiusDialog
    border.width: Style.heavyWidth
    border.color: Style.ink

    Item {
        id: header
        x: Style.heavyWidth
        y: Style.heavyWidth
        width: parent.width - 2 * Style.heavyWidth
        height: Style.touchMin + Style.ruleWidth
        Text {
            anchors { left: parent.left; leftMargin: Style.dialogPadding - Style.heavyWidth; right: closeButton.left; rightMargin: 12; verticalCenter: parent.verticalCenter; verticalCenterOffset: -1 }
            text: root.title
            elide: Text.ElideRight
            font.pixelSize: Style.rowTitleSize
            font.bold: true
            color: Style.ink
        }
        IconButton {
            id: closeButton
            anchors { right: parent.right; rightMargin: 6; top: parent.top }
            text: qsTr("Close")
            icon: "close"
            iconSize: Style.iconTool
            onClicked: root.closeRequested()
        }
        Rectangle {
            objectName: "dialogRule"
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right; leftMargin: Style.dialogPadding - Style.heavyWidth; rightMargin: Style.dialogPadding - Style.heavyWidth }
            height: Style.hairlineWidth
            color: Style.ink
        }
    }

    Column {
        id: body
        x: Style.dialogPadding
        y: header.y + header.height + Style.dialogPadding
        width: parent.width - 2 * Style.dialogPadding
        spacing: 24
    }

    Item {
        id: footerRow
        visible: root.footer.length > 0
        x: Style.heavyWidth
        y: parent.height - Style.heavyWidth - height
        width: parent.width - 2 * Style.heavyWidth
        height: Style.footerHeight + Style.hairlineWidth
        Rectangle { objectName: "footerTopRule"; width: parent.width; height: Style.hairlineWidth; color: Style.ink }
        Row {
            y: Style.hairlineWidth
            Repeater {
                model: root.footer
                delegate: Item {
                    width: footerRow.width / Math.max(1, root.footer.length)
                    height: Style.footerHeight
                    FooterButton {
                        anchors.fill: parent
                        text: modelData.text
                        onClicked: root.footerClicked(modelData.role)
                    }
                    Rectangle {
                        objectName: "footerRule"
                        visible: index > 0
                        width: Style.hairlineWidth
                        height: parent.height
                        color: Style.ink
                    }
                }
            }
        }
    }
}
