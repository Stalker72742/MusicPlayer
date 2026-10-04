import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SoundLink 1.0

// Collapsible sidebar section. The chevron only collapses or expands it; the rest of the header
// emits clicked() (and expands a collapsed section). In compact mode the items are always shown as
// an icon column and the header turns into a separator (or nothing, see compactSeparator);
// a section without items keeps its header as an icon button.
Item {
    id: root

    property string title: ""
    property string iconName: ""
    property bool   compact: false
    property bool   expanded: true
    property bool   collapsible: true
    property bool   showDot: false
    property bool   selected: false
    property bool   compactSeparator: true
    property bool   hovered: hover.hovered

    default property alias content: itemsLayout.children

    signal clicked

    readonly property bool hasItems: itemsLayout.children.length > 0
    readonly property bool itemsShown: hasItems && (compact || expanded)
    readonly property bool headerShown: !(compact && hasItems)

    readonly property real headerHeight: headerShown ? Theme.metrics.navItemHeight
                                                     : (compactSeparator ? separatorArea : 0)
    readonly property real separatorArea: 9
    readonly property real itemsTopMargin: headerShown ? 4 : 0

    implicitHeight: headerHeight + (itemsShown ? itemsTopMargin + itemsLayout.implicitHeight : 0)
    clip: true

    Behavior on implicitHeight { NumberAnimation { duration: Theme.motion.durationNormal; easing.type: Theme.motion.easingType } }

    Rectangle {
        id: separator
        visible: !root.headerShown && root.compactSeparator
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        y: (root.separatorArea - height) / 2
        height: 1
        color: Theme.palette.hairline
    }

    Rectangle {
        id: header
        visible: root.headerShown
        width: parent.width
        height: Theme.metrics.navItemHeight
        radius: Theme.metrics.radiusMd
        color: root.selected ? Theme.palette.selectedOverlay
                             : (hover.hovered ? Theme.palette.hoverOverlay : "transparent")

        ToolTip.visible: root.compact && hover.hovered
        ToolTip.text: root.title
        ToolTip.delay: 500

        Behavior on color { ColorAnimation { duration: Theme.motion.durationFast } }

        AppIcon {
            id: chevron
            visible: root.collapsible && root.hasItems && !root.compact
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
            x: root.compact ? (parent.width - width) / 2 : chevron.x + chevron.width + 6
            width: 16; height: 16
            name: root.iconName
            color: hover.hovered ? Theme.palette.iconActive : Theme.palette.textSecondary
        }

        Text {
            id: titleText
            visible: !root.compact
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
        }

        Rectangle {
            id: dot
            visible: root.showDot
            anchors.right: parent.right
            anchors.rightMargin: root.compact ? 10 : 12
            // Positioned by y: switching anchors between modes briefly applies both and stretches the dot.
            y: root.compact ? 6 : (parent.height - height) / 2
            width: root.compact ? 6 : 8
            height: width
            radius: width / 2
            color: Theme.palette.badge
        }

        HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
        TapHandler {
            onTapped: (eventPoint) => {
                const canCollapse = root.collapsible && root.hasItems && !root.compact;
                if (canCollapse && eventPoint.position.x < chevron.x + chevron.width + 4) {
                    root.expanded = !root.expanded;
                    return;
                }
                if (canCollapse)
                    root.expanded = true;
                root.clicked();
            }
        }
    }

    ColumnLayout {
        id: itemsLayout
        y: root.headerHeight + root.itemsTopMargin
        width: parent.width
        spacing: 2
        visible: root.itemsShown
    }
}
