import QtQuick
import SoundLink 1.0

// A line of a menu popup: an icon, the text and an optional icon at the end.
Rectangle {
    id: entry

    property string iconName: ""
    property string text: ""
    property string trailing: ""
    property bool dimmed: false

    signal triggered

    width: 232
    height: visible ? 32 : 0
    radius: Theme.metrics.radiusMd
    color: entryHover.hovered && !dimmed ? Theme.palette.hoverOverlay : "transparent"

    AppIcon {
        id: entryIcon
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        width: 14; height: 14
        name: entry.iconName
        color: entryHover.hovered ? Theme.palette.iconActive : Theme.palette.iconDefault
    }
    Text {
        anchors.left: entryIcon.right
        anchors.leftMargin: 10
        anchors.right: trailingIcon.left
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        elide: Text.ElideRight
        text: entry.text
        color: entry.dimmed ? Theme.palette.textSecondary : Theme.palette.textPrimary
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.bodySize
    }
    AppIcon {
        id: trailingIcon
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        width: entry.trailing.length > 0 ? 12 : 0
        height: 12
        name: entry.trailing
        color: Theme.palette.textSecondary
    }

    HoverHandler { id: entryHover; cursorShape: entry.dimmed ? Qt.ArrowCursor : Qt.PointingHandCursor }
    TapHandler { onTapped: entry.triggered() }
}
