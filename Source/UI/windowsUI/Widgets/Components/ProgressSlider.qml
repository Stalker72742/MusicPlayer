import QtQuick
import SoundLink 1.0

Item {
    id: root

    property real value: 0.0    // 0..1
    property real trackHeight: 3
    property color trackColor: Theme.palette.sliderTrack
    property color fillColor: Theme.palette.sliderFill
    property color handleColor: Theme.palette.sliderHandle
    property bool  showHandle: hovered || dragging
    property bool  hovered: hover.hovered
    property bool  dragging: drag.active

    signal moved(real value)

    implicitHeight: 14

    Rectangle {
        id: track
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.right: parent.right
        height: root.trackHeight
        radius: height / 2
        color: root.trackColor
    }

    Rectangle {
        id: fill
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: track.left
        height: root.trackHeight
        radius: height / 2
        width: track.width * Math.max(0, Math.min(1, root.value))
        color: root.fillColor
    }

    Rectangle {
        id: handle
        anchors.verticalCenter: parent.verticalCenter
        x: track.x + track.width * Math.max(0, Math.min(1, root.value)) - width / 2
        width: 12
        height: 12
        radius: 6
        color: root.handleColor
        opacity: root.showHandle ? 1 : 0
        scale: root.showHandle ? 1 : 0.6

        Behavior on opacity { NumberAnimation { duration: Theme.motion.durationFast } }
        Behavior on scale { NumberAnimation { duration: Theme.motion.durationFast } }
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }

    TapHandler {
        onTapped: (eventPoint) => {
            const v = Math.max(0, Math.min(1, eventPoint.position.x / root.width));
            root.value = v;
            root.moved(v);
        }
    }

    DragHandler {
        id: drag
        target: null
        onActiveTranslationChanged: {
            // not used directly; we use centroid below
        }
        onCentroidChanged: {
            if (active) {
                const v = Math.max(0, Math.min(1, centroid.position.x / root.width));
                root.value = v;
                root.moved(v);
            }
        }
    }
}
