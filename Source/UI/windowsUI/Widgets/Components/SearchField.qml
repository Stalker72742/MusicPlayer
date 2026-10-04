import QtQuick
import QtQuick.Controls
import SoundLink 1.0

Rectangle {
    id: root

    property alias text: input.text
    property string placeholder: "Search..."
    property string shortcutHint: ""
    readonly property bool inputFocused: input.activeFocus

    signal accepted(string text)
    signal textEdited(string text)
    signal upPressed
    signal downPressed
    signal tabPressed
    signal escapePressed

    function focusInput() {
        input.forceActiveFocus();
        input.selectAll();
    }

    function clearFocus() {
        input.focus = false;
    }

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
        anchors.right: trailing.left
        anchors.leftMargin: 8
        anchors.rightMargin: 8

        clip: true
        color: Theme.palette.textPrimary
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.bodySize
        selectionColor: Theme.palette.borderStrong
        selectByMouse: true

        onAccepted: root.accepted(text)
        onTextEdited: root.textEdited(text)

        Keys.onUpPressed: root.upPressed()
        Keys.onDownPressed: root.downPressed()
        Keys.onTabPressed: root.tabPressed()
        Keys.onEscapePressed: root.escapePressed()

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.right: parent.right
            elide: Text.ElideRight
            text: root.placeholder
            color: Theme.palette.textTertiary
            font: input.font
            visible: input.text.length === 0 && !input.activeFocus
        }
    }

    // Clear button while there is text, the shortcut hint otherwise.
    Item {
        id: trailing
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        width: input.text.length > 0 ? 18 : hint.implicitWidth
        height: 18

        Text {
            id: hint
            anchors.centerIn: parent
            visible: input.text.length === 0 && root.shortcutHint.length > 0
            text: root.shortcutHint
            color: Theme.palette.textMuted
            font.family: Theme.typography.family
            font.pixelSize: Theme.typography.tinySize
        }

        IconButton {
            anchors.fill: parent
            visible: input.text.length > 0
            iconName: "close"
            iconSize: 10
            cornerRadius: Theme.metrics.radiusSm
            onClicked: {
                input.clear();
                root.textEdited("");
                input.forceActiveFocus();
            }
        }
    }
}
