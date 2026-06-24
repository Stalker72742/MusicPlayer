import QtQuick
import QtQuick.Layouts
import SoundLink 1.0
import "../Components"

Item {
    id: root

    property string title: "My Library"
    property int trackCount: 0
    property int albumCount: 0
    property string currentTab: "all"
    property var tracksModel: ListModel {}

    signal tabChanged(string tab)
    signal trackActivated(int index)

    // header column metrics shared with rows
    readonly property real indexCol:    60
    readonly property real albumCol:    280
    readonly property real dateCol:     160
    readonly property real durationCol: 100
    readonly property real actionsCol:  90

    // Header
    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 56
        anchors.rightMargin: 32
        anchors.topMargin: 32
        anchors.bottomMargin: 0
        spacing: 16

        // Title block
        Column {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: root.title
                color: Theme.palette.textPrimary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.titleSize
                font.weight: Font.Bold
            }
            Text {
                text: root.trackCount + " tracks · " + root.albumCount + " albums"
                color: Theme.palette.textSecondary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.bodySize
            }
        }

        // Tabs
        Row {
            spacing: 4
            Layout.topMargin: 4

            TabPill {
                text: "All"
                selected: root.currentTab === "all"
                onClicked: { root.currentTab = "all"; root.tabChanged("all") }
            }
            TabPill {
                text: "Albums"
                selected: root.currentTab === "albums"
                onClicked: { root.currentTab = "albums"; root.tabChanged("albums") }
            }
            TabPill {
                text: "Artists"
                selected: root.currentTab === "artists"
                onClicked: { root.currentTab = "artists"; root.tabChanged("artists") }
            }
            TabPill {
                text: "Genres"
                selected: root.currentTab === "genres"
                onClicked: { root.currentTab = "genres"; root.tabChanged("genres") }
            }
        }

        // Header row
        Item {
            Layout.fillWidth: true
            Layout.topMargin: 16
            height: 30

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.palette.hairline
                opacity: 0.6
            }

            Text {
                width: root.indexCol
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignHCenter
                text: "#"
                color: Theme.palette.textTertiary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.headerSize
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 1
            }
            Text {
                anchors.left: parent.left
                anchors.leftMargin: root.indexCol
                anchors.verticalCenter: parent.verticalCenter
                text: "Title"
                color: Theme.palette.textTertiary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.headerSize
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 1
            }
            Text {
                width: root.albumCol
                anchors.right: dateHdr.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 16
                elide: Text.ElideRight
                text: "Album"
                color: Theme.palette.textTertiary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.headerSize
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 1
            }
            Text {
                id: dateHdr
                width: root.dateCol
                anchors.right: durHdr.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 16
                text: "Date Added"
                color: Theme.palette.textTertiary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.headerSize
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 1
            }
            Text {
                id: durHdr
                width: root.durationCol
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: root.actionsCol + 16
                text: "Duration"
                color: Theme.palette.textTertiary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.headerSize
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 1
            }
        }

        // Tracks list
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 0
            model: root.tracksModel
            boundsBehavior: Flickable.StopAtBounds

            delegate: TrackRow {
                width: list.width
                index: model.idx
                title: model.title
                artist: model.artist
                album: model.album
                dateAdded: model.dateAdded
                duration: model.duration
                artTint: model.artTint
                playing: model.playing === true
                liked: model.liked === true

                indexColWidth: root.indexCol
                albumColWidth: root.albumCol
                dateColWidth: root.dateCol
                durationColWidth: root.durationCol
                actionsColWidth: root.actionsCol

                onDoubleClicked: root.trackActivated(index)
                onLikeClicked: root.tracksModel.setProperty(model.index, "liked", !model.liked)
            }
        }
    }
}
