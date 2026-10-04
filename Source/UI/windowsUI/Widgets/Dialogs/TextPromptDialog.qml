import QtQuick
import SoundLink 1.0

// Asks for one line of text: ask(title, text, onDone) calls onDone(text) on OK.
ModalDialog {
    id: root

    property string hint: ""
    property string placeholder: ""
    // E.g. tags, where an empty line clears them.
    property bool allowEmpty: false
    property var onDone: null

    function ask(dialogTitle, initialText, callback) {
        title = dialogTitle;
        field.text = initialText || "";
        onDone = callback;
        open();
    }

    acceptEnabled: root.allowEmpty || field.text.trim().length > 0
    onOpened: field.focusInput()
    onAccepted: if (root.onDone) root.onDone(field.text)

    Text {
        width: parent.width
        visible: root.hint.length > 0
        wrapMode: Text.WordWrap
        text: root.hint
        color: Theme.palette.textSecondary
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.smallSize
    }

    InputField {
        id: field
        width: parent.width
        placeholder: root.placeholder
        onAccepted: root.accept()
    }
}
