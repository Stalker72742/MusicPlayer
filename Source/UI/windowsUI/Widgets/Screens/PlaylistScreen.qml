import QtQuick
import SoundLink 1.0

// One playlist (Navigation.playlistId): its tracks, Play, and editing. Tracks taken out of a smart playlist
// stay hidden until restored; the rest of a smart playlist follows its rules as the library changes.
LibraryView {
    id: root

    readonly property bool smart: PlaylistsViewModel.currentSmart
    readonly property int excluded: PlaylistsViewModel.currentExcluded

    title: PlaylistsViewModel.currentName
    subtitle: (smart ? "Smart playlist · " : "Playlist · ") + (model.count === 1 ? "1 track" : model.count + " tracks")
              + (smart ? " · " + PlaylistsViewModel.currentSummary : "")
    model: PlaylistsViewModel.currentTracks
    contextPlaylistId: PlaylistsViewModel.currentId
    contextSmart: smart

    // A smart playlist's order comes from its sort.
    reorderable: !smart
    onMoveRequested: (from, to) => PlaylistsViewModel.moveTrack(PlaylistsViewModel.currentId, from, to)

    emptyIcon: smart ? "funnel" : "list"
    emptyTitle: smart ? "No tracks match the rules" : "This playlist is empty"
    emptyText: smart ? "Edit the rules, or wait for the library to grow."
                     : "Add tracks from their ⋯ menu: Add to playlist."
    emptyActionText: smart ? "Edit rules" : ""
    onEmptyActionClicked: smartDialog.openEdit(PlaylistsViewModel.currentId)

    headerActions: Row {
        spacing: 8

        TextButton {
            primary: true
            enabled: root.model.count > 0
            text: "Play"
            onClicked: PlayerViewModel.play(root.model, 0)
        }
        TextButton {
            visible: PlaylistsViewModel.currentDownloadable > 0
            text: "Download " + (PlaylistsViewModel.currentDownloadable === 1 ? "1 track" : PlaylistsViewModel.currentDownloadable + " tracks")
            onClicked: DownloadsViewModel.downloadPlaylist(PlaylistsViewModel.currentId)
        }
        TextButton {
            visible: root.smart
            text: "Edit rules"
            onClicked: smartDialog.openEdit(PlaylistsViewModel.currentId)
        }
        TextButton {
            visible: root.smart && root.excluded > 0
            text: root.excluded === 1 ? "Restore 1 hidden track" : "Restore " + root.excluded + " hidden tracks"
            onClicked: PlaylistsViewModel.restoreExcluded(PlaylistsViewModel.currentId)
        }
        TextButton {
            text: "Rename"
            onClicked: renameDialog.ask("Rename playlist", PlaylistsViewModel.currentName,
                (name) => PlaylistsViewModel.rename(PlaylistsViewModel.currentId, name))
        }
        TextButton {
            text: "Delete"
            onClicked: {
                const id = PlaylistsViewModel.currentId;
                deleteDialog.ask("Delete playlist",
                    "“" + PlaylistsViewModel.currentName + "” will be deleted. Its tracks stay in the library.",
                    "Delete", () => PlaylistsViewModel.remove(id));
            }
        }
    }

    SmartPlaylistDialog { id: smartDialog }
    TextPromptDialog {
        id: renameDialog
        acceptText: "Rename"
        placeholder: "Playlist name"
    }
    ConfirmDialog { id: deleteDialog }
}
