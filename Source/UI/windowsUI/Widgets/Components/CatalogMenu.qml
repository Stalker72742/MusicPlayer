import QtQuick
import QtQuick.Dialogs
import SoundLink 1.0

// The right-click menu of an artist, album or genre: its tracks and its picture.
Item {
    id: root

    property string kind: ""
    property string key: ""

    function openAt(point, groupKind, groupKey, withOpen) {
        kind = groupKind;
        key = groupKey;

        const k = groupKind;
        const g = groupKey;
        menu.openAt(point, [
            { text: "Play", icon: "play", action: () => CatalogViewModel.playGroup(k, g) },
            withOpen ? { text: "Open", icon: "library", action: () => CatalogViewModel.open(k, g) } : null,
            { text: "Add to playlist", icon: "list", action: () => tracks.openPlaylistsAt(point, CatalogViewModel.trackIdsOf(k, g)) },
            { text: "Properties of all tracks…", icon: "tag", action: () => LibraryViewModel.openProperties(CatalogViewModel.trackIdsOf(k, g)) },
            { separator: true },
            { text: "Choose image…", icon: "folder", action: () => imageDialog.open() },
            { text: "Find image online", icon: "globe", action: () => CatalogViewModel.findImageOnlineOf(k, g) },
            CatalogViewModel.hasImageOf(k, g) ? { text: "Remove image", icon: "trash", action: () => CatalogViewModel.removeImageOf(k, g) } : null,
            { text: "Show image in folder", icon: "external", action: () => CatalogViewModel.showImageInFolderOf(k, g) }
        ]);
    }

    ContextMenu { id: menu }
    TrackMenu { id: tracks }
    ConfirmDialog { id: errorDialog }

    FileDialog {
        id: imageDialog
        title: "Choose an image for " + CatalogViewModel.nameOf(root.kind, root.key)
        nameFilters: ["Images (*.jpg *.jpeg *.png *.webp *.svg *.gif *.bmp)"]
        onAccepted: {
            const problem = CatalogViewModel.setImageOf(root.kind, root.key, selectedFile);
            if (problem.length > 0)
                errorDialog.ask("Could not set the image", problem, "OK", () => {});
        }
    }
}
