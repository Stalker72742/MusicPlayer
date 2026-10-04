import QtQuick
import SoundLink 1.0

// Delegate for TrackListModel rows.
Rectangle {
    id: root

    required property int    index
    required property int    trackId
    required property int    number
    required property string title
    required property string artist
    required property string album
    required property string dateAdded
    required property string duration
    required property color  artTint
    required property string artUrl
    required property bool   liked
    required property bool   online
    required property bool   inLibrary
    required property bool   downloaded

    property bool   playing: false

    // Reorderable lists: a grip replaces the number on hover; dragging it reports scene positions.
    property bool   showGrip: false
    property bool   dragging: false
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
    signal moreClicked(Item anchor)
    signal dragStarted
    signal dragMoved(point scenePosition)
    signal dragEnded

    implicitHeight: Theme.metrics.rowHeight
    opacity: dragging ? 0.4 : 1

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

        readonly property bool gripShown: root.showGrip && (root.hovered || root.dragging)

        AppIcon {
            anchors.centerIn: parent
            visible: indexCell.gripShown
            width: 16; height: 16
            name: "grip"
            color: Theme.palette.iconActive
        }

        AppIcon {
            anchors.centerIn: parent
            visible: root.playing && !indexCell.gripShown
            width: 16; height: 16
            name: "playing"
            color: Theme.palette.iconActive
        }

        Text {
            anchors.centerIn: parent
            visible: !root.playing && !indexCell.gripShown
            text: root.number
            color: Theme.palette.textTertiary
            font.family: Theme.typography.family
            font.pixelSize: Theme.typography.bodySize
        }

        HoverHandler {
            enabled: root.showGrip
            cursorShape: gripDrag.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        }
        DragHandler {
            id: gripDrag
            enabled: root.showGrip
            target: null
            xAxis.enabled: false
            // The list must not turn this drag into a flick.
            grabPermissions: PointerHandler.CanTakeOverFromAnything

            onActiveChanged: active ? root.dragStarted() : root.dragEnded()
            onCentroidChanged: if (active) root.dragMoved(centroid.scenePosition)
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
            source: root.artUrl
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
            Row {
                width: parent.width
                spacing: 5

                AppIcon {
                    id: onlineMark
                    visible: root.online
                    anchors.verticalCenter: parent.verticalCenter
                    width: visible ? 11 : 0
                    height: 11
                    // Streamed, or played from its downloaded copy.
                    name: root.downloaded ? "download" : "globe"
                    color: Theme.palette.textTertiary
                }
                Text {
                    width: parent.width - (onlineMark.visible ? onlineMark.width + parent.spacing : 0)
                    elide: Text.ElideRight
                    text: root.artist
                    color: Theme.palette.textSecondary
                    font.family: Theme.typography.family
                    font.pixelSize: Theme.typography.smallSize
                }
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
            id: moreButton
            iconName: "more"
            iconSize: 16
            implicitWidth: 30; implicitHeight: 30
            onClicked: root.moreClicked(moreButton)
        }
    }

    HoverHandler { id: hover }
    TapHandler {
        onTapped: root.clicked()
        onDoubleTapped: root.doubleClicked()
    }
}
