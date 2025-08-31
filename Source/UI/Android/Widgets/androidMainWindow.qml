import QtQuick
import QtQuick.Controls
import QtMultimedia

ApplicationWindow {

    property var playlist: ["file:///storage/emulated/0/Music/Primorose.mp3",
        "file:///storage/emulated/0/Music/Gasoline.mp3", "file:///storage/emulated/0/Music/Judas.mp3"]
    property int currentIndex: 0
    width: 1080
    height: 1920
    visible: true
    color: "#fff"

    Column {
            anchors.centerIn: parent
            spacing: 16

            Button {
                text: "▶ Play / ⏸ Pause"
                background: Rectangle {
                    radius: 8
                    color: "#000"
                }
                width: 150
                height: 150
                contentItem: Text {
                    text: parent.Button.text
                    color: "white"
                    font.pixelSize: 18
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    player.play()
                }
            }

            Button {
                text: "⏭ Next"
                background: Rectangle {
                    radius: 8
                    color: "#4CAF50"
                }
                contentItem: Text {
                    text: parent.Button.text
                    color: "white"
                    font.pixelSize: 18
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    currentIndex = (currentIndex + 1) % playlist.length
                    player.source = playlist[currentIndex]
                    player.play()
                }
            }

            Button {
                text: "⏮ Previous"
                background: Rectangle {
                    radius: 8
                    color: "#F44336"
                }
                contentItem: Text {
                    text: parent.Button.text
                    color: "white"
                    font.pixelSize: 18
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    currentIndex = (currentIndex - 1 + playlist.length) % playlist.length
                    player.source = playlist[currentIndex]
                    player.play()
                }
            }
    }

    MediaPlayer {
            id: player
            source: playlist[currentIndex]
            audioOutput: AudioOutput { id: audioOut; volume: 1.0 }
            onPlaybackStateChanged: {
                if (playbackState === MediaPlayer.StoppedState) {
                    currentIndex = (currentIndex + 1) % playlist.length
                    source = playlist[currentIndex]
                    play()
                }
            }
        }
}
