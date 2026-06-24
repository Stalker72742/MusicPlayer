import QtQuick
import SoundLink 1.0

Rectangle {
    id: root

    property int    index: 0
    property string title: ""
    property string artist: ""
    property string album: ""
    property string dateAdded: ""
    property string duration: ""
    property color  artTint: "#3A3A40"
    property bool   liked: false
    property bool   playing: false
    property bool   hovered: hover.hovered
    property bool   selected: false

    // column widths (driven by parent header)
    property real indexColWidth: 60
    property real albumColWidth: 280
    property real dateColWidth:  160
    property real durationColWidth: 100
    property real actionsColWidth: 90

    signal clicked
    signal doubleClicked
    signal likeClicked
    signal moreClicked

    implicitHeight: Theme.metrics.rowHeight

    color: selected ? Theme.palette.selectedOverlay
         : hovered  ? Theme.palette.hoverOverlay
                    : "transparent"
    radius: Theme.metrics.radiusMd

    Behavior on color { ColorAnimation { duration: Theme.motion.durationFast } }

    // Index / playing indicator
    Item {
        id: indexCell
        width: root.indexColWidth
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        AppIcon {
            anchors.centerIn: parent
            visible: root.playing
            width: 16; height: 16
            name: "playing"
            color: Theme.palette.iconActive
        }

        Text {
            anchors.centerIn: parent
            visible: !root.playing
            text: root.index
            color: Theme.palette.textTertiary
            font.family: Theme.typography.family
            font.pixelSize: Theme.typography.bodySize
        }
    }

    // Title block (art + texts)
    Item {
        id: titleCell
        anchors.left: indexCell.right
        anchors.right: albumCell.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        AlbumArt {
            id: art
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            width: 36; height: 36
            tint: root.artTint
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: art.right
            anchors.leftMargin: 12
            anchors.right: parent.right
            anchors.rightMargin: 16
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
    }

    Text {
        id: albumCell
        width: root.albumColWidth
        anchors.right: dateCell.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 16
        elide: Text.ElideRight
        text: root.album
        color: Theme.palette.textSecondary
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.bodySize
    }

    Text {
        id: dateCell
        width: root.dateColWidth
        anchors.right: durationCell.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 16
        elide: Text.ElideRight
        text: root.dateAdded
        color: Theme.palette.textSecondary
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.bodySize
    }

    Text {
        id: durationCell
        width: root.durationColWidth
        anchors.right: actionsCell.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 16
        text: root.duration
        color: Theme.palette.textSecondary
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.bodySize
    }

    Row {
        id: actionsCell
        width: root.actionsColWidth
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 8
        spacing: 4
        opacity: root.hovered || root.liked ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: Theme.motion.durationFast } }

        IconButton {
            iconName: root.liked ? "heart-filled" : "heart"
            iconColor: root.liked ? Theme.palette.iconActive : Theme.palette.iconDefault
            iconSize: 16
            implicitWidth: 30; implicitHeight: 30
            onClicked: root.likeClicked()
        }
        IconButton {
            iconName: "more"
            iconSize: 16
            implicitWidth: 30; implicitHeight: 30
            onClicked: root.moreClicked()
        }
    }

    HoverHandler { id: hover }
    TapHandler {
        onTapped: root.clicked()
        onDoubleTapped: root.doubleClicked()
    }
}
