pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import SoundLink 1.0

// Track list screen: header, optional view tabs and the list, or an empty state.
Item {
    id: root

    property string title: ""
    property string subtitle: ""
    property TrackListModel model: null
    property bool showTabs: false
    property string emptyIcon: "library"
    property string emptyTitle: ""
    property string emptyText: ""
    property string emptyActionText: ""

    // Search results: a click plays just that track; library lists queue the whole list on double click.
    property bool singleTrackPlayback: false

    property string albumHeader: "Album"
    property string dateHeader: "Date Added"

    // Endless lists: endReached() when scrolled to the bottom (or when everything fits), a footer while loading.
    property bool loadingMore: false

    // Buttons under the header, e.g. a playlist's Play and Edit.
    property Component headerActions: null

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

        ScreenHeader {
            Layout.fillWidth: true
            title: root.title
            subtitle: root.subtitle
        }

        Loader {
            active: root.headerActions !== null
            visible: active
            Layout.fillWidth: true
            sourceComponent: root.headerActions
        }

        Row {
            visible: root.showTabs
            spacing: 4
            Layout.topMargin: 4

            Repeater {
                model: [
                    { id: "all", title: "All" },
                    { id: "albums", title: "Albums" },
                    { id: "artists", title: "Artists" },
                    { id: "genres", title: "Genres" }
                ]

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
        }

        ListView {
            id: list
            visible: root.showList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.model
            boundsBehavior: Flickable.StopAtBounds

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
                width: list.width
                playing: PlayerViewModel.hasTrack && PlayerViewModel.trackId === trackId

                indexColWidth: root.indexCol
                albumColWidth: root.albumCol
                dateColWidth: root.dateCol
                durationColWidth: root.durationCol
                actionsColWidth: root.actionsCol

                onClicked: if (root.singleTrackPlayback) PlayerViewModel.playSingle(trackId)
                onDoubleClicked: if (!root.singleTrackPlayback) PlayerViewModel.play(root.model, index)
                onLikeClicked: LibraryViewModel.toggleLiked(trackId)
                onMoreClicked: (anchor) => trackMenu.openFor(anchor, row)

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

        EmptyState {
            visible: !root.showList
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
    }

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
