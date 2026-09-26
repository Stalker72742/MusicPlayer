import QtQuick
import QtQuick.Controls
import SoundLink 1.0
import "../Sections"

Item {
    id: root

    LibraryView {
        anchors.fill: parent
        title: "My library"
        trackCount: trackModel.count
        albumCount: 3
        tracksModel: trackModel
    }

    MedialibModel {
        id: trackModel
    }
}

