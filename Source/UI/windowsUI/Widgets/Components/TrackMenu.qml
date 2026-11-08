pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import SoundLink 1.0

// The "more" menu of a track: favorites, playlists, tags, the library (online tracks), the track's file
// (local tracks and downloaded copies: show, rename, delete) and its page (online tracks).
// Opened from a playlist (contextPlaylistId), it can also take the track out of that playlist.
// openPlaylistsFor() opens only the "Add to playlist" page, for several selected tracks.
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

    // A local file, or the downloaded copy of an online track.
    readonly property bool hasFile: !online || downloaded

    // The second page: the static playlists to add the tracks to.
    property bool choosingPlaylist: false
    property var playlistChoices: []
    property bool smartPlaylistsExist: false
    // What the playlist page adds: this track, or the selected tracks of openPlaylistsFor().
    property var targetIds: []
    property bool onlyPlaylists: false

    // In a list with selection: the menu offers Select.
    property bool selectable: false
    property bool rowSelected: false

    signal tracksChanged
    signal selectRequested(int trackId)

    // track: anything with trackId, liked, online and inLibrary, e.g. a TrackRow.
    function openFor(anchor, track) {
        trackId = track.trackId;
        liked = track.liked;
        online = track.online;
        inLibrary = track.inLibrary;
        downloaded = track.downloaded;
        downloadState = DownloadsViewModel.stateOf(trackId);
        downloadProgress = DownloadsViewModel.progressOf(trackId);
        targetIds = [trackId];
        onlyPlaylists = false;
        choosingPlaylist = false;
        placeAt(anchor);
        open();
    }

    // Right click: at a point of the window, for a track given by id.
    function openAt(point, id) {
        const track = LibraryViewModel.trackProperties(id);
        if (track.id === 0)
            return;
        trackId = id;
        liked = track.liked;
        online = track.online;
        inLibrary = track.inLibrary;
        downloaded = track.downloaded;
        downloadState = DownloadsViewModel.stateOf(id);
        downloadProgress = DownloadsViewModel.progressOf(id);
        targetIds = [id];
        onlyPlaylists = false;
        choosingPlaylist = false;
        placeAtPoint(point);
        open();
    }

    function openPlaylistsAt(point, ids) {
        targetIds = ids;
        onlyPlaylists = true;
        showPlaylists();
        placeAtPoint(point);
        open();
    }

    function placeAtPoint(point) {
        x = Math.max(8, Math.min(point.x, parent.width - width - 8));
        y = Math.max(8, Math.min(point.y, parent.height - implicitHeight - 8));
    }

    function openPlaylistsFor(anchor, ids) {
        targetIds = ids;
        onlyPlaylists = true;
        showPlaylists();
        placeAt(anchor, true);
        open();
    }

    // alignLeft: under the anchor's left edge (a button) rather than ending at its right edge (a "⋯").
    function placeAt(anchor, alignLeft) {
        const overlay = parent;
        const point = anchor.mapToItem(overlay, anchor.width, anchor.height);
        const left = alignLeft ? point.x - anchor.width : point.x - width;
        x = Math.max(8, Math.min(left, overlay.width - width - 8));
        y = point.y + 4 + implicitHeight > overlay.height - 8
            ? point.y - anchor.height - implicitHeight - 4
            : point.y + 4;
    }

    function askRename(id, name, error) {
        renameDialog.hint = error || "";
        renameDialog.ask("Rename file", name, (text) => {
            const problem = LibraryViewModel.renameFile(id, text);
            if (problem.length > 0)
                root.askRename(id, text, problem);
        });
    }

    function run(action) {
        close();
        action();
    }

    function showPlaylists() {
        playlistChoices = PlaylistsViewModel.staticPlaylistsForTracks(targetIds);
        smartPlaylistsExist = PlaylistsViewModel.hasSmartPlaylists();
        choosingPlaylist = true;
    }

    parent: Overlay.overlay
    width: 240
    padding: 4
    // Modal without a dimmer: a click outside only closes the menu and does not also play the row under it.
    modal: true
    dim: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    // The playlist page can be taller: keep the menu inside the window.
    onImplicitHeightChanged: if (visible) y = Math.max(8, Math.min(y, parent.height - implicitHeight - 8))

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
                text: "Properties…"
                onTriggered: root.run(() => LibraryViewModel.openProperties([root.trackId]))
            }
            MenuEntry {
                visible: root.selectable
                iconName: "select"
                text: root.rowSelected ? "Deselect" : "Select"
                onTriggered: root.run(() => root.selectRequested(root.trackId))
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

            // ─── The file ──────────────────────────────
            MenuDivider { visible: root.hasFile }

            MenuEntry {
                visible: root.hasFile
                iconName: "folder"
                text: "Show in folder"
                onTriggered: root.run(() => LibraryViewModel.showInFolder(root.trackId))
            }
            MenuEntry {
                visible: root.hasFile
                iconName: "pencil"
                text: "Rename file…"
                onTriggered: {
                    const id = root.trackId;
                    root.run(() => root.askRename(id, LibraryViewModel.fileBaseName(id), ""));
                }
            }
            MenuEntry {
                visible: root.hasFile
                iconName: "link"
                text: "Copy file path"
                onTriggered: root.run(() => LibraryViewModel.copyFilePath(root.trackId))
            }
            MenuEntry {
                visible: !root.online
                iconName: "trash"
                text: "Delete file…"
                onTriggered: {
                    const id = root.trackId;
                    root.run(() => deleteDialog.ask("Delete file",
                        "“" + LibraryViewModel.fileBaseName(id) + "” goes to the Recycle Bin and leaves the library. "
                        + "Restore it from there and the track comes back with its likes, tags and playlists.",
                        "Delete", () => {
                            const result = LibraryViewModel.removeTracks([id]);
                            if (result.failed > 0)
                                failedDialog.ask("Could not delete the file", result.error, "OK", () => {});
                        }));
                }
            }

            // ─── Artist and album pages ────────────────
            MenuDivider { visible: goArtist.visible || goAlbum.visible }

            MenuEntry {
                id: goArtist
                visible: root.trackId > 0 && CatalogViewModel.hasOfTrack("artist", root.trackId)
                iconName: "library"
                text: "Go to artist"
                onTriggered: root.run(() => CatalogViewModel.openOfTrack("artist", root.trackId))
            }
            MenuEntry {
                id: goAlbum
                visible: root.trackId > 0 && CatalogViewModel.hasOfTrack("album", root.trackId)
                iconName: "list"
                text: "Go to album"
                onTriggered: root.run(() => CatalogViewModel.openOfTrack("album", root.trackId))
            }

            // ─── The page ──────────────────────────────
            MenuDivider { visible: root.online }

            MenuEntry {
                visible: root.online
                iconName: "globe"
                text: "Go to author"
                onTriggered: root.run(() => OnlinePageViewModel.openAuthor(root.trackId))
            }
            MenuEntry {
                visible: root.online
                iconName: "external"
                text: "Open in browser"
                onTriggered: root.run(() => LibraryViewModel.openSource(root.trackId))
            }
            MenuEntry {
                visible: root.online
                iconName: "link"
                text: "Copy link"
                onTriggered: root.run(() => LibraryViewModel.copyLink(root.trackId))
            }
        }

        // ─── Add to playlist ───────────────────────────
        Column {
            visible: root.choosingPlaylist
            width: parent.width

            MenuEntry {
                visible: !root.onlyPlaylists
                iconName: "chevron-left"
                text: "Back"
                onTriggered: root.choosingPlaylist = false
            }
            MenuEntry {
                iconName: "plus"
                text: "New playlist…"
                onTriggered: {
                    const ids = root.targetIds;
                    root.run(() => newPlaylistDialog.ask("New playlist", "", (name) => {
                        PlaylistsViewModel.createPlaylist(name, ids);
                        root.tracksChanged();
                    }));
                }
            }

            MenuDivider { visible: root.playlistChoices.length > 0 || root.smartPlaylistsExist }

            Repeater {
                model: root.playlistChoices

                MenuEntry {
                    required property var modelData

                    iconName: modelData.contains ? "check" : "list"
                    text: modelData.name
                    dimmed: modelData.contains
                    onTriggered: {
                        if (modelData.contains)
                            return;
                        const id = modelData.id;
                        const ids = root.targetIds;
                        root.run(() => {
                            PlaylistsViewModel.addTracks(id, ids);
                            root.tracksChanged();
                        });
                    }
                }
            }

            // Smart playlists are not listed: they pick their tracks by rules.
            Text {
                visible: root.smartPlaylistsExist
                width: 232
                leftPadding: 10
                rightPadding: 10
                topPadding: 6
                bottomPadding: 6
                wrapMode: Text.WordWrap
                text: "Smart playlists pick tracks by their rules. To get a track into one, give it a tag the rules look for."
                color: Theme.palette.textTertiary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.smallSize
            }
        }
    }

    TextPromptDialog {
        id: newPlaylistDialog
        acceptText: "Create"
        placeholder: "Playlist name"
    }

    ConfirmDialog { id: removeDownloadDialog }
    ConfirmDialog { id: deleteDialog }
    ConfirmDialog { id: failedDialog }

    TextPromptDialog {
        id: renameDialog
        acceptText: "Rename"
        placeholder: "File name"
    }
}
