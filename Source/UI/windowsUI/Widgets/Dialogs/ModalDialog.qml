import QtQuick
import QtQuick.Controls
import SoundLink 1.0

// Centered modal dialog: a title, the content (children) and buttons at the bottom right.
Popup {
    id: root

    property string title: ""
    property string acceptText: "OK"
    property bool acceptEnabled: true
    property bool danger: false

    default property alias content: body.children

    signal accepted

    function accept() {
        if (!acceptEnabled)
            return;
        close();
        accepted();
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: 440
    padding: 20
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.55) }

    background: Rectangle {
        color: Theme.palette.content
        radius: Theme.metrics.radiusXl
        border.width: 1
        border.color: Theme.palette.borderStrong
    }

    contentItem: Column {
        spacing: 16

        Text {
            width: parent.width
            elide: Text.ElideRight
            text: root.title
            color: Theme.palette.textPrimary
            font.family: Theme.typography.family
            font.pixelSize: 17
            font.weight: Font.Bold
        }

        Column {
            id: body
            width: parent.width
            spacing: 12
        }

        Row {
            anchors.right: parent.right
            spacing: 8

            TextButton {
                text: "Cancel"
                onClicked: root.close()
            }
            TextButton {
                text: root.acceptText
                primary: !root.danger
                enabled: root.acceptEnabled
                onClicked: root.accept()
            }
        }
    }
}
