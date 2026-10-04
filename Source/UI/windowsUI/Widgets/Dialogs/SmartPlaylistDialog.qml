pragma ComponentBehavior: Bound

import QtQuick
import SoundLink 1.0

// Creates (openNew) or edits (openEdit) a smart playlist: rules, all/any, sort order and limit,
// with a live count of matching tracks.
ModalDialog {
    id: root

    // Empty while creating.
    property string playlistId: ""

    // [{field, op, value}]. Values change in place while typing (so the fields keep focus);
    // field and operator changes replace the array. revision drives the preview.
    property var rules: []
    property bool matchAll: true
    property string sort: "added"
    property int revision: 0

    readonly property var fields: PlaylistsViewModel.ruleFields()

    readonly property int preview: {
        revision;
        return PlaylistsViewModel.previewCount(definition());
    }

    function definition() {
        const limit = parseInt(limitField.text);
        return { rules: rules, matchAll: matchAll, sort: sort, limit: isNaN(limit) ? 0 : limit };
    }

    function defaultRule(field) {
        const type = PlaylistsViewModel.fieldType(field);
        const ops = PlaylistsViewModel.operators(type);
        const values = PlaylistsViewModel.valueOptions(type);
        return { field: field, op: ops[0].id, value: values.length > 0 ? values[0].id : (type === "date" ? "30" : "") };
    }

    function openNew() {
        playlistId = "";
        title = "New smart playlist";
        acceptText = "Create";
        nameField.text = "";
        rules = [defaultRule("artist")];
        matchAll = true;
        sort = "added";
        limitField.text = "";
        revision++;
        open();
    }

    function openEdit(id) {
        const current = PlaylistsViewModel.definition(id);
        playlistId = id;
        title = "Edit smart playlist";
        acceptText = "Save";
        nameField.text = PlaylistsViewModel.nameOf(id);
        rules = current.rules.map(rule => ({ field: rule.field, op: rule.op, value: rule.value }));
        matchAll = current.matchAll;
        sort = current.sort;
        limitField.text = current.limit > 0 ? String(current.limit) : "";
        revision++;
        open();
    }

    function changeField(index, field) {
        const copy = rules.slice();
        copy[index] = defaultRule(field);
        rules = copy;
        revision++;
    }

    function changeOp(index, op) {
        const copy = rules.slice();
        copy[index] = { field: copy[index].field, op: op, value: copy[index].value };
        rules = copy;
        revision++;
    }

    function changeValue(index, value, bReplace) {
        if (bReplace) {
            const copy = rules.slice();
            copy[index] = { field: copy[index].field, op: copy[index].op, value: value };
            rules = copy;
        } else {
            rules[index].value = value;
        }
        revision++;
    }

    function removeRule(index) {
        const copy = rules.slice();
        copy.splice(index, 1);
        rules = copy;
        revision++;
    }

    width: 640
    acceptEnabled: nameField.text.trim().length > 0

    onOpened: nameField.focusInput()
    onAccepted: {
        if (playlistId.length === 0) {
            const id = PlaylistsViewModel.createSmartPlaylist(nameField.text, definition());
            PlaylistsViewModel.open(id);
        } else {
            PlaylistsViewModel.rename(playlistId, nameField.text);
            PlaylistsViewModel.setDefinition(playlistId, definition());
        }
    }

    InputField {
        id: nameField
        width: parent.width
        placeholder: "Name"
    }

    Row {
        spacing: 8

        Label { text: "Tracks that match" }
        TabPill {
            anchors.verticalCenter: parent.verticalCenter
            text: "all rules"
            selected: root.matchAll
            onClicked: { root.matchAll = true; root.revision++; }
        }
        TabPill {
            anchors.verticalCenter: parent.verticalCenter
            text: "any rule"
            selected: !root.matchAll
            onClicked: { root.matchAll = false; root.revision++; }
        }
    }

    Column {
        width: parent.width
        spacing: 6

        Repeater {
            model: root.rules.length

            Row {
                id: ruleRow

                required property int index
                readonly property var rule: root.rules[index]
                readonly property string type: PlaylistsViewModel.fieldType(rule ? rule.field : "")
                readonly property var valueOptions: PlaylistsViewModel.valueOptions(type)

                spacing: 6

                Dropdown {
                    width: 140
                    options: root.fields
                    currentId: ruleRow.rule ? ruleRow.rule.field : ""
                    onActivated: (id) => root.changeField(ruleRow.index, id)
                }
                Dropdown {
                    width: 170
                    options: PlaylistsViewModel.operators(ruleRow.type)
                    currentId: ruleRow.rule ? ruleRow.rule.op : ""
                    onActivated: (id) => root.changeOp(ruleRow.index, id)
                }
                Dropdown {
                    visible: ruleRow.valueOptions.length > 0
                    width: 230
                    options: ruleRow.valueOptions
                    currentId: ruleRow.rule ? ruleRow.rule.value : ""
                    onActivated: (id) => root.changeValue(ruleRow.index, id, true)
                }
                InputField {
                    visible: ruleRow.valueOptions.length === 0
                    width: 230
                    placeholder: ruleRow.type === "text" ? "text" : "number"
                    text: ruleRow.rule ? ruleRow.rule.value : ""
                    onTextEdited: root.changeValue(ruleRow.index, text, false)
                }
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    implicitWidth: 28; implicitHeight: 28
                    iconName: "close"
                    iconSize: 11
                    onClicked: root.removeRule(ruleRow.index)
                }
            }
        }

        TextButton {
            text: "Add rule"
            onClicked: {
                root.rules = root.rules.concat([root.defaultRule("artist")]);
                root.revision++;
            }
        }
    }

    Row {
        spacing: 8

        Label { text: "Sort by" }
        Dropdown {
            width: 170
            options: PlaylistsViewModel.sortOrders()
            currentId: root.sort
            onActivated: (id) => { root.sort = id; root.revision++; }
        }
        Item { width: 12; height: 1 }
        Label { text: "Limit" }
        InputField {
            id: limitField
            width: 80
            placeholder: "none"
            validator: IntValidator { bottom: 0; top: 100000 }
            onTextEdited: root.revision++
        }
    }

    Text {
        width: parent.width
        wrapMode: Text.WordWrap
        text: (root.preview === 1 ? "1 track matches" : root.preview + " tracks match")
              + (root.rules.length === 0 ? " (no rules: the whole library)" : "")
        color: Theme.palette.textSecondary
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.smallSize
    }

    component Label: Text {
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.palette.textSecondary
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.bodySize
    }
}
