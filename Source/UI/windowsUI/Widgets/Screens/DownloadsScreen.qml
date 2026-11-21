pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SoundLink 1.0

// The download queue (what is waiting, downloading, done or failed) and, on its own tab, device sync: files being
// received and conflicts waiting for a choice.
Item {
    id: root

    /// "downloads" or "sync".
    property string tab: "downloads"

    // Navigation keeps this with the location while the screen is gone; see WindowsMainWindow.
    function saveState() {
        return {
            scroll: list.contentY - list.originY,
            syncScroll: syncPanel.listView.contentY - syncPanel.listView.originY,
            tab: tab
        };
    }

    function restoreScroll(view, y) {
        if (y > 0)
            Qt.callLater(() => view.contentY = view.originY + Math.min(y, Math.max(0, view.contentHeight - view.height)));
    }

    Component.onCompleted: {
        const state = Navigation.pageState;
        tab = state.tab || (SyncViewModel.conflictCount > 0 ? "sync" : "downloads");
        restoreScroll(list, state.scroll || 0);
        restoreScroll(syncPanel.listView, state.syncScroll || 0);
    }

    // A notification about conflicts was clicked.
    Connections {
        target: SyncViewModel

        function onConflictsRequested() {
            root.tab = "sync";
        }
    }

    readonly property DownloadListModel jobs: DownloadsViewModel.jobs

    function showJobMenu(point, id, state) {
        const done = state === DownloadListModel.Done;
        const failed = state === DownloadListModel.Failed;
        jobMenu.openAt(point, [
            done ? { text: "Play", icon: "play", action: () => PlayerViewModel.playSingle(id) } : null,
            failed ? { text: "Retry", icon: "repeat", action: () => DownloadsViewModel.retry(id) } : null,
            done ? null : { text: failed ? "Remove from the queue" : "Cancel", icon: "close", action: () => DownloadsViewModel.cancel(id) },
            { separator: true },
            done ? { text: "Show in folder", icon: "folder", action: () => LibraryViewModel.showInFolder(id) } : null,
            { text: "Properties…", icon: "tag", action: () => LibraryViewModel.openProperties([id]) },
            done ? { text: "Remove download…", icon: "trash", action: () => removeDialog.ask("Remove download",
                "The downloaded file will be deleted. The track stays and plays from YouTube again.",
                "Remove", () => DownloadsViewModel.removeDownload(id)) } : null
        ]);
    }

    ContextMenu { id: jobMenu }
    ConfirmDialog { id: removeDialog }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 56
        anchors.rightMargin: 32
        anchors.topMargin: 32
        spacing: 16

        ScreenHeader {
            Layout.fillWidth: true
            title: "Downloads"
            subtitle: root.tab === "sync"
                      ? (SyncViewModel.running && SyncViewModel.fileSyncAvailable
                         ? "Received files go to " + SyncViewModel.placementRoot : "")
                      : (DownloadsViewModel.activeCount > 0 ? DownloadsViewModel.activeCount + " in the queue · " : "")
                        + "Saved to " + DownloadsViewModel.folder
        }

        Row {
            spacing: 4

            TabPill {
                text: DownloadsViewModel.activeCount > 0 ? "Downloads · " + DownloadsViewModel.activeCount : "Downloads"
                selected: root.tab === "downloads"
                onClicked: root.tab = "downloads"
            }
            TabPill {
                text: SyncViewModel.conflictCount > 0 ? "Sync · " + SyncViewModel.conflictCount : "Sync"
                selected: root.tab === "sync"
                onClicked: root.tab = "sync"
            }
        }

        Row {
            visible: root.tab === "downloads"
            spacing: 8

            TextButton {
                text: "Open folder"
                onClicked: DownloadsViewModel.openFolder()
            }
            TextButton {
                visible: DownloadsViewModel.finishedCount > 0
                text: "Clear finished"
                onClicked: DownloadsViewModel.clearFinished()
            }
        }

        ListView {
            id: list
            visible: root.tab === "downloads" && root.jobs.count > 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 8
            clip: true
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            model: root.jobs
            ScrollBar.vertical: AppScrollBar {}

            delegate: Rectangle {
                id: job

                required property int trackId
                required property string title
                required property string artist
                required property string artUrl
                required property color artTint
                required property int jobState
                required property real progress
                required property string sizeText
                required property string error

                width: list.width - 12
                height: 64
                radius: Theme.metrics.radiusMd
                color: rowHover.hovered ? Theme.palette.hoverOverlay : "transparent"

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onClicked: (mouse) => root.showJobMenu(job.mapToItem(null, mouse.x, mouse.y), job.trackId, job.jobState)
                }

                AlbumArt {
                    id: art
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 44; height: 44
                    tint: job.artTint
                    source: job.artUrl
                }

                Column {
                    anchors.left: art.right
                    anchors.leftMargin: 14
                    anchors.right: actions.left
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 5

                    Text {
                        width: parent.width
                        elide: Text.ElideRight
                        text: job.title + (job.artist.length > 0 ? "  ·  " + job.artist : "")
                        color: Theme.palette.textPrimary
                        font.family: Theme.typography.family
                        font.pixelSize: Theme.typography.bodySize
                        font.weight: Font.DemiBold
                    }

                    // Progress while downloading; an indeterminate stripe until the size is known.
                    Rectangle {
                        visible: job.jobState === DownloadListModel.Downloading
                        width: parent.width
                        height: 4
                        radius: 2
                        color: Theme.palette.sliderTrack
                        clip: true

                        Rectangle {
                            height: parent.height
                            radius: 2
                            color: Theme.palette.sliderFill
                            width: job.progress >= 0 ? parent.width * job.progress : parent.width

                            Behavior on width { NumberAnimation { duration: Theme.motion.durationFast } }

                            // Pulses until the size is known.
                            SequentialAnimation on opacity {
                                running: job.progress < 0
                                loops: Animation.Infinite
                                NumberAnimation { from: 0.15; to: 0.6; duration: 700 }
                                NumberAnimation { from: 0.6; to: 0.15; duration: 700 }
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        elide: Text.ElideRight
                        text: {
                            switch (job.jobState) {
                            case DownloadListModel.Queued:      return "Waiting";
                            case DownloadListModel.Downloading: return job.sizeText.length > 0 ? job.sizeText : "Starting…";
                            case DownloadListModel.Done:        return "Downloaded" + (job.sizeText.length > 0 ? " · " + job.sizeText : "");
                            }
                            return "Failed: " + job.error;
                        }
                        color: job.jobState === DownloadListModel.Failed ? Theme.palette.danger : Theme.palette.textSecondary
                        font.family: Theme.typography.family
                        font.pixelSize: Theme.typography.smallSize
                    }
                }

                Row {
                    id: actions
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6

                    TextButton {
                        visible: job.jobState === DownloadListModel.Failed
                        text: "Retry"
                        onClicked: DownloadsViewModel.retry(job.trackId)
                    }
                    TextButton {
                        visible: job.jobState === DownloadListModel.Queued || job.jobState === DownloadListModel.Downloading
                                 || job.jobState === DownloadListModel.Failed
                        text: job.jobState === DownloadListModel.Failed ? "Remove" : "Cancel"
                        onClicked: DownloadsViewModel.cancel(job.trackId)
                    }
                    TextButton {
                        visible: job.jobState === DownloadListModel.Done
                        text: "Play"
                        onClicked: PlayerViewModel.playSingle(job.trackId)
                    }
                }

                HoverHandler { id: rowHover }
            }
        }

        EmptyState {
            visible: root.tab === "downloads" && root.jobs.count === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            iconName: "download"
            title: "Nothing downloading"
            text: "Download a YouTube track from its ⋯ menu, or a whole playlist with Download. Downloaded tracks play from the file, with no waiting for the stream."
        }

        SyncPanel {
            id: syncPanel
            visible: root.tab === "sync"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 8
        }
    }
}
