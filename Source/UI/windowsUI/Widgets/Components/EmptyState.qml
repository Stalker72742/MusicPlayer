import QtQuick
import SoundLink 1.0

Item {
    id: root

    property string iconName: ""
    property string title: ""
    property string text: ""
    property string actionText: ""

    signal actionClicked

    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 360)
        spacing: 10

        AppIcon {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.iconName.length > 0
            width: 32; height: 32
            name: root.iconName
            color: Theme.palette.textTertiary
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: root.title
            color: Theme.palette.textPrimary
            font.family: Theme.typography.family
            font.pixelSize: Theme.typography.sectionSize
            font.weight: Font.DemiBold
        }
        Text {
            width: parent.width
            visible: root.text.length > 0
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: root.text
            color: Theme.palette.textSecondary
            font.family: Theme.typography.family
            font.pixelSize: Theme.typography.bodySize
        }
        Item {
            width: parent.width
            height: action.visible ? action.implicitHeight + 6 : 0

            TextButton {
                id: action
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                visible: root.actionText.length > 0
                text: root.actionText
                onClicked: root.actionClicked()
            }
        }
    }
}
