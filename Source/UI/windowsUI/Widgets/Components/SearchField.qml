import QtQuick
import QtQuick.Controls
import SoundLink 1.0

Rectangle {
    id: root

    property alias text: input.text
    property string placeholder: "Search..."

    signal accepted(string text)

    implicitWidth: 480
    implicitHeight: 30
    radius: Theme.metrics.radiusMd
    color: Theme.palette.fieldBg
    border.color: input.activeFocus ? Theme.palette.borderStrong : Theme.palette.border
    border.width: 1

    Behavior on border.color { ColorAnimation { duration: Theme.motion.durationFast } }

    AppIcon {
        id: searchIcon
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 10
        width: 14
        height: 14
        name: "search"
        color: Theme.palette.textSecondary
    }

    TextInput {
        id: input
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: searchIcon.right
        anchors.right: parent.right
        anchors.leftMargin: 8
        anchors.rightMargin: 10

        clip: true
        color: Theme.palette.textPrimary
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.bodySize
        selectionColor: Theme.palette.borderStrong
        selectByMouse: true

        onAccepted: root.accepted(text)

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            text: root.placeholder
            color: Theme.palette.textTertiary
            font: input.font
            visible: input.text.length === 0 && !input.activeFocus
        }
    }
}
