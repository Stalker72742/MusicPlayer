import QtQuick
import QtQuick.Layouts
import SoundLink 1.0

Item {
    id: root

    property string title: ""
    property string iconName: ""
    property bool   compact: false
    property bool   expanded: true
    property bool   collapsible: true
    property bool   showDot: false
    property bool   hovered: hover.hovered

    default property alias content: itemsLayout.children

    signal clicked

    implicitHeight: header.implicitHeight + (expanded && !compact ? itemsLayout.implicitHeight + 6 : 0)

    Rectangle {
        id: header
        width: parent.width
        height: Theme.metrics.navItemHeight
        radius: Theme.metrics.radiusMd
        color: hover.hovered ? Theme.palette.hoverOverlay : "transparent"

        Behavior on color { ColorAnimation { duration: Theme.motion.durationFast } }

        AppIcon {
            id: chevron
            visible: root.collapsible && !root.compact
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 4
            width: 14; height: 14
            name: root.expanded ? "chevron-down" : "chevron-right"
            color: Theme.palette.textSecondary
        }

        AppIcon {
            id: sectionIcon
            visible: root.iconName.length > 0
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: root.compact ? undefined : chevron.right
            anchors.horizontalCenter: root.compact ? parent.horizontalCenter : undefined
            anchors.leftMargin: 6
            width: 16; height: 16
            name: root.iconName
            color: Theme.palette.textSecondary
        }

        Text {
            id: titleText
            visible: !root.compact
            opacity: root.compact ? 0 : 1
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: sectionIcon.visible ? sectionIcon.right : chevron.right
            anchors.leftMargin: 8
            anchors.right: parent.right
            anchors.rightMargin: 12
            elide: Text.ElideRight
            text: root.title
            color: Theme.palette.textPrimary
            font.family: Theme.typography.family
            font.pixelSize: Theme.typography.sectionSize
            font.weight: Font.DemiBold

            Behavior on opacity { NumberAnimation { duration: Theme.motion.durationFast } }
        }

        Rectangle {
            id: dot
            visible: root.showDot
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: root.compact ? 8 : 12
            width: 8; height: 8
            radius: 4
            color: Theme.palette.badge
        }

        HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
        TapHandler {
            onTapped: {
                if (root.collapsible && !root.compact)
                    root.expanded = !root.expanded;
                root.clicked();
            }
        }
    }

    ColumnLayout {
        id: itemsLayout
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 4
        spacing: 2
        visible: root.expanded && !root.compact
        opacity: visible ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: Theme.motion.durationFast } }
    }

    Behavior on implicitHeight { NumberAnimation { duration: Theme.motion.durationNormal; easing.type: Theme.motion.easingType } }
}
