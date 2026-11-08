pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SoundLink 1.0

// Track list screen: header, optional view tabs and the list, or an empty state.
Item {
    id: root

    property string title: ""
    property string subtitle: ""
    property TrackListModel model: null
    property bool showTabs: false
    property var tabs: [
        { id: "all", title: "All" },
        { id: "albums", title: "Albums" },
        { id: "artists", title: "Artists" },
        { id: "genres", title: "Genres" }
    ]
    // What the tabs other than "all" show instead of "Coming soon".
    property Component tabContent: null
    property string emptyIcon: "library"
    property string emptyTitle: ""
    property string emptyText: ""
    property string emptyActionText: ""

    // Search results: a click plays just that track; library lists queue the whole list on double click.
    property bool singleTrackPlayback: false
    // Playlists: a single click queues the whole list.
    property bool clickToPlay: false

    property string albumHeader: "Album"
    property string dateHeader: "Date Added"

    // Endless lists: endReached() when scrolled to the bottom (or when everything fits), a footer while loading.
    property bool loadingMore: false

    // Selection: Ctrl+click toggles a row, Shift+click selects a range, the Select button turns clicks into
    // selecting. While anything is selected a plain click selects too, and Esc clears.
    property bool selectionMode: false
    property var selectedIds: ({})
    readonly property int selectedCount: Object.keys(selectedIds).length
    readonly property bool selecting: selectionMode || selectedCount > 0
    // The row Shift+click counts from.
    property int selectionAnchor: -1
    // The bar of actions on the selected tracks; screens with their own (OnlinePageScreen) turn it off.
    property bool bulkActions: true
    // Bumped after a bulk action, so the bar reads the tracks again.
    property int selectionRevision: 0

    function toggleSelected(trackId) {
        const next = Object.assign({}, selectedIds);
        if (next[trackId])
            delete next[trackId];
        else
            next[trackId] = true;
        selectedIds = next;
    }

    function selectRange(from, to) {
        const next = Object.assign({}, selectedIds);
        for (const id of root.model.idsBetween(from, to))
            next[id] = true;
        selectedIds = next;
    }

    function selectAll() {
        const next = {};
        for (const id of root.model.allIds())
            next[id] = true;
        selectedIds = next;
    }

    function clearSelection() {
        selectionMode = false;
        selectedIds = {};
        selectionAnchor = -1;
    }

    function selectedList() {
        return Object.keys(selectedIds).map(Number);
    }

    function plural(count, one, many) {
        return count + " " + (count === 1 ? one : many);
    }

    function removeText(info) {
        const parts = [];
        if (info.files > 0)
            parts.push(plural(info.files, "file goes", "files go") + " to the Recycle Bin and leave the library");
        if (info.downloads > 0)
            parts.push(plural(info.downloads, "downloaded copy is", "downloaded copies are") + " deleted, the tracks stream from YouTube again");
        if (info.saved > 0)
            parts.push(plural(info.saved, "online track leaves", "online tracks leave") + " the library and every playlist");
        return parts.join(";\n") + ".";
    }

    // info: LibraryViewModel.describeTracks() of the selection.
    function askRemoveSelected(info) {
        const ids = selectedList();
        const removable = info.files + info.downloads + info.saved;
        removeDialog.ask((info.files > 0 ? "Delete " : "Remove ") + plural(removable, "track", "tracks"),
            removeText(info), info.files > 0 ? "Delete" : "Remove", () => {
                const result = LibraryViewModel.removeTracks(ids);
                clearSelection();
                if (result.failed > 0)
                    removeFailedDialog.ask("Could not delete " + plural(result.failed, "file", "files"),
                        result.error + (result.failed > 1 ? "\nand " + (result.failed - 1) + " more." : ""),
                        "OK", () => {});
            });
    }

    // The row the track menu was opened for; Select counts ranges from it.
    property int menuRow: -1

    // A right click on one of several selected rows is about all of them.
    function showRowMenu(point, row, trackId) {
        menuRow = row;
        if (selectedIds[trackId] === true && selectedCount > 1)
            openSelectionMenu(point);
        else
            trackMenu.openAt(point, trackId);
    }

    function openSelectionMenu(point) {
        const ids = selectedList();
        const info = LibraryViewModel.describeTracks(ids);
        const removable = info.files + info.downloads + info.saved;
        const inPlaylist = contextPlaylistId.length > 0;
        selectionMenu.openAt(point, [
            { text: "Properties of " + plural(ids.length, "track", "tracks") + "…", icon: "tag",
              action: () => LibraryViewModel.openProperties(ids) },
            { text: "Add to playlist", icon: "list", action: () => trackMenu.openPlaylistsAt(point, ids) },
            { text: info.liked ? "Remove from favorites" : "Add to favorites", icon: info.liked ? "heart-filled" : "heart",
              action: () => { LibraryViewModel.toggleLikedTracks(ids); selectionRevision++; } },
            info.downloadable > 0
                ? { text: "Download " + info.downloadable, icon: "download",
                    action: () => { DownloadsViewModel.downloadTracks(ids); selectionRevision++; } }
                : null,
            inPlaylist
                ? { text: contextSmart ? "Hide from this playlist" : "Remove from this playlist", icon: "close",
                    action: () => { PlaylistsViewModel.removeTracks(contextPlaylistId, ids); clearSelection(); } }
                : null,
            removable > 0 ? { text: info.files > 0 ? "Delete…" : "Remove…", icon: "trash", action: () => askRemoveSelected(info) } : null,
            { separator: true },
            { text: "Select all", icon: "select", action: () => selectAll() },
            { text: "Clear selection", icon: "close", action: () => clearSelection() }
        ]);
    }

    // Returns true when the click was taken by selecting.
    function clickForSelection(row, trackId, modifiers) {
        if ((modifiers & Qt.ShiftModifier) && selectionAnchor >= 0) {
            selectRange(selectionAnchor, row);
            return true;
        }
        if ((modifiers & Qt.ControlModifier) || (modifiers & Qt.ShiftModifier) || selecting) {
            toggleSelected(trackId);
            selectionAnchor = row;
            return true;
        }
        return false;
    }

    onModelChanged: clearSelection()
    onContextPlaylistIdChanged: clearSelection()

    // Navigation keeps this with the location while the screen is gone; see WindowsMainWindow.
    function saveState() {
        return { scroll: list.contentY - list.originY, tab: currentTab };
    }

    function restoreState(state) {
        if (state.tab)
            currentTab = state.tab;
        const y = state.scroll || 0;
        if (y > 0)
            Qt.callLater(() => list.contentY = list.originY
                + Math.min(y, Math.max(0, list.count * Theme.metrics.rowHeight - list.height)));
    }

    Component.onCompleted: restoreState(Navigation.pageState)

    Shortcut {
        sequence: "Esc"
        enabled: root.visible && root.selecting
        onActivated: root.clearSelection()
    }

    // Buttons under the header, e.g. a playlist's Play and Edit.
    property Component headerActions: null
    // A picture left of the title (an artist's photo, an album cover).
    property Component headerArt: null

    // Rows can be dragged by their grip; moveRequested(from, to) asks the owner to reorder.
    property bool reorderable: false
    signal moveRequested(int from, int to)

    // Shown inside a playlist: the track menu can take tracks out of it.
    property string contextPlaylistId: ""
    property bool contextSmart: false

    signal emptyActionClicked
    signal endReached

    property string currentTab: "all"

    // header column metrics shared with rows
    readonly property real indexCol:    60
    readonly property real albumCol:    280
    readonly property real dateCol:     160
    readonly property real durationCol: 100
    readonly property real actionsCol:  90

    readonly property bool showList: currentTab === "all" && root.model !== null && root.model.count > 0

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 56
        anchors.rightMargin: 32
        anchors.topMargin: 32
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 24

            Loader {
                active: root.headerArt !== null
                visible: active
                sourceComponent: root.headerArt
            }

            ScreenHeader {
                Layout.fillWidth: true
                Layout.alignment: root.headerArt !== null ? Qt.AlignBottom : Qt.AlignTop
                title: root.title
                subtitle: root.subtitle
            }
        }

        Loader {
            active: root.headerActions !== null
            visible: active
            Layout.fillWidth: true
            sourceComponent: root.headerActions
        }

        Loader {
            active: root.bulkActions && root.selectedCount > 0
            visible: active
            Layout.fillWidth: true
            sourceComponent: selectionBar
        }

        Row {
            visible: root.showTabs
            spacing: 4
            Layout.topMargin: 4

            Repeater {
                model: root.tabs

                TabPill {
                    required property var modelData

                    text: modelData.title
                    selected: root.currentTab === modelData.id
                    onClicked: root.currentTab = modelData.id
                }
            }
        }

        // Column headers
        Item {
            visible: root.showList
            Layout.fillWidth: true
            Layout.rightMargin: 12
            Layout.topMargin: 16
            Layout.preferredHeight: 30

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.palette.hairline
                opacity: 0.6
            }

            ColumnHeader {
                width: root.indexCol
                anchors.left: parent.left
                horizontalAlignment: Text.AlignHCenter
                text: "#"
            }
            ColumnHeader {
                anchors.left: parent.left
                anchors.leftMargin: root.indexCol
                text: "Title"
            }
            ColumnHeader {
                width: root.albumCol
                anchors.right: dateHdr.left
                anchors.rightMargin: 16
                text: root.albumHeader
            }
            ColumnHeader {
                id: dateHdr
                width: root.dateCol
                anchors.right: durHdr.left
                anchors.rightMargin: 16
                text: root.dateHeader
            }
            ColumnHeader {
                id: durHdr
                width: root.durationCol
                anchors.right: parent.right
                anchors.rightMargin: root.actionsCol + 16
                text: "Duration"
            }

            IconButton {
                visible: root.bulkActions
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                implicitWidth: 26; implicitHeight: 26
                iconName: "select"
                iconSize: 15
                active: root.selecting
                onClicked: root.selecting ? root.clearSelection() : root.selectionMode = true
            }
        }

        ListView {
            id: list
            visible: root.showList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.model
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: AppScrollBar {}

            // Drag state: the row taken, the gap it would go to (0..count) and the pointer for auto-scroll.
            property int dragFrom: -1
            property int dropIndex: -1
            property real dragY: 0

            function updateDrop(scenePosition) {
                const local = list.mapFromItem(null, scenePosition.x, scenePosition.y);
                dragY = local.y;
                const contentY = local.y + list.contentY - list.originY;
                dropIndex = Math.max(0, Math.min(count, Math.round(contentY / Theme.metrics.rowHeight)));
            }

            function finishDrag() {
                const from = dragFrom;
                const to = dropIndex > from ? dropIndex - 1 : dropIndex;
                dragFrom = -1;
                dropIndex = -1;
                autoScroll.stop();
                if (from >= 0 && to >= 0 && to !== from)
                    root.moveRequested(from, to);
            }

            // Near the edges the list scrolls by itself while dragging.
            Timer {
                id: autoScroll
                interval: 16
                repeat: true
                onTriggered: {
                    const edge = 48;
                    const step = list.dragY < edge ? -(edge - list.dragY) / 3
                               : list.dragY > list.height - edge ? (list.dragY - list.height + edge) / 3 : 0;
                    if (step === 0)
                        return;
                    const maxY = Math.max(0, list.contentHeight - list.height) + list.originY;
                    list.contentY = Math.max(list.originY, Math.min(maxY, list.contentY + step));
                }
            }

            // Where the dragged row would land.
            Rectangle {
                parent: list.contentItem
                visible: list.dragFrom >= 0 && list.dropIndex >= 0
                x: 0
                y: list.dropIndex * Theme.metrics.rowHeight - 1
                z: 10
                width: list.width
                height: 2
                radius: 1
                color: Theme.palette.accent
            }

            function checkEnd() {
                if (visible && count > 0 && atYEnd)
                    root.endReached();
            }

            onAtYEndChanged: checkEnd()
            // A short page that fits without scrolling never changes atYEnd.
            onCountChanged: Qt.callLater(checkEnd)
            onHeightChanged: Qt.callLater(checkEnd)

            footer: Item {
                width: list.width
                height: root.loadingMore ? 48 : 0
                visible: root.loadingMore

                Text {
                    anchors.centerIn: parent
                    text: "Loading more…"
                    color: Theme.palette.textSecondary
                    font.family: Theme.typography.family
                    font.pixelSize: Theme.typography.bodySize
                }
            }

            delegate: TrackRow {
                id: row
                width: list.width - 12
                playing: PlayerViewModel.hasTrack && PlayerViewModel.trackId === trackId
                selected: root.selectedIds[trackId] === true

                indexColWidth: root.indexCol
                albumColWidth: root.albumCol
                dateColWidth: root.dateCol
                durationColWidth: root.durationCol
                actionsColWidth: root.actionsCol

                onClicked: (modifiers) => {
                    if (root.clickForSelection(index, trackId, modifiers))
                        return;
                    root.selectionAnchor = index;
                    if (root.singleTrackPlayback || (root.clickToPlay && playing))
                        PlayerViewModel.playSingle(trackId); // Does not restart the current track.
                    else if (root.clickToPlay)
                        PlayerViewModel.play(root.model, index);
                }
                onDoubleClicked: (modifiers) => {
                    if (!root.selecting && modifiers === Qt.NoModifier && !root.singleTrackPlayback && !root.clickToPlay)
                        PlayerViewModel.play(root.model, index);
                }
                onLikeClicked: LibraryViewModel.toggleLiked(trackId)
                onMoreClicked: (anchor) => {
                    root.menuRow = index;
                    trackMenu.openFor(anchor, row);
                }
                onContextRequested: (scenePosition) => root.showRowMenu(scenePosition, index, trackId)

                showGrip: root.reorderable
                dragging: list.dragFrom === index
                onDragStarted: {
                    list.dragFrom = index;
                    list.dropIndex = index;
                    autoScroll.start();
                }
                onDragMoved: (scenePosition) => list.updateDrop(scenePosition)
                onDragEnded: list.finishDrag()
            }
        }

        Loader {
            active: root.tabContent !== null && root.currentTab !== "all"
            visible: active
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: root.tabContent
        }

        EmptyState {
            visible: !root.showList && !(root.tabContent !== null && root.currentTab !== "all")
            Layout.fillWidth: true
            Layout.fillHeight: true
            iconName: root.currentTab === "all" ? root.emptyIcon : "list"
            title: root.currentTab === "all" ? root.emptyTitle : "Coming soon"
            text: root.currentTab === "all" ? root.emptyText : "This view is not implemented yet."
            actionText: root.currentTab === "all" ? root.emptyActionText : ""
            onActionClicked: root.emptyActionClicked()
        }
    }

    TrackMenu {
        id: trackMenu
        contextPlaylistId: root.contextPlaylistId
        contextSmart: root.contextSmart
        selectable: true
        rowSelected: root.selectedIds[trackMenu.trackId] === true
        onTracksChanged: root.selectionRevision++
        onSelectRequested: (trackId) => {
            root.toggleSelected(trackId);
            root.selectionAnchor = root.menuRow;
        }
    }

    ContextMenu { id: selectionMenu }

    // ─── Actions on the selected tracks ────────────────
    Component {
        id: selectionBar

        Rectangle {
            id: bar

            // files, downloads, saved, downloadable, liked, total: see LibraryViewModel.describeTracks().
            readonly property var info: {
                root.selectionRevision;
                root.model.count;
                return LibraryViewModel.describeTracks(root.selectedList());
            }
            readonly property int removable: info.files + info.downloads + info.saved

            implicitHeight: barRow.implicitHeight + 16
            radius: Theme.metrics.radiusLg
            color: Theme.palette.selectedOverlay
            border.width: 1
            border.color: Theme.palette.hairline

            Flow {
                id: barRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 12
                anchors.rightMargin: 8
                spacing: 8

                Text {
                    height: addButton.height
                    verticalAlignment: Text.AlignVCenter
                    rightPadding: 4
                    text: root.selectedCount + " selected"
                    color: Theme.palette.textPrimary
                    font.family: Theme.typography.family
                    font.pixelSize: Theme.typography.bodySize
                    font.weight: Font.DemiBold
                }
                TextButton {
                    id: addButton
                    text: "Add to playlist"
                    onClicked: trackMenu.openPlaylistsFor(addButton, root.selectedList())
                }
                TextButton {
                    text: "Properties…"
                    onClicked: LibraryViewModel.openProperties(root.selectedList())
                }
                TextButton {
                    text: bar.info.liked ? "Unlike" : "Like"
                    onClicked: {
                        LibraryViewModel.toggleLikedTracks(root.selectedList());
                        root.selectionRevision++;
                    }
                }
                TextButton {
                    visible: bar.info.downloadable > 0
                    text: "Download " + bar.info.downloadable
                    onClicked: {
                        DownloadsViewModel.downloadTracks(root.selectedList());
                        root.selectionRevision++;
                    }
                }
                TextButton {
                    visible: root.contextPlaylistId.length > 0
                    text: root.contextSmart ? "Hide from this playlist" : "Remove from this playlist"
                    onClicked: {
                        PlaylistsViewModel.removeTracks(root.contextPlaylistId, root.selectedList());
                        root.clearSelection();
                    }
                }
                TextButton {
                    visible: bar.removable > 0
                    text: bar.info.files > 0 ? "Delete…" : "Remove…"
                    onClicked: root.askRemoveSelected(bar.info)
                }
                TextButton {
                    text: root.selectedCount === root.model.count ? "Select none" : "Select all"
                    onClicked: root.selectedCount === root.model.count ? root.selectedIds = {} : root.selectAll()
                }
                TextButton {
                    text: "Done"
                    onClicked: root.clearSelection()
                }
            }
        }
    }

    ConfirmDialog { id: removeDialog }
    ConfirmDialog { id: removeFailedDialog }

    component ColumnHeader: Text {
        anchors.verticalCenter: parent.verticalCenter
        elide: Text.ElideRight
        color: Theme.palette.textTertiary
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.headerSize
        font.weight: Font.DemiBold
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 1
    }
}
