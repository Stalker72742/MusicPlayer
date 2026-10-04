pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import SoundLink 1.0

// The download queue: what is waiting, downloading, done or failed.
Item {
    id: root

    readonly property DownloadListModel jobs: DownloadsViewModel.jobs

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 56
        anchors.rightMargin: 32
        anchors.topMargin: 32
        spacing: 16

        ScreenHeader {
            Layout.fillWidth: true
            title: "Downloads"
            subtitle: (DownloadsViewModel.activeCount > 0 ? DownloadsViewModel.activeCount + " in the queue · " : "")
                      + "Saved to " + DownloadsViewModel.folder
        }

        Row {
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
            visible: root.jobs.count > 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 8
            clip: true
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            model: root.jobs

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

                width: list.width
                height: 64
                radius: Theme.metrics.radiusMd
                color: rowHover.hovered ? Theme.palette.hoverOverlay : "transparent"

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
            visible: root.jobs.count === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            iconName: "download"
            title: "Nothing downloading"
            text: "Download a YouTube track from its ⋯ menu, or a whole playlist with Download. Downloaded tracks play from the file, with no waiting for the stream."
        }
    }
}
