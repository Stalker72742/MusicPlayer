pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import SoundLink 1.0

// The start screen: a way into search and shelves of what was played, liked and saved.
Item {
    id: root

    readonly property bool hasShelves: LibraryViewModel.recent.count > 0
                                       || LibraryViewModel.favorites.count > 0
                                       || LibraryViewModel.online.count > 0

    function greeting() {
        const hour = new Date().getHours();
        if (hour < 5)  return "Good night";
        if (hour < 12) return "Good morning";
        if (hour < 18) return "Good afternoon";
        return "Good evening";
    }

    Flickable {
        id: scroller
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight + 64
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: column
            x: 48
            y: 32
            width: scroller.width - 48 - 32
            spacing: 28

            ScreenHeader {
                Layout.leftMargin: 8
                title: root.greeting()
                subtitle: LibraryViewModel.tracks.count + " tracks in your library"
                          + (LibraryViewModel.online.count > 0 ? ", " + LibraryViewModel.online.count + " from YouTube" : "")
            }

            // ─── Search prompt ─────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                Layout.maximumWidth: 860
                Layout.leftMargin: 8
                implicitHeight: promptColumn.implicitHeight + 32
                radius: Theme.metrics.radiusXl
                color: Theme.palette.fieldBg
                border.width: 1
                border.color: promptHover.hovered ? Theme.palette.borderStrong : Theme.palette.border

                Column {
                    id: promptColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    spacing: 14

                    Row {
                        spacing: 12

                        AppIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 20; height: 20
                            name: "search"
                            color: Theme.palette.iconActive
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Find music on YouTube, in your library or jump anywhere"
                            color: Theme.palette.textPrimary
                            font.family: Theme.typography.family
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                        }
                    }

                    Flow {
                        width: parent.width
                        spacing: 8

                        Repeater {
                            model: [
                                { tag: "yt:", text: "YouTube" },
                                { tag: "lib:", text: "Library" },
                                { tag: "s:", text: "Settings" },
                                { tag: "go:", text: "Pages" }
                            ]

                            delegate: Rectangle {
                                id: chip

                                required property var modelData

                                width: chipRow.implicitWidth + 20
                                height: 28
                                radius: Theme.metrics.radiusMd
                                color: chipHover.hovered ? Theme.palette.pillSelected : Theme.palette.hoverOverlay

                                Row {
                                    id: chipRow
                                    anchors.centerIn: parent
                                    spacing: 6

                                    Text {
                                        text: chip.modelData.tag
                                        color: Theme.palette.textPrimary
                                        font.family: "Consolas"
                                        font.pixelSize: Theme.typography.smallSize
                                    }
                                    Text {
                                        text: chip.modelData.text
                                        color: Theme.palette.textSecondary
                                        font.family: Theme.typography.family
                                        font.pixelSize: Theme.typography.smallSize
                                    }
                                }

                                HoverHandler { id: chipHover; cursorShape: Qt.PointingHandCursor }
                                TapHandler { onTapped: SearchViewModel.focusSearch(chip.modelData.tag) }
                            }
                        }

                        Text {
                            height: 28
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 4
                            text: "or just type · Ctrl+K"
                            color: Theme.palette.textTertiary
                            font.family: Theme.typography.family
                            font.pixelSize: Theme.typography.smallSize
                        }
                    }
                }

                HoverHandler { id: promptHover }
                TapHandler { onTapped: SearchViewModel.focusSearch() }
            }

            // ─── Shelves ───────────────────────────────────
            TrackShelf {
                Layout.fillWidth: true
                title: "Jump back in"
                model: LibraryViewModel.recent
                seeAllPage: Navigation.Recent
            }
            TrackShelf {
                Layout.fillWidth: true
                title: "Favorites"
                model: LibraryViewModel.favorites
                seeAllPage: Navigation.Favorites
            }
            TrackShelf {
                Layout.fillWidth: true
                title: "Saved from YouTube"
                model: LibraryViewModel.online
            }

            EmptyState {
                visible: !root.hasShelves
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                iconName: "home"
                title: "Nothing here yet"
                text: "Play something from your library or search YouTube. Recently played, favorite and saved tracks show up here."
                actionText: "Search YouTube"
                onActionClicked: SearchViewModel.focusSearch("yt:")
            }
        }
    }
}
