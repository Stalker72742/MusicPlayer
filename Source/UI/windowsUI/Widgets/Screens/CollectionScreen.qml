import QtQuick
import QtQuick.Dialogs
import SoundLink 1.0

// An artist, album or genre (Navigation.collectionKind / collectionKey): its picture and tracks.
// The picture is a file in the artwork folder; it can be replaced, removed or looked up online.
LibraryView {
    id: root

    readonly property string kind: CatalogViewModel.currentKind
    property string imageError: ""

    title: CatalogViewModel.currentName
    subtitle: CatalogViewModel.currentSubtitle
    model: CatalogViewModel.currentTracks
    albumHeader: "Album"

    emptyIcon: "library"
    emptyTitle: kind.length > 0 ? "No tracks" : "Nothing here anymore"
    emptyText: kind.length > 0 ? "" : "Its tracks have left the library."

    headerArt: CatalogArt {
        width: 160
        height: 160
        round: root.kind === "artist"
        source: CatalogViewModel.currentImage
        initials: CatalogViewModel.currentInitials
        tint: CatalogViewModel.currentTint

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            onClicked: (mouse) => groupMenu.openAt(parent.mapToItem(null, mouse.x, mouse.y),
                Navigation.collectionKind, Navigation.collectionKey, false)
        }
    }

    headerActions: Column {
        spacing: 8

        Row {
            spacing: 8

            TextButton {
                primary: true
                enabled: root.model.count > 0
                text: "Play"
                onClicked: PlayerViewModel.play(root.model, 0)
            }
            TextButton {
                text: "Choose image…"
                onClicked: imageDialog.open()
            }
            TextButton {
                enabled: !CatalogViewModel.currentFetching
                text: CatalogViewModel.currentFetching ? "Searching…" : "Find image online"
                onClicked: {
                    root.imageError = "";
                    CatalogViewModel.findImageOnline();
                }
            }
            TextButton {
                visible: CatalogViewModel.currentImage.toString().length > 0
                text: "Remove image"
                onClicked: CatalogViewModel.removeImage()
            }
            TextButton {
                text: "Show in folder"
                onClicked: CatalogViewModel.showImageInFolder()
            }
        }

        Text {
            visible: root.imageError.length > 0
            text: root.imageError
            color: Theme.palette.danger
            font.family: Theme.typography.family
            font.pixelSize: Theme.typography.smallSize
        }
    }

    CatalogMenu { id: groupMenu }

    FileDialog {
        id: imageDialog
        title: "Choose an image for " + CatalogViewModel.currentName
        nameFilters: ["Images (*.jpg *.jpeg *.png *.webp *.svg *.gif *.bmp)"]
        onAccepted: root.imageError = CatalogViewModel.setImage(selectedFile)
    }
}
