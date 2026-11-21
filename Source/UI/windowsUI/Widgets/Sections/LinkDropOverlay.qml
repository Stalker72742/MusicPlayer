pragma ComponentBehavior: Bound

import QtQuick
import SoundLink 1.0

// A YouTube link dragged from a browser over any part of the window: drop it on what should happen to it.
// Dropping outside the cards just opens the link. Anything else being dragged passes through.
DropArea {
    id: root

    // OnlinePageViewModel.linkKind() of what is being dragged, empty when it is not a link.
    property string kind: ""
    property int hoveredZone: 0

    readonly property var zones: {
        if (kind === "video")
            return [
                { title: "Listen", text: "Open the track", icon: "play", action: OnlinePageViewModel.OpenOnly },
                { title: "Add to library", text: "Keep it, streamed when played", icon: "plus", action: OnlinePageViewModel.AddToLibrary },
                { title: "Download", text: "Save the file", icon: "download", action: OnlinePageViewModel.Download }
            ];
        if (kind === "playlist")
            return [
                { title: "Open", text: "See the tracks, pick some", icon: "list", action: OnlinePageViewModel.OpenOnly },
                { title: "Add all to library", text: "Keep every track", icon: "plus", action: OnlinePageViewModel.AddToLibrary },
                { title: "Download all", text: "Save every file", icon: "download", action: OnlinePageViewModel.Download }
            ];
        return [{ title: "Open author", text: "See the tracks, pick some", icon: "globe", action: OnlinePageViewModel.OpenOnly }];
    }

    function linkText(event) {
        if (event.hasUrls && event.urls.length > 0)
            return event.urls[0].toString();
        return event.hasText ? event.text : "";
    }

    function updateZone(x, y) {
        for (let i = 0; i < zoneRepeater.count; ++i) {
            const card = zoneRepeater.itemAt(i);
            const point = card.mapFromItem(root, x, y);
            if (card.contains(point)) {
                hoveredZone = i;
                return;
            }
        }
        hoveredZone = 0;
    }

    onEntered: (drag) => {
        kind = OnlinePageViewModel.linkKind(linkText(drag));
        if (kind.length === 0)
            drag.accepted = false;
        else
            drag.acceptProposedAction();
    }
    onPositionChanged: (drag) => updateZone(drag.x, drag.y)
    onExited: kind = ""
    onDropped: (drop) => {
        updateZone(drop.x, drop.y);
        const text = linkText(drop);
        const zone = zones[Math.min(hoveredZone, zones.length - 1)];
        kind = "";
        drop.acceptProposedAction();
        OnlinePageViewModel.openLink(text, zone.action);
    }

    Rectangle {
        anchors.fill: parent
        visible: root.kind.length > 0
        color: Theme.palette.windowBottom
        opacity: 0.92
    }

    Row {
        anchors.centerIn: parent
        visible: root.kind.length > 0
        spacing: 16

        Repeater {
            id: zoneRepeater
            model: root.zones

            Rectangle {
                id: card

                required property var modelData
                required property int index

                readonly property bool active: root.hoveredZone === index

                width: 220
                height: 150
                radius: Theme.metrics.radiusLg
                color: active ? Theme.palette.pillSelected : Theme.palette.fieldBg
                border.width: 1
                border.color: active ? Theme.palette.accent : Theme.palette.border

                Column {
                    anchors.centerIn: parent
                    width: parent.width - 24
                    spacing: 8

                    AppIcon {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 28; height: 28
                        name: card.modelData.icon
                        color: card.active ? Theme.palette.iconActive : Theme.palette.iconDefault
                    }
                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        text: card.modelData.title
                        color: Theme.palette.textPrimary
                        font.family: Theme.typography.family
                        font.pixelSize: Theme.typography.bodySize
                        font.weight: Font.DemiBold
                    }
                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: card.modelData.text
                        color: Theme.palette.textSecondary
                        font.family: Theme.typography.family
                        font.pixelSize: Theme.typography.smallSize
                    }
                }
            }
        }
    }
}
