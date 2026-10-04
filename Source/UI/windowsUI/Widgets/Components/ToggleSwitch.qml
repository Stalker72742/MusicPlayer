import QtQuick
import SoundLink 1.0

Rectangle {
    id: root

    property bool checked: false

    signal toggled(bool checked)

    implicitWidth: 36
    implicitHeight: 20
    radius: height / 2
    color: checked ? Theme.palette.accent : Theme.palette.sliderTrack

    Accessible.role: Accessible.CheckBox
    Accessible.checkable: true
    Accessible.checked: checked

    Behavior on color { ColorAnimation { duration: Theme.motion.durationFast } }

    Rectangle {
        width: parent.height - 6
        height: width
        radius: width / 2
        y: 3
        x: root.checked ? parent.width - width - 3 : 3
        color: root.checked ? Theme.palette.content : Theme.palette.textSecondary

        Behavior on x { NumberAnimation { duration: Theme.motion.durationFast; easing.type: Theme.motion.easingType } }
    }

    HoverHandler { cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: root.toggled(!root.checked) }
}
