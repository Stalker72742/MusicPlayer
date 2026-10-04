pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import SoundLink 1.0

// A titled horizontal row of track cards; a card plays the shelf's list from that track.
// Hidden while the list is empty.
ColumnLayout {
    id: root

    property string title: ""
    property TrackListModel model: null

    // -1: no "See all" link.
    property int seeAllPage: -1

    visible: model !== null && model.count > 0
    spacing: 8

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 8
        Layout.rightMargin: 8

        Text {
            Layout.fillWidth: true
            text: root.title
            color: Theme.palette.textPrimary
            font.family: Theme.typography.family
            font.pixelSize: 18
            font.weight: Font.Bold
        }
        Text {
            visible: root.seeAllPage >= 0
            text: "See all"
            color: seeAllHover.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary
            font.family: Theme.typography.family
            font.pixelSize: Theme.typography.smallSize
            font.weight: Font.DemiBold

            HoverHandler { id: seeAllHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: Navigation.navigate(root.seeAllPage) }
        }
    }

    ListView {
        id: list
        Layout.fillWidth: true
        Layout.preferredHeight: 232
        orientation: ListView.Horizontal
        clip: true
        spacing: 4
        boundsBehavior: Flickable.StopAtBounds
        model: root.model

        delegate: TrackCard {
            width: 168
            height: list.height
            onClicked: PlayerViewModel.play(root.model, index)
        }

        // Shift+wheel scrolls the shelf sideways; the plain wheel keeps scrolling the page.
        WheelHandler {
            acceptedDevices: PointerDevice.Mouse
            acceptedModifiers: Qt.ShiftModifier
            orientation: Qt.Vertical
            onWheel: (event) => {
                const maxX = Math.max(0, list.contentWidth - list.width);
                list.contentX = Math.max(0, Math.min(maxX, list.contentX - event.angleDelta.y));
            }
        }
    }
}
