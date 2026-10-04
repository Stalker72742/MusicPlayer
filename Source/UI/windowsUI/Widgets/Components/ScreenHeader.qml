import QtQuick
import SoundLink 1.0

Column {
    id: root

    property string title: ""
    property string subtitle: ""

    spacing: 6

    Text {
        text: root.title
        color: Theme.palette.textPrimary
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.titleSize
        font.weight: Font.Bold
    }
    Text {
        width: root.width
        visible: root.subtitle.length > 0
        elide: Text.ElideRight
        text: root.subtitle
        color: Theme.palette.textSecondary
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.bodySize
    }
}
