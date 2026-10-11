import QtQuick
import QtQuick.Controls
import ".."

// One attachment picture fitted to the whole window. No zoom, no pan, no animation (e-ink). Back returns to
// the note at the same position (the reader page below keeps its scroll position).
Item {
    id: page
    property int attachmentId: 0

    Rectangle { anchors.fill: parent; color: "#ffffff" }

    Image {
        id: picture
        anchors.fill: parent
        source: page.attachmentId > 0 ? "image://attachment/" + page.attachmentId + "?w=" + Math.round(width) : ""
        asynchronous: true
        cache: false
        fillMode: Image.PreserveAspectFit
    }

    Text {
        visible: picture.status === Image.Error
        anchors.centerIn: parent
        width: parent.width - 120
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        font.pixelSize: 32
        color: "#000000"
        text: qsTr("This picture cannot be shown.")
    }

    InkButton {
        x: 24; y: 24
        text: qsTr("Back")
        onClicked: page.StackView.view.pop()
    }
}
