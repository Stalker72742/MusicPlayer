import QtQuick
import SoundLink 1.0

Rectangle {
    id: root

    property string iconName: ""
    property string text: ""
    property bool   selected: false
    property bool   compact: false
    property string badge: ""
    property bool   showDot: false
    property bool   hovered: hover.hovered

    signal clicked

    implicitHeight: Theme.metrics.navItemHeight

    color: selected ? Theme.palette.selectedOverlay
                    : (hovered ? Theme.palette.hoverOverlay : "transparent")
    radius: Theme.metrics.radiusMd

    Behavior on color { ColorAnimation { duration: Theme.motion.durationFast } }

    AppIcon {
        id: icon
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: root.compact ? (root.width - width) / 2 : 12
        width: 16
        height: 16
        name: root.iconName
        color: (root.selected || root.hovered) ? Theme.palette.iconActive
                                               : Theme.palette.iconDefault

        Behavior on anchors.leftMargin { NumberAnimation { duration: Theme.motion.durationNormal; easing.type: Theme.motion.easingType } }
    }

    Text {
        id: label
        visible: !root.compact && opacity > 0
        opacity: root.compact ? 0 : 1
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: icon.right
        anchors.leftMargin: 10
        anchors.right: badgeContainer.left
        anchors.rightMargin: 8
        elide: Text.ElideRight
        text: root.text
        color: root.selected ? Theme.palette.textPrimary
             : root.hovered  ? Theme.palette.textPrimary
                             : Theme.palette.textSecondary
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.bodySize
        font.weight: root.selected ? Font.DemiBold : Font.Normal

        Behavior on opacity { NumberAnimation { duration: Theme.motion.durationFast } }
        Behavior on color { ColorAnimation { duration: Theme.motion.durationFast } }
    }

    Item {
        id: badgeContainer
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        width: badgeText.visible ? badgeText.implicitWidth : (dot.visible ? dot.width : 0)
        height: 14
        visible: !root.compact

        Text {
            id: badgeText
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            visible: root.badge.length > 0 && !root.showDot
            text: root.badge
            color: Theme.palette.textTertiary
            font.family: Theme.typography.family
            font.pixelSize: Theme.typography.smallSize
        }

        Rectangle {
            id: dot
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            visible: root.showDot
            width: 8; height: 8
            radius: 4
            color: Theme.palette.badge
        }
    }

    Rectangle {
        id: collapsedDot
        visible: root.compact && root.showDot
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 10
        anchors.topMargin: 6
        width: 6; height: 6
        radius: 3
        color: Theme.palette.badge
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: root.clicked() }
}
