import QtQuick
import SoundLink 1.0

Rectangle {
    id: root

    property color tint: "#3A3A40"
    property string source: ""
    property real cornerRadius: Theme.metrics.radiusSm

    implicitWidth: 36
    implicitHeight: 36
    radius: cornerRadius
    color: tint
    clip: true

    Image {
        anchors.fill: parent
        source: root.source
        fillMode: Image.PreserveAspectCrop
        visible: source !== ""
        smooth: true
    }
}
