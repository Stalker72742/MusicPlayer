import QtQuick
import QtQuick.Templates as T
import SoundLink 1.0

// The app's scroll bar: attach as ScrollBar.vertical or ScrollBar.horizontal. Shown whenever there is more to
// scroll to, can be dragged, and a press on the track jumps there.
T.ScrollBar {
    id: root

    implicitWidth: vertical ? 10 : 40
    implicitHeight: vertical ? 40 : 10
    padding: 2
    hoverEnabled: true
    minimumSize: 0.05
    policy: T.ScrollBar.AlwaysOn
    visible: size > 0 && size < 1

    contentItem: Rectangle {
        implicitWidth: 6
        implicitHeight: 6
        radius: 3
        color: root.pressed ? Theme.palette.textPrimary
             : root.hovered ? Theme.palette.textSecondary
                            : Theme.palette.textMuted
    }
}
