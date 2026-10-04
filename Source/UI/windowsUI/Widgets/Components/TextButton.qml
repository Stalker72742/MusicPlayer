import QtQuick
import SoundLink 1.0

Rectangle {
    id: root

    property string text: ""
    property bool primary: false
    property bool hovered: hover.hovered

    signal clicked

    implicitWidth: label.implicitWidth + 28
    implicitHeight: 30
    radius: Theme.metrics.radiusMd
    opacity: enabled ? 1 : 0.5

    color: primary ? (hovered ? Theme.palette.textPrimary : Theme.palette.accent)
                   : (hovered ? Theme.palette.pillSelected : Theme.palette.fieldBg)
    border.width: primary ? 0 : 1
    border.color: Theme.palette.fieldBorder

    Accessible.role: Accessible.Button
    Accessible.name: text

    Behavior on color { ColorAnimation { duration: Theme.motion.durationFast } }

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.primary ? Theme.palette.content : Theme.palette.textPrimary
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.bodySize
        font.weight: Font.DemiBold
    }

    HoverHandler { id: hover; enabled: root.enabled; cursorShape: Qt.PointingHandCursor }
    TapHandler { enabled: root.enabled; onTapped: root.clicked() }
}
