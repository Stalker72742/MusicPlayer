import QtQuick
import QtQuick.Controls
import SoundLink 1.0
import "Sections"
import "Components"

ApplicationWindow {
    id: root

    title: "SoundLink"
    width: 1600
    height: 900
    minimumWidth: 720
    minimumHeight: 460
    visible: true
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint

    // ─── Background gradient layer ───────────────────────
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.palette.windowTop }
            GradientStop { position: 1.0; color: Theme.palette.windowBottom }
        }
    }

    // ─── Title bar ───────────────────────────────────────
    AppTitleBar {
        id: titleBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        window: root
        maximized: root.visibility === Window.Maximized
        onMenuClicked: sideBar.expanded = !sideBar.expanded
    }

    // ─── Bottom player bar ───────────────────────────────
    PlayerBar {
        id: playerBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        title: "Blinding Lights"
        artist: "The Weeknd"
        artTint: "#3A1A1A"
        playing: false
        liked: false
        progress: 2.0 * 60 / (3 * 60 + 22)
        elapsedText: "2:44"
        totalText: "3:22"
        volume: 0.8

        onTogglePlay: playing = !playing
        onToggleLike: liked = !liked
        onToggleShuffle: shuffled = !shuffled
        onToggleRepeat: repeating = !repeating
    }

    // ─── Side bar ────────────────────────────────────────
    SideBar {
        id: sideBar
        anchors.top: titleBar.bottom
        anchors.left: parent.left
        anchors.bottom: playerBar.top
        expanded: true
        onToggleRequested: expanded = !expanded
    }

    // ─── Main content ────────────────────────────────────
    Item {
        id: contentArea
        anchors.top: titleBar.bottom
        anchors.bottom: playerBar.top
        anchors.left: sideBar.right
        anchors.right: parent.right

        LibraryView {
            anchors.fill: parent
            title: "My Library"
            trackCount: tracks.count
            albumCount: 3
            tracksModel: tracks
        }
    }

    // ─── Resize handle ───────────────────────────────────
    ResizeHandle {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        z: 100
        window: root
    }

    // ─── Demo data ───────────────────────────────────────
    ListModel {
        id: tracks
        ListElement { idx: 1; title: "Blinding Lights";   artist: "The Weeknd";              album: "After Hours";              dateAdded: "Jan 14, 2026"; duration: "3:22"; artTint: "#3A1A1A"; playing: true;  liked: false }
        ListElement { idx: 2; title: "Starboy";           artist: "The Weeknd ft. Daft Punk"; album: "Starboy";                  dateAdded: "Jan 14, 2026"; duration: "3:50"; artTint: "#1A1F3A"; playing: false; liked: false }
        ListElement { idx: 3; title: "Numb";              artist: "Linkin Park";             album: "Meteora";                  dateAdded: "Dec 28, 2025"; duration: "3:07"; artTint: "#1F2E1F"; playing: false; liked: false }
        ListElement { idx: 4; title: "In The End";       artist: "Linkin Park";             album: "Hybrid Theory";            dateAdded: "Dec 28, 2025"; duration: "3:36"; artTint: "#3B3A1A"; playing: false; liked: false }
        ListElement { idx: 5; title: "The Hills";        artist: "The Weeknd";              album: "Beauty Behind the Madness"; dateAdded: "Jan 14, 2026"; duration: "3:55"; artTint: "#2A1A3A"; playing: false; liked: false }
        ListElement { idx: 6; title: "Breaking The Habit"; artist: "Linkin Park";          album: "Meteora";                  dateAdded: "Dec 28, 2025"; duration: "3:16"; artTint: "#1A2B3A"; playing: false; liked: false }
        ListElement { idx: 7; title: "Save Your Tears";   artist: "The Weeknd";              album: "After Hours";              dateAdded: "Jan 14, 2026"; duration: "3:35"; artTint: "#3A1A1F"; playing: false; liked: false }
        ListElement { idx: 8; title: "Faint";             artist: "Linkin Park";             album: "Meteora";                  dateAdded: "Dec 28, 2025"; duration: "2:42"; artTint: "#2D3A1A"; playing: false; liked: false }
    }
}
