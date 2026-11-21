import QtQuick
import QtQuick.Effects
import SoundLink 1.0

// Picture of an artist (round), album or genre (rounded square); initials on a tint while there is none.
Item {
    id: root

    property url source
    property string initials: ""
    property color tint: "#3A3A40"
    property bool round: false
    property real cornerRadius: Theme.metrics.radiusMd

    readonly property real shapeRadius: round ? Math.min(width, height) / 2 : cornerRadius
    readonly property bool hasImage: image.status === Image.Ready

    Rectangle {
        anchors.fill: parent
        visible: !root.hasImage
        radius: root.shapeRadius
        color: root.tint

        Text {
            anchors.centerIn: parent
            text: root.initials
            color: Qt.rgba(1, 1, 1, 0.85)
            font.family: Theme.typography.family
            font.pixelSize: Math.max(12, root.height * 0.32)
            font.weight: Font.DemiBold
        }
    }

    Image {
        id: image
        anchors.fill: parent
        visible: false
        source: root.source
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        smooth: true
        mipmap: true
        sourceSize: Qt.size(Math.ceil(root.width * Theme.pixelRatio), Math.ceil(root.height * Theme.pixelRatio))
    }

    MultiEffect {
        anchors.fill: parent
        visible: root.hasImage
        source: image
        maskEnabled: true
        maskSource: mask
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1.0
    }

    Item {
        id: mask
        anchors.fill: parent
        visible: false
        layer.enabled: true

        Rectangle {
            anchors.fill: parent
            radius: root.shapeRadius
            color: "black"
            antialiasing: true
        }
    }
}
