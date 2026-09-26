import QtQuick
import QtQuick.Controls
import SoundLink 1.0
import "Sections"
import "Components"
import "Screens"

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
    Medialib {
        id: contentArea
        anchors.top: titleBar.bottom
        anchors.bottom: playerBar.top
        anchors.left: sideBar.right
        anchors.right: parent.right

    }

    // ─── Resize handle ───────────────────────────────────
    ResizeHandle {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        z: 100
        window: root
    }

    onClosing: function(close) {
        close.accepted = false
        root.hide()
    }
}
