import QtQuick
import SoundLink 1.0
import "../Components"

Rectangle {
    id: root

    property string title: ""
    property string artist: ""
    property string artSource: ""
    property color  artTint: "#3A3A40"

    property bool playing: false
    property bool shuffled: false
    property bool repeating: false
    property bool liked: false

    property real progress: 0.0      // 0..1
    property string elapsedText: "0:00"
    property string totalText: "0:00"

    property real volume: 0.7

    signal togglePlay
    signal next
    signal prev
    signal toggleShuffle
    signal toggleRepeat
    signal toggleLike
    signal seekRequested(real value)
    signal volumeRequested(real value)
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
            tint: root.artTint
            source: root.artSource
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
                text: root.title
                color: Theme.palette.textPrimary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.bodySize
                font.weight: Font.DemiBold
            }
            Text {
                width: parent.width
                elide: Text.ElideRight
                text: root.artist
                color: Theme.palette.textSecondary
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
            iconName: root.liked ? "heart-filled" : "heart"
            iconSize: 16
            iconColor: root.liked ? Theme.palette.iconActive : Theme.palette.iconDefault
            onClicked: root.toggleLike()
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
                active: root.shuffled
                onClicked: root.toggleShuffle()
            }
            PlaybackButton {
                anchors.verticalCenter: parent.verticalCenter
                iconName: "prev"
                iconSize: 16
                onClicked: root.prev()
            }
            PlayButton {
                anchors.verticalCenter: parent.verticalCenter
                playing: root.playing
                diameter: 40
                onClicked: root.togglePlay()
            }
            PlaybackButton {
                anchors.verticalCenter: parent.verticalCenter
                iconName: "next"
                iconSize: 16
                onClicked: root.next()
            }
            PlaybackButton {
                anchors.verticalCenter: parent.verticalCenter
                iconName: "repeat"
                iconSize: 16
                active: root.repeating
                onClicked: root.toggleRepeat()
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
                text: root.elapsedText
                color: Theme.palette.textSecondary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.smallSize
            }
            Text {
                id: total
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                text: root.totalText
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
                value: root.progress
                onMoved: (v) => root.seekRequested(v)
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
                value: root.volume
                onMoved: (v) => root.volumeRequested(v)
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
