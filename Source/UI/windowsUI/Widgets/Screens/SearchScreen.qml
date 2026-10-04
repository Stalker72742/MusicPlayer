import QtQuick
import SoundLink 1.0

// YouTube results of the last online search. A click plays one track; the heart or "Add to library"
// keeps it in the library (its page URL is saved, the stream is found again on every play).
LibraryView {
    id: root

    readonly property int searchState: SearchViewModel.onlineState
    readonly property string query: SearchViewModel.onlineQuery

    title: "YouTube"
    subtitle: query.length === 0 ? "Search from the bar above"
            : (searchState === SearchViewModel.Searching ? "Searching for “" + query + "”…"
                                                    : model.count + " results for “" + query + "”"
                                                      + (OnlineViewModel.lastSearchBackend.length > 0 ? " · via " + OnlineViewModel.lastSearchBackend : ""))
    model: SearchViewModel.results
    singleTrackPlayback: true
    loadingMore: SearchViewModel.loadingMore
    onEndReached: SearchViewModel.loadMore()
    albumHeader: "Channel"
    dateHeader: ""

    emptyIcon: searchState === SearchViewModel.Failed ? "info" : "globe"
    emptyTitle: searchState === SearchViewModel.Searching ? "Searching YouTube…"
              : searchState === SearchViewModel.Failed ? "Search failed"
              : searchState === SearchViewModel.Done ? "Nothing found"
              : "Search YouTube"
    emptyText: searchState === SearchViewModel.Failed ? SearchViewModel.onlineError
             : searchState === SearchViewModel.Idle ? "Type in the search bar and pick “Search YouTube”, or start with yt:"
             : ""
    emptyActionText: searchState === SearchViewModel.Failed ? "Try again"
                   : searchState === SearchViewModel.Idle ? "Search YouTube" : ""
    onEmptyActionClicked: {
        if (searchState === SearchViewModel.Failed)
            SearchViewModel.retryOnline();
        else
            SearchViewModel.focusSearch("yt:");
    }
}
