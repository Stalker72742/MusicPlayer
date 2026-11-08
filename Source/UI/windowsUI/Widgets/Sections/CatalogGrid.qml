pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import SoundLink 1.0

// Artists, albums or genres of the library as cards; a click opens the group's page.
Item {
    id: root

    // "artist", "album" or "genre".
    property string kind: "artist"

    readonly property CatalogListModel entries: kind === "album" ? CatalogViewModel.albums
                                              : kind === "genre" ? CatalogViewModel.genres
                                              : CatalogViewModel.artists

    GridView {
        id: grid
        anchors.fill: parent
        anchors.topMargin: 8
        visible: count > 0
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: AppScrollBar {}
        cacheBuffer: 400
        model: root.entries

        // As many columns of at least 168 px as fit, stretched to the full width.
        readonly property int columns: Math.max(1, Math.floor(width / 168))
        cellWidth: Math.floor(width / columns)
        cellHeight: cellWidth + 58

        delegate: Item {
            id: card

            required property string key
            required property string kind
            required property string name
            required property string subtitle
            required property url imageUrl
            required property string initials
            required property color tint

            width: grid.cellWidth
            height: grid.cellHeight

            Rectangle {
                anchors.fill: parent
                anchors.margins: 6
                radius: Theme.metrics.radiusLg
                color: hover.hovered ? Theme.palette.hoverOverlay : "transparent"

                Behavior on color { ColorAnimation { duration: Theme.motion.durationFast } }

                Column {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    CatalogArt {
                        width: parent.width
                        height: width
                        round: card.kind === "artist"
                        source: card.imageUrl
                        initials: card.initials
                        tint: card.tint
                    }
                    Text {
                        width: parent.width
                        horizontalAlignment: card.kind === "artist" ? Text.AlignHCenter : Text.AlignLeft
                        elide: Text.ElideRight
                        text: card.name
                        color: Theme.palette.textPrimary
                        font.family: Theme.typography.family
                        font.pixelSize: Theme.typography.bodySize
                        font.weight: Font.DemiBold
                    }
                    Text {
                        width: parent.width
                        horizontalAlignment: card.kind === "artist" ? Text.AlignHCenter : Text.AlignLeft
                        elide: Text.ElideRight
                        text: card.subtitle
                        color: Theme.palette.textSecondary
                        font.family: Theme.typography.family
                        font.pixelSize: Theme.typography.smallSize
                    }
                }

                HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: (mouse) => {
                        if (mouse.button === Qt.RightButton)
                            groupMenu.openAt(card.mapToItem(null, mouse.x + 6, mouse.y + 6), card.kind, card.key, true);
                        else
                            CatalogViewModel.open(card.kind, card.key);
                    }
                }
            }
        }
    }

    CatalogMenu { id: groupMenu }

    EmptyState {
        anchors.fill: parent
        visible: grid.count === 0
        iconName: "library"
        title: root.kind === "album" ? "No albums yet" : root.kind === "genre" ? "No genres yet" : "No artists yet"
        text: "They come from the tags of your tracks."
    }
}
