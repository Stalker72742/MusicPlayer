pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SoundLink 1.0

// A window of its own for the tags, file name and playlists of one or many tracks. Several tracks get a tab
// "All tracks", where a change goes to every track, and a tab per track. Nothing is applied until Save.
// Every track has a draft; the All tab reads and writes the drafts of all of them.
Window {
    id: root

    property var originals: []
    property var drafts: []
    property var choices: []
    property var tabs: []
    // -1: the All tab, otherwise a row of drafts.
    property int page: 0
    // Fields read their text when this changes, so that typing is not overwritten.
    property int revision: 0
    property int checkRevision: 0
    property var mixedOnLoad: ({})
    property string errorText: ""

    readonly property var current: {
        revision;
        return page >= 0 && page < drafts.length ? drafts[page] : null;
    }

    function openFor(ids) {
        errorText = "";
        load(ids);
        show();
        raise();
        requestActivate();
    }

    function load(ids) {
        const loaded = [];
        for (const id of ids) {
            const track = LibraryViewModel.trackProperties(id);
            if (track.id === 0)
                continue;
            track.playlists = {};
            for (const choice of PlaylistsViewModel.staticPlaylistsFor(id))
                track.playlists[choice.id] = choice.contains;
            loaded.push(track);
        }

        originals = loaded;
        drafts = JSON.parse(JSON.stringify(loaded));
        choices = ids.length > 0 ? PlaylistsViewModel.staticPlaylistsFor(ids[0]) : [];

        const list = loaded.length > 1 ? [{ page: -1, title: "All " + loaded.length + " tracks", artist: "Changes every track" }] : [];
        loaded.forEach((track, row) => list.push({ page: row, title: track.title, artist: track.artist }));
        tabs = list;
        showPage(loaded.length > 1 ? -1 : 0);
    }

    function showPage(next) {
        page = next;
        if (next === -1) {
            const mixed = {};
            for (const key of ["title", "artist", "album", "genre", "year", "track", "tags"])
                mixed[key] = !allSame(key);
            mixedOnLoad = mixed;
        }
        revision++;
        form.contentY = 0;
    }

    function allSame(key) {
        return drafts.every((draft) => draft[key] === drafts[0][key]);
    }

    function splitTags(text) {
        return text.split(",").map((tag) => tag.trim()).filter((tag) => tag.length > 0);
    }

    function commonTags() {
        if (drafts.length === 0)
            return [];
        return splitTags(drafts[0].tags).filter((tag) => drafts.every((draft) => splitTags(draft.tags).includes(tag)));
    }

    function fieldText(key) {
        if (drafts.length === 0)
            return "";
        if (page >= 0)
            return drafts[page][key] || "";
        if (key === "tags")
            return commonTags().join(", ");
        return allSame(key) ? drafts[0][key] : "";
    }

    function setField(key, text) {
        if (page >= 0) {
            drafts[page][key] = text;
            return;
        }
        if (key === "tags") {
            // Only what is common to all is shown, so only that can be taken away; other tags stay.
            const before = commonTags();
            const after = splitTags(text);
            for (const draft of drafts) {
                const kept = splitTags(draft.tags).filter((tag) => !before.includes(tag) || after.includes(tag));
                for (const tag of after) {
                    if (!kept.includes(tag))
                        kept.push(tag);
                }
                draft.tags = kept.join(", ");
            }
            return;
        }
        // Clearing a field with different values means "leave them", not "erase them all".
        drafts.forEach((draft, row) => draft[key] = text.length === 0 && mixedOnLoad[key] ? originals[row][key] : text);
    }

    function playlistState(id) {
        const list = page >= 0 ? (page < drafts.length ? [drafts[page]] : []) : drafts;
        const count = list.filter((draft) => draft.playlists[id]).length;
        return count === 0 ? 0 : count === list.length ? 2 : 1;
    }

    function togglePlaylist(id) {
        const value = playlistState(id) !== 2;
        for (const draft of page >= 0 ? [drafts[page]] : drafts)
            draft.playlists[id] = value;
        checkRevision++;
    }

    function save() {
        const problems = [];
        drafts.forEach((draft, row) => {
            const original = originals[row];

            if (draft.hasFile && draft.fileName !== original.fileName) {
                const problem = LibraryViewModel.renameFile(draft.id, draft.fileName);
                if (problem.length > 0)
                    problems.push(original.fileName + ": " + problem);
            }

            const tags = {};
            for (const key of ["title", "artist", "album", "genre", "year", "track"]) {
                if (draft[key] !== original[key])
                    tags[key] = draft[key];
            }
            if (Object.keys(tags).length > 0)
                LibraryViewModel.setTrackTags(draft.id, tags);

            if (draft.tags !== original.tags)
                LibraryViewModel.setLabelsText(draft.id, draft.tags);
        });

        for (const choice of choices) {
            const added = drafts.filter((draft, row) => draft.playlists[choice.id] && !originals[row].playlists[choice.id]);
            const removed = drafts.filter((draft, row) => !draft.playlists[choice.id] && originals[row].playlists[choice.id]);
            if (added.length > 0)
                PlaylistsViewModel.addTracks(choice.id, added.map((draft) => draft.id));
            if (removed.length > 0)
                PlaylistsViewModel.removeTracks(choice.id, removed.map((draft) => draft.id));
        }

        if (problems.length === 0) {
            close();
            return;
        }
        load(originals.map((track) => track.id));
        errorText = problems.join("\n");
    }

    title: originals.length > 1 ? "Properties of " + originals.length + " tracks" : "Track properties"
    width: 780
    height: 640
    minimumWidth: 520
    minimumHeight: 420
    color: Theme.palette.content
    flags: Qt.Window | Qt.FramelessWindowHint

    Shortcut {
        sequence: "Esc"
        onActivated: root.close()
    }

    IntValidator { id: numberValidator; bottom: 0; top: 9999 }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ─── Title bar ─────────────────────────────────
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 44

            MouseArea {
                anchors.fill: parent
                onPressed: root.startSystemMove()
            }
            Text {
                anchors.left: parent.left
                anchors.right: closeButton.left
                anchors.leftMargin: 20
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                elide: Text.ElideRight
                text: root.title
                color: Theme.palette.textPrimary
                font.family: Theme.typography.family
                font.pixelSize: 15
                font.weight: Font.Bold
            }
            IconButton {
                id: closeButton
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                implicitWidth: 30; implicitHeight: 30
                iconName: "close"
                iconSize: 12
                onClicked: root.close()
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.palette.hairline }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ─── Tabs: all tracks, then each one ───────
            ListView {
                id: tabList
                visible: root.originals.length > 1
                Layout.preferredWidth: 220
                Layout.fillHeight: true
                Layout.margins: 8
                clip: true
                spacing: 2
                boundsBehavior: Flickable.StopAtBounds
                model: root.tabs
                ScrollBar.vertical: AppScrollBar {}

                delegate: Rectangle {
                    id: tab

                    required property var modelData
                    readonly property bool active: root.page === modelData.page

                    width: tabList.width - 12
                    height: 44
                    radius: Theme.metrics.radiusMd
                    color: active ? Theme.palette.selectedOverlay : tabHover.hovered ? Theme.palette.hoverOverlay : "transparent"

                    Column {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 1

                        Text {
                            width: parent.width
                            elide: Text.ElideRight
                            text: tab.modelData.title
                            color: Theme.palette.textPrimary
                            font.family: Theme.typography.family
                            font.pixelSize: Theme.typography.bodySize
                            font.weight: tab.active ? Font.DemiBold : Font.Normal
                        }
                        Text {
                            width: parent.width
                            elide: Text.ElideRight
                            text: tab.modelData.artist
                            color: Theme.palette.textSecondary
                            font.family: Theme.typography.family
                            font.pixelSize: Theme.typography.smallSize
                        }
                    }

                    HoverHandler { id: tabHover; cursorShape: Qt.PointingHandCursor }
                    MouseArea { anchors.fill: parent; onClicked: root.showPage(tab.modelData.page) }
                }
            }

            Rectangle {
                visible: tabList.visible
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                color: Theme.palette.hairline
            }

            // ─── Fields of the open tab ────────────────
            Flickable {
                id: form
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: width
                contentHeight: fields.implicitHeight + 40
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: AppScrollBar {}

                Column {
                    id: fields
                    x: 20
                    y: 20
                    width: form.width - 44
                    spacing: 14

                    // What is being edited, with what cannot be edited.
                    Column {
                        visible: root.current !== null
                        width: parent.width
                        spacing: 2

                        Text {
                            width: parent.width
                            elide: Text.ElideMiddle
                            text: root.current ? root.current.path : ""
                            color: Theme.palette.textSecondary
                            font.family: Theme.typography.family
                            font.pixelSize: Theme.typography.smallSize
                        }
                        Text {
                            text: root.current ? "Duration " + root.current.duration : ""
                            color: Theme.palette.textTertiary
                            font.family: Theme.typography.family
                            font.pixelSize: Theme.typography.smallSize
                        }
                    }

                    Field { width: parent.width; key: "title"; label: "Title" }
                    Field { width: parent.width; key: "artist"; label: "Artist" }
                    Field { width: parent.width; key: "album"; label: "Album" }
                    Field { width: parent.width; key: "genre"; label: "Genre" }

                    Row {
                        width: parent.width
                        spacing: 12

                        Field { width: (parent.width - 12) / 2; key: "year"; label: "Year"; numeric: true }
                        Field { width: (parent.width - 12) / 2; key: "track"; label: "Track number"; numeric: true }
                    }

                    Field {
                        width: parent.width
                        key: "tags"
                        label: "Tags"
                        hint: root.page === -1 ? "Comma-separated. Tags only some of the tracks have are not shown and stay as they are."
                                               : "Comma-separated. Smart playlists can pick tracks by tag."
                    }
                    Field {
                        visible: root.current !== null && root.current.hasFile
                        width: parent.width
                        key: "fileName"
                        label: "File name"
                        hint: "Without the extension. The title follows a new file name unless you change the title too."
                    }

                    Column {
                        width: parent.width
                        spacing: 6

                        FieldLabel { text: "Playlists" }

                        Text {
                            visible: root.choices.length === 0
                            text: "There are no ordinary playlists yet."
                            color: Theme.palette.textSecondary
                            font.family: Theme.typography.family
                            font.pixelSize: Theme.typography.smallSize
                        }

                        Repeater {
                            model: root.choices

                            Rectangle {
                                id: playlistRow

                                required property var modelData
                                // 0 no track, 1 some of the tracks, 2 all of them.
                                readonly property int checkState: {
                                    root.revision;
                                    root.checkRevision;
                                    return root.playlistState(modelData.id);
                                }

                                width: parent.width
                                height: 32
                                radius: Theme.metrics.radiusMd
                                color: rowHover.hovered ? Theme.palette.hoverOverlay : "transparent"

                                Rectangle {
                                    id: box
                                    anchors.left: parent.left
                                    anchors.leftMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 16; height: 16
                                    radius: Theme.metrics.radiusSm
                                    color: playlistRow.checkState > 0 ? Theme.palette.pillSelected : "transparent"
                                    border.width: 1
                                    border.color: Theme.palette.borderStrong

                                    AppIcon {
                                        anchors.centerIn: parent
                                        visible: playlistRow.checkState === 2
                                        width: 12; height: 12
                                        name: "check"
                                        color: Theme.palette.iconActive
                                    }
                                    Rectangle {
                                        anchors.centerIn: parent
                                        visible: playlistRow.checkState === 1
                                        width: 8; height: 2
                                        color: Theme.palette.iconActive
                                    }
                                }
                                Text {
                                    anchors.left: box.right
                                    anchors.leftMargin: 10
                                    anchors.right: parent.right
                                    anchors.rightMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    elide: Text.ElideRight
                                    text: playlistRow.modelData.name
                                    color: Theme.palette.textPrimary
                                    font.family: Theme.typography.family
                                    font.pixelSize: Theme.typography.bodySize
                                }

                                HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
                                MouseArea { anchors.fill: parent; onClicked: root.togglePlaylist(playlistRow.modelData.id) }
                            }
                        }
                    }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.palette.hairline }

        // ─── Buttons ───────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 12
            Layout.leftMargin: 20
            spacing: 8

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                maximumLineCount: 3
                elide: Text.ElideRight
                text: root.errorText
                color: Theme.palette.danger
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.smallSize
            }
            TextButton {
                text: "Cancel"
                onClicked: root.close()
            }
            TextButton {
                primary: true
                text: "Save"
                onClicked: root.save()
            }
        }
    }

    WindowResizeFrame {
        anchors.fill: parent
        window: root
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: 1
        border.color: Theme.palette.borderStrong
    }

    component FieldLabel: Text {
        color: Theme.palette.textSecondary
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.smallSize
        font.weight: Font.DemiBold
    }

    component Field: Column {
        id: fieldRoot

        property string key: ""
        property string label: ""
        property string hint: ""
        property bool numeric: false

        spacing: 6

        FieldLabel { text: fieldRoot.label }

        InputField {
            width: parent.width
            validator: fieldRoot.numeric ? numberValidator : null
            text: {
                root.revision;
                return root.fieldText(fieldRoot.key);
            }
            placeholder: {
                root.revision;
                return root.page === -1 && root.mixedOnLoad[fieldRoot.key] === true && fieldRoot.key !== "tags" ? "Multiple values" : "";
            }
            onTextEdited: root.setField(fieldRoot.key, text)
        }

        Text {
            visible: fieldRoot.hint.length > 0
            width: parent.width
            wrapMode: Text.WordWrap
            text: fieldRoot.hint
            color: Theme.palette.textTertiary
            font.family: Theme.typography.family
            font.pixelSize: Theme.typography.smallSize
        }
    }
}
