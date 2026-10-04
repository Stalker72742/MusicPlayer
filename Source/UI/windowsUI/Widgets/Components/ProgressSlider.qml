import QtQuick
import SoundLink 1.0

// Never writes `value` itself, so a binding to a model stays intact; the owner reacts to moved().
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

    // While dragging, show where the pointer is even if the owner applies moved() with a delay.
    property real dragValue: 0.0
    readonly property real shownValue: Math.max(0, Math.min(1, dragging ? dragValue : value))

    signal moved(real value)

    implicitHeight: 14

    function valueAt(x) {
        return Math.max(0, Math.min(1, x / root.width));
    }

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
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: track.left
        height: root.trackHeight
        radius: height / 2
        width: track.width * root.shownValue
        color: root.fillColor
    }

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        x: track.x + track.width * root.shownValue - width / 2
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
        onTapped: (eventPoint) => root.moved(root.valueAt(eventPoint.position.x))
    }

    DragHandler {
        id: drag
        target: null
        onCentroidChanged: {
            if (active) {
                root.dragValue = root.valueAt(centroid.position.x);
                root.moved(root.dragValue);
            }
        }
    }
}
