import QtQuick
import SoundLink 1.0

// A YouTube video, public playlist or channel opened by link: listen, save, download everything or what is selected.
LibraryView {
    id: root

    readonly property int pageState: OnlinePageViewModel.state
    readonly property string kind: OnlinePageViewModel.kind
    readonly property bool isList: kind !== "video"
    readonly property string pageUrl: OnlinePageViewModel.url

    // What is shown after a bulk action.
    property string notice: ""

    function actionIds() {
        return selectedCount > 0 ? Object.keys(selectedIds).map(Number) : OnlinePageViewModel.allTrackIds();
    }

    function actionLabel(verb) {
        if (selectedCount > 0)
            return verb + " " + selectedCount;
        return isList ? verb + " all" : verb;
    }

    function startDownload() {
        const count = OnlinePageViewModel.download(actionIds());
        notice = count > 0 ? "Queued " + count + (count === 1 ? " track" : " tracks") + " for download"
                           : "Everything here is already downloaded or queued";
    }

    onPageUrlChanged: {
        clearSelection();
        notice = "";
    }

    title: OnlinePageViewModel.title.length > 0 ? OnlinePageViewModel.title
         : pageState === OnlinePageViewModel.Loading ? "Opening…" : "YouTube"
    subtitle: {
        const parts = [];
        if (isList)
            parts.push(model.count + (model.count === 1 ? " track" : " tracks") + (OnlinePageViewModel.limitReached ? " (the newest ones)" : ""));
        if (kind !== "channel" && OnlinePageViewModel.channel.length > 0)
            parts.push(OnlinePageViewModel.channel);
        if (pageState === OnlinePageViewModel.Loading)
            parts.push("Loading…");
        if (pageState === OnlinePageViewModel.Failed && model.count > 0)
            parts.push("Stopped: " + OnlinePageViewModel.error);
        return parts.join(" · ");
    }

    model: OnlinePageViewModel.tracks
    clickToPlay: true
    // The page has its own Download, Add to library and Select acting on the selection.
    bulkActions: false
    albumHeader: "Channel"
    dateHeader: "Added"

    emptyIcon: pageState === OnlinePageViewModel.Failed ? "info" : "globe"
    emptyTitle: pageState === OnlinePageViewModel.Failed ? "Could not open this link"
              : pageState === OnlinePageViewModel.Loading ? "Opening…" : "Nothing here"
    emptyText: pageState === OnlinePageViewModel.Failed ? OnlinePageViewModel.error : ""
    emptyActionText: pageState === OnlinePageViewModel.Failed ? "Try again" : ""
    onEmptyActionClicked: OnlinePageViewModel.retry()

    headerActions: Component {
        Column {
            spacing: 8

            Flow {
                width: parent.width
                spacing: 8

                TextButton {
                    primary: true
                    text: "Play"
                    enabled: root.model.count > 0
                    onClicked: PlayerViewModel.play(root.model, 0)
                }
                TextButton {
                    text: root.actionLabel("Download")
                    enabled: root.model.count > 0
                    onClicked: {
                        const count = root.actionIds().length;
                        if (count > 50)
                            confirmDownload.ask("Download " + count + " tracks?", "They go to the download queue, two at a time.",
                                "Download", () => root.startDownload());
                        else
                            root.startDownload();
                    }
                }
                TextButton {
                    text: root.selectedCount > 0 ? "Add " + root.selectedCount + " to library" : (root.isList ? "Add all to library" : "Add to library")
                    enabled: root.model.count > 0
                    onClicked: {
                        const count = OnlinePageViewModel.addToLibrary(root.actionIds());
                        root.notice = count > 0 ? "Added " + count + (count === 1 ? " track" : " tracks") + " to the library"
                                                : "Already in the library";
                    }
                }
                TextButton {
                    visible: root.isList
                    text: "Save as playlist"
                    enabled: root.model.count > 0 && root.pageState !== OnlinePageViewModel.Loading
                    onClicked: playlistDialog.ask("Save as playlist", OnlinePageViewModel.title,
                        (name) => OnlinePageViewModel.saveAsPlaylist(name, root.actionIds()))
                }
                TextButton {
                    visible: root.isList
                    text: root.selecting ? "Done selecting" : "Select"
                    enabled: root.model.count > 0
                    onClicked: root.selecting ? root.clearSelection() : root.selectionMode = true
                }
                TextButton {
                    visible: root.selecting
                    text: root.selectedCount === root.model.count ? "Select none" : "Select all"
                    onClicked: root.selectedCount === root.model.count ? root.selectedIds = {} : root.selectAll()
                }
                TextButton {
                    visible: root.kind !== "channel" && OnlinePageViewModel.channelUrl.length > 0
                    text: "Go to author"
                    onClicked: OnlinePageViewModel.openChannel()
                }
                TextButton {
                    visible: OnlinePageViewModel.playlistUrl.length > 0
                    text: "Open the playlist"
                    onClicked: OnlinePageViewModel.openPlaylist()
                }
                TextButton {
                    text: "Open in browser"
                    onClicked: OnlinePageViewModel.openInBrowser()
                }
            }

            Text {
                width: parent.width
                visible: root.notice.length > 0
                elide: Text.ElideRight
                text: root.notice
                color: Theme.palette.textSecondary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.smallSize
            }
        }
    }

    TextPromptDialog {
        id: playlistDialog
        acceptText: "Create"
        placeholder: "Playlist name"
    }

    ConfirmDialog { id: confirmDownload }
}
