pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import SoundLink 1.0

// The "more" menu of a track: favorites, playlists, tags, the library (online tracks) and the track's source.
// Opened from a playlist (contextPlaylistId), it can also take the track out of that playlist.
Popup {
    id: root

    property int trackId: 0
    property bool liked: false
    property bool online: false
    property bool inLibrary: true
    property bool downloaded: false

    // DownloadListModel.State of the track's download job, -1 without one; read when the menu opens.
    property int downloadState: -1
    property real downloadProgress: -1

    property string contextPlaylistId: ""
    property bool contextSmart: false

    // The second page: the static playlists to add the track to.
    property bool choosingPlaylist: false
    property var playlistChoices: []

    // track: anything with trackId, liked, online and inLibrary, e.g. a TrackRow.
    function openFor(anchor, track) {
        trackId = track.trackId;
        liked = track.liked;
        online = track.online;
        inLibrary = track.inLibrary;
        downloaded = track.downloaded;
        downloadState = DownloadsViewModel.stateOf(trackId);
        downloadProgress = DownloadsViewModel.progressOf(trackId);
        choosingPlaylist = false;

        const overlay = parent;
        const point = anchor.mapToItem(overlay, anchor.width, anchor.height);
        x = Math.max(8, Math.min(point.x - width, overlay.width - width - 8));
        y = point.y + 4 + implicitHeight > overlay.height - 8
            ? point.y - anchor.height - implicitHeight - 4
            : point.y + 4;
        open();
    }

    function run(action) {
        close();
        action();
    }

    function showPlaylists() {
        playlistChoices = PlaylistsViewModel.staticPlaylistsFor(trackId);
        choosingPlaylist = true;
    }

    parent: Overlay.overlay
    width: 240
    padding: 4
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    // The playlist page can be taller: keep the menu inside the window.
    onImplicitHeightChanged: if (opened) y = Math.max(8, Math.min(y, parent.height - implicitHeight - 8))

    background: Rectangle {
        color: Theme.palette.fieldBg
        radius: Theme.metrics.radiusLg
        border.width: 1
        border.color: Theme.palette.borderStrong
    }

    contentItem: Column {
        spacing: 0

        // ─── Main page ─────────────────────────────────
        Column {
            visible: !root.choosingPlaylist
            width: parent.width

            MenuEntry {
                iconName: root.liked ? "heart-filled" : "heart"
                text: root.liked ? "Remove from favorites" : (root.online && !root.inLibrary ? "Like and add to library" : "Add to favorites")
                onTriggered: root.run(() => LibraryViewModel.toggleLiked(root.trackId))
            }
            MenuEntry {
                iconName: "list"
                text: "Add to playlist"
                trailing: "chevron-right"
                onTriggered: root.showPlaylists()
            }
            MenuEntry {
                visible: root.contextPlaylistId.length > 0
                iconName: "close"
                text: root.contextSmart ? "Hide from this smart playlist" : "Remove from this playlist"
                onTriggered: root.run(() => PlaylistsViewModel.removeTrack(root.contextPlaylistId, root.trackId))
            }
            MenuEntry {
                iconName: "tag"
                text: "Edit tags…"
                onTriggered: root.run(() => labelsDialog.ask("Tags", LibraryViewModel.labelsText(root.trackId),
                    (text) => LibraryViewModel.setLabelsText(root.trackId, text)))
            }
            MenuEntry {
                readonly property bool busy: root.downloadState === DownloadListModel.Queued
                                             || root.downloadState === DownloadListModel.Downloading

                visible: root.online && !root.downloaded
                iconName: busy ? "close" : "download"
                text: !busy ? "Download"
                    : root.downloadState === DownloadListModel.Queued ? "Cancel download (waiting)"
                    : "Cancel download" + (root.downloadProgress >= 0 ? " (" + Math.round(root.downloadProgress * 100) + "%)" : "")
                onTriggered: root.run(() => busy ? DownloadsViewModel.cancel(root.trackId) : DownloadsViewModel.download(root.trackId))
            }
            MenuEntry {
                visible: root.online && root.downloaded
                iconName: "trash"
                text: "Remove download"
                onTriggered: root.run(() => removeDownloadDialog.ask("Remove download",
                    "The downloaded file will be deleted. The track stays and plays from YouTube again.",
                    "Remove", () => DownloadsViewModel.removeDownload(root.trackId)))
            }
            MenuEntry {
                visible: root.online && !root.inLibrary
                iconName: "plus"
                text: "Add to library"
                onTriggered: root.run(() => LibraryViewModel.setInLibrary(root.trackId, true))
            }
            MenuEntry {
                visible: root.online && root.inLibrary
                iconName: "trash"
                text: "Remove from library"
                onTriggered: root.run(() => LibraryViewModel.setInLibrary(root.trackId, false))
            }

            Separator {}

            MenuEntry {
                iconName: root.online ? "external" : "folder"
                text: root.online ? "Open in browser" : "Show in folder"
                onTriggered: root.run(() => LibraryViewModel.openSource(root.trackId))
            }
            MenuEntry {
                iconName: "link"
                text: root.online ? "Copy link" : "Copy file path"
                onTriggered: root.run(() => LibraryViewModel.copyLink(root.trackId))
            }
        }

        // ─── Add to playlist ───────────────────────────
        Column {
            visible: root.choosingPlaylist
            width: parent.width

            MenuEntry {
                iconName: "chevron-left"
                text: "Back"
                onTriggered: root.choosingPlaylist = false
            }
            MenuEntry {
                iconName: "plus"
                text: "New playlist…"
                onTriggered: root.run(() => newPlaylistDialog.ask("New playlist", "", (name) => {
                    PlaylistsViewModel.createPlaylist(name, [root.trackId]);
                }))
            }

            Separator { visible: root.playlistChoices.length > 0 }

            Repeater {
                model: root.playlistChoices

                MenuEntry {
                    required property var modelData

                    iconName: modelData.contains ? "check" : "list"
                    text: modelData.name
                    dimmed: modelData.contains
                    onTriggered: {
                        if (!modelData.contains)
                            root.run(() => PlaylistsViewModel.addTrack(modelData.id, root.trackId));
                    }
                }
            }
        }
    }

    TextPromptDialog {
        id: newPlaylistDialog
        acceptText: "Create"
        placeholder: "Playlist name"
    }

    ConfirmDialog { id: removeDownloadDialog }

    TextPromptDialog {
        id: labelsDialog
        acceptText: "Save"
        allowEmpty: true
        placeholder: "chill, gym, 2000s"
        hint: "Comma-separated. Smart playlists can pick tracks by tag."
    }

    component Separator: Item {
        width: 232
        height: visible ? 9 : 0

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            height: 1
            color: Theme.palette.hairline
        }
    }

    component MenuEntry: Rectangle {
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
}
