import QtQuick
import SoundLink 1.0

// A track as a cover card, for the home screen shelves. Delegate for TrackListModel rows.
Item {
    id: root

    required property int    index
    required property int    trackId
    required property string title
    required property string artist
    required property color  artTint
    required property string artUrl
    required property bool   online

    readonly property bool current: PlayerViewModel.hasTrack && PlayerViewModel.trackId === trackId

    signal clicked

    implicitWidth: 168
    implicitHeight: art.height + texts.implicitHeight + 22

    Rectangle {
        anchors.fill: parent
        radius: Theme.metrics.radiusLg
        color: hover.hovered ? Theme.palette.hoverOverlay : "transparent"

        Behavior on color { ColorAnimation { duration: Theme.motion.durationFast } }
    }

    AlbumArt {
        id: art
        x: 8
        y: 8
        width: root.width - 16
        height: width
        cornerRadius: Theme.metrics.radiusMd
        tint: root.artTint
        source: root.artUrl

        // Play button on hover, equalizer while it is the current track.
        Rectangle {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 8
            width: 36; height: 36
            radius: 18
            color: Theme.palette.accent
            visible: hover.hovered || root.current
            opacity: hover.hovered ? 1 : 0.9

            AppIcon {
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: hover.hovered ? 1 : 0
                width: 16; height: 16
                name: hover.hovered ? "play" : "playing"
                color: Theme.palette.content
            }
        }
    }

    Column {
        id: texts
        anchors.top: art.bottom
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 2

        Text {
            width: parent.width
            elide: Text.ElideRight
            text: root.title
            color: root.current ? Theme.palette.accent : Theme.palette.textPrimary
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

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: root.clicked() }
}
