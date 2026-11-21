import QtQuick
import SoundLink 1.0

// A single-line text field in the app's style.
Rectangle {
    id: root

    property alias text: input.text
    property alias validator: input.validator
    property string placeholder: ""
    readonly property bool inputFocused: input.activeFocus

    signal accepted
    signal textEdited

    function focusInput() {
        input.forceActiveFocus();
        input.selectAll();
    }

    implicitWidth: 200
    implicitHeight: 30
    radius: Theme.metrics.radiusMd
    color: Theme.palette.fieldBg
    border.width: 1
    border.color: input.activeFocus ? Theme.palette.borderStrong : Theme.palette.border

    Behavior on border.color { ColorAnimation { duration: Theme.motion.durationFast } }

    ContextMenu { id: textMenu }

    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        verticalAlignment: TextInput.AlignVCenter
        clip: true
        color: Theme.palette.textPrimary
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.bodySize
        selectionColor: Theme.palette.borderStrong
        selectByMouse: true
        // The menu takes the focus; the selection has to outlive that.
        persistentSelection: true

        onAccepted: root.accepted()
        onTextEdited: root.textEdited()

        Text {
            anchors.fill: parent
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            text: root.placeholder
            color: Theme.palette.textTertiary
            font: input.font
            visible: input.text.length === 0
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            cursorShape: Qt.IBeamCursor
            onClicked: (mouse) => {
                input.forceActiveFocus();
                textMenu.openAt(input.mapToItem(null, mouse.x, mouse.y), [
                    { text: "Cut", icon: "close", action: () => input.cut() },
                    { text: "Copy", icon: "link", action: () => input.copy() },
                    { text: "Paste", icon: "plus", action: () => input.paste() },
                    { separator: true },
                    { text: "Select all", icon: "select", action: () => input.selectAll() }
                ]);
            }
        }
    }
}
