import QtQuick
import SoundLink 1.0

// Now-playing bar, driven by PlayerViewModel.
Rectangle {
    id: root

    signal queueClicked

    implicitHeight: Theme.metrics.bottomBarHeight
    color: Theme.palette.bottomBar

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Theme.palette.hairline
    }

    // ─── Left: Now playing info ───────────────────────────
    Item {
        id: leftBlock
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 16
        width: 300

        AlbumArt {
            id: art
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            width: 56; height: 56
            cornerRadius: Theme.metrics.radiusSm
            tint: PlayerViewModel.artTint
            source: PlayerViewModel.artUrl
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: art.right
            anchors.right: likeBtn.left
            anchors.leftMargin: 14
            anchors.rightMargin: 8
            spacing: 2

            Text {
                width: parent.width
                elide: Text.ElideRight
                text: PlayerViewModel.title
                color: Theme.palette.textPrimary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.bodySize
                font.weight: Font.DemiBold
            }
            Text {
                width: parent.width
                elide: Text.ElideRight
                // Online tracks: the stream is being found, or it could not be played.
                text: PlayerViewModel.errorText.length > 0 ? "Cannot play: " + PlayerViewModel.errorText
                    : PlayerViewModel.loading ? "Loading stream…"
                    : PlayerViewModel.artist
                color: PlayerViewModel.errorText.length > 0 ? Theme.palette.danger : Theme.palette.textSecondary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.smallSize
            }
        }

        IconButton {
            id: likeBtn
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 4
            implicitWidth: 32; implicitHeight: 32
            iconName: PlayerViewModel.liked ? "heart-filled" : "heart"
            iconSize: 16
            iconColor: PlayerViewModel.liked ? Theme.palette.iconActive : Theme.palette.iconDefault
            onClicked: PlayerViewModel.toggleLike()
        }
    }

    // ─── Center: Controls + progress ──────────────────────
    Item {
        id: centerBlock
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Math.min(560, parent.width - leftBlock.width - rightBlock.width - 80)

        // Controls row
        Row {
            id: controls
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 16
            spacing: 12

            PlaybackButton {
                anchors.verticalCenter: parent.verticalCenter
                iconName: "shuffle"
                iconSize: 16
                active: PlayerViewModel.shuffle
                onClicked: PlayerViewModel.toggleShuffle()
            }
            PlaybackButton {
                anchors.verticalCenter: parent.verticalCenter
                iconName: "prev"
                iconSize: 16
                onClicked: PlayerViewModel.previous()
            }
            PlayButton {
                anchors.verticalCenter: parent.verticalCenter
                playing: PlayerViewModel.playing
                diameter: 40
                onClicked: PlayerViewModel.togglePlay()
            }
            PlaybackButton {
                anchors.verticalCenter: parent.verticalCenter
                iconName: "next"
                iconSize: 16
                onClicked: PlayerViewModel.next()
            }
            PlaybackButton {
                anchors.verticalCenter: parent.verticalCenter
                iconName: "repeat"
                iconSize: 16
                // Off, the whole queue, the current track.
                active: PlayerViewModel.repeatMode > 0
                badge: PlayerViewModel.repeatMode === 2 ? "1" : ""
                onClicked: PlayerViewModel.toggleRepeat()
            }
        }

        // Progress with timestamps
        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 14
            height: 14

            Text {
                id: elapsed
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                text: PlayerViewModel.elapsedText
                color: Theme.palette.textSecondary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.smallSize
            }
            Text {
                id: total
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                text: PlayerViewModel.totalText
                color: Theme.palette.textSecondary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.smallSize
            }
            ProgressSlider {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: elapsed.right
                anchors.right: total.left
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                value: PlayerViewModel.progress
                onMoved: (v) => PlayerViewModel.seek(v)
            }
        }
    }

    // ─── Right: Volume + queue ────────────────────────────
    Row {
        id: rightBlock
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 16
        spacing: 8

        AppIcon {
            anchors.verticalCenter: parent.verticalCenter
            width: 16; height: 16
            name: "volume"
            color: Theme.palette.iconDefault
        }
        Item {
            anchors.verticalCenter: parent.verticalCenter
            width: 110
            height: 14
            ProgressSlider {
                anchors.fill: parent
                value: PlayerViewModel.volume
                onMoved: (v) => PlayerViewModel.setVolume(v)
            }
        }
        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            implicitWidth: 32; implicitHeight: 32
            iconName: "queue"
            iconSize: 16
            onClicked: root.queueClicked()
        }
    }
}
