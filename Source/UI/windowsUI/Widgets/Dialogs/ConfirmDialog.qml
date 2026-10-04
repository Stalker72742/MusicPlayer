import QtQuick
import SoundLink 1.0

// Asks to confirm: ask(title, text, acceptText, onDone).
ModalDialog {
    id: root

    property string text: ""
    property var onDone: null

    function ask(dialogTitle, message, buttonText, callback) {
        title = dialogTitle;
        text = message;
        acceptText = buttonText;
        onDone = callback;
        open();
    }

    danger: true
    onAccepted: if (root.onDone) root.onDone()

    Text {
        width: parent.width
        wrapMode: Text.WordWrap
        text: root.text
        color: Theme.palette.textSecondary
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.bodySize
    }
}
