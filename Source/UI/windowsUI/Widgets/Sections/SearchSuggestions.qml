pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import SoundLink 1.0

// Suggestions under the title bar search, from SearchViewModel.suggestions.
// The search field keeps the keyboard focus; it drives this list through moveCurrent() and currentIndex.
Popup {
    id: root

    property alias currentIndex: list.currentIndex
    readonly property SuggestionListModel model: SearchViewModel.suggestions

    signal activated(int row)

    function moveCurrent(step) {
        if (list.count === 0)
            return;
        list.currentIndex = (list.currentIndex + step + list.count) % list.count;
    }

    padding: 6
    focus: false
    closePolicy: Popup.CloseOnPressOutsideParent

    background: Rectangle {
        color: Theme.palette.fieldBg
        radius: Theme.metrics.radiusLg
        border.width: 1
        border.color: Theme.palette.borderStrong
    }

    contentItem: Column {
        spacing: 4

        ListView {
            id: list
            width: parent.width
            height: Math.min(contentHeight, 440)
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: root.model
            currentIndex: 0
            highlightMoveDuration: 0

            Connections {
                target: root.model
                function onModelReset() { list.currentIndex = 0; }
            }

            delegate: Rectangle {
                id: row

                required property int index
                required property int kind
                required property string title
                required property string subtitle
                required property string icon
                required property string tag

                readonly property bool current: ListView.isCurrentItem

                width: list.width
                height: 44
                radius: Theme.metrics.radiusMd
                color: current ? Theme.palette.selectedOverlay : "transparent"

                AppIcon {
                    id: rowIcon
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    width: 16; height: 16
                    name: row.icon
                    color: row.current ? Theme.palette.iconActive : Theme.palette.iconDefault
                }

                Column {
                    anchors.left: rowIcon.right
                    anchors.leftMargin: 12
                    anchors.right: tagPill.left
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 1

                    Text {
                        width: parent.width
                        elide: Text.ElideRight
                        text: row.title
                        color: Theme.palette.textPrimary
                        font.family: Theme.typography.family
                        font.pixelSize: Theme.typography.bodySize
                        font.weight: row.current ? Font.DemiBold : Font.Normal
                    }
                    Text {
                        width: parent.width
                        visible: row.subtitle.length > 0
                        elide: Text.ElideRight
                        text: row.subtitle
                        color: Theme.palette.textSecondary
                        font.family: Theme.typography.family
                        font.pixelSize: Theme.typography.tinySize
                    }
                }

                // The tag form of this suggestion, so the short syntax is learned by looking.
                Rectangle {
                    id: tagPill
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    visible: row.tag.length > 0
                    width: visible ? Math.min(tagMetrics.advanceWidth + 12, 200) : 0
                    height: 20
                    radius: Theme.metrics.radiusSm
                    color: Theme.palette.pillSelected

                    TextMetrics {
                        id: tagMetrics
                        font: tagText.font
                        text: row.tag
                    }

                    Text {
                        id: tagText
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        text: row.tag
                        color: Theme.palette.textSecondary
                        font.family: "Consolas"
                        font.pixelSize: Theme.typography.tinySize
                    }
                }

                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                    onHoveredChanged: if (hovered) list.currentIndex = row.index
                }
                TapHandler { onTapped: root.activated(row.index) }
            }
        }

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.palette.hairline
        }

        Text {
            width: parent.width
            leftPadding: 10
            topPadding: 2
            bottomPadding: 2
            elide: Text.ElideRight
            text: "↑↓ choose · Enter open · Tab complete · tags: yt: lib: s: go: · ; separates parts"
            color: Theme.palette.textTertiary
            font.family: Theme.typography.family
            font.pixelSize: Theme.typography.tinySize
        }
    }
}
