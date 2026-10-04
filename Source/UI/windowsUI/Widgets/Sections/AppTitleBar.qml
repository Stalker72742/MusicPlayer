import QtQuick
import SoundLink 1.0

Rectangle {
    id: root

    property var window: null
    property bool maximized: false

    // Ctrl+K and SearchViewModel.focusSearch() land here.
    function focusSearch() {
        search.focusInput();
    }

    function closeSearch() {
        suggestions.close();
        search.clearFocus();
    }

    function activateSuggestion(row) {
        if (SearchViewModel.activate(row))
            closeSearch();
    }

    implicitHeight: Theme.metrics.titleBarHeight
    height: Theme.metrics.titleBarHeight
    color: Theme.palette.titleBar

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.palette.hairline
    }

    DragHandler {
        id: dragHandler
        target: null
        onActiveChanged: {
            if (active && root.window)
                root.window.startSystemMove();
        }
    }

    SearchField {
        id: search
        anchors.centerIn: parent
        width: Math.min(560, parent.width * 0.45)
        height: 30
        placeholder: "Search YouTube, your library, settings..."
        shortcutHint: "Ctrl+K"

        // Two-way with SearchViewModel.query: typing writes it, completions and clearing come back.
        text: SearchViewModel.query
        onTextEdited: (value) => SearchViewModel.query = value

        // Suggestions depend on the current screen.
        onInputFocusedChanged: if (inputFocused) SearchViewModel.refresh()
        onAccepted: root.activateSuggestion(suggestions.currentIndex)
        onUpPressed: suggestions.moveCurrent(-1)
        onDownPressed: suggestions.moveCurrent(1)
        onTabPressed: SearchViewModel.complete(suggestions.currentIndex)
        onEscapePressed: {
            if (SearchViewModel.query.length > 0 && suggestions.opened)
                SearchViewModel.query = "";
            else
                root.closeSearch();
        }
    }

    SearchSuggestions {
        id: suggestions
        parent: search
        x: 0
        y: search.height + 6
        width: search.width

        // Shown while the field has focus and there is something to suggest.
        visible: search.inputFocused && SearchViewModel.suggestions.count > 0

        onActivated: (row) => root.activateSuggestion(row)
        onClosed: search.clearFocus()
    }

    Connections {
        target: SearchViewModel
        function onFocusRequested() { root.focusSearch(); }
    }

    Row {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        spacing: 0

        TitleBarButton {
            iconName: "minimize"
            onClicked: if (root.window) root.window.showMinimized()
        }
        TitleBarButton {
            iconName: root.maximized ? "restore" : "maximize"
            onClicked: {
                if (!root.window) return;
                if (root.window.visibility === Window.Maximized)
                    root.window.showNormal();
                else
                    root.window.showMaximized();
            }
        }
        TitleBarButton {
            iconName: "close"
            isClose: true
            onClicked: if (root.window) root.window.close()
        }
    }
}
