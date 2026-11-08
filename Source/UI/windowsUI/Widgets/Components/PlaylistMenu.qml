import QtQuick
import SoundLink 1.0

// The right-click menu of a playlist (the sidebar, the playlist cards) and of the Playlists section.
Item {
    id: root

    function openAt(point, id) {
        const info = PlaylistsViewModel.infoOf(id);
        if (!info.exists)
            return;

        const name = PlaylistsViewModel.nameOf(id);
        menu.openAt(point, [
            info.count > 0 ? { text: "Play", icon: "play", action: () => PlaylistsViewModel.play(id) } : null,
            { text: "Open", icon: "list", action: () => Navigation.openPlaylist(id) },
            { separator: true },
            info.downloadable > 0
                ? { text: "Download " + (info.downloadable === 1 ? "1 track" : info.downloadable + " tracks"), icon: "download",
                    action: () => DownloadsViewModel.downloadPlaylist(id) }
                : null,
            info.count > 0
                ? { text: "Properties of all tracks…", icon: "tag",
                    action: () => LibraryViewModel.openProperties(PlaylistsViewModel.trackIdsOf(id)) }
                : null,
            info.smart ? { text: "Edit rules…", icon: "funnel", action: () => smartDialog.openEdit(id) } : null,
            info.excluded > 0
                ? { text: info.excluded === 1 ? "Restore 1 hidden track" : "Restore " + info.excluded + " hidden tracks", icon: "plus",
                    action: () => PlaylistsViewModel.restoreExcluded(id) }
                : null,
            { text: "Rename…", icon: "pencil",
              action: () => renameDialog.ask("Rename playlist", name, (text) => PlaylistsViewModel.rename(id, text)) },
            { separator: true },
            { text: "Delete…", icon: "trash",
              action: () => deleteDialog.ask("Delete playlist", "“" + name + "” will be deleted. Its tracks stay in the library.",
                  "Delete", () => PlaylistsViewModel.remove(id)) }
        ]);
    }

    // The Playlists section itself.
    function openSectionAt(point) {
        menu.openAt(point, [
            { text: "New playlist…", icon: "plus",
              action: () => newDialog.ask("New playlist", "", (name) => PlaylistsViewModel.open(PlaylistsViewModel.createPlaylist(name))) },
            { text: "New smart playlist…", icon: "funnel", action: () => smartDialog.openNew() },
            { separator: true },
            { text: "All playlists", icon: "list", action: () => Navigation.navigate(Navigation.Playlists) }
        ]);
    }

    ContextMenu { id: menu }
    SmartPlaylistDialog { id: smartDialog }
    ConfirmDialog { id: deleteDialog }
    TextPromptDialog {
        id: renameDialog
        acceptText: "Rename"
        placeholder: "Playlist name"
    }
    TextPromptDialog {
        id: newDialog
        acceptText: "Create"
        placeholder: "Playlist name"
    }
}
