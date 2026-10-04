pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import SoundLink 1.0

// Picks one of options ([{id, title}]); currentId is the owner's, activated() reports the choice.
Rectangle {
    id: root

    property var options: []
    property string currentId: ""
    readonly property string currentTitle: {
        for (let i = 0; i < options.length; ++i) {
            if (options[i].id === currentId)
                return options[i].title;
        }
        return options.length > 0 ? options[0].title : "";
    }

    signal activated(string id)

    implicitWidth: 160
    implicitHeight: 30
    radius: Theme.metrics.radiusMd
    color: hover.hovered || list.visible ? Theme.palette.pillSelected : Theme.palette.fieldBg
    border.width: 1
    border.color: Theme.palette.border

    Text {
        anchors.left: parent.left
        anchors.right: chevron.left
        anchors.leftMargin: 10
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        elide: Text.ElideRight
        text: root.currentTitle
        color: Theme.palette.textPrimary
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.bodySize
    }

    AppIcon {
        id: chevron
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        width: 12; height: 12
        name: "chevron-down"
        color: Theme.palette.textSecondary
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: list.open() }

    Popup {
        id: list
        y: root.height + 4
        width: Math.max(root.width, 180)
        padding: 4
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: Theme.palette.fieldBg
            radius: Theme.metrics.radiusLg
            border.width: 1
            border.color: Theme.palette.borderStrong
        }

        contentItem: ListView {
            implicitHeight: Math.min(contentHeight, 320)
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: root.options

            delegate: Rectangle {
                id: option

                required property var modelData

                width: ListView.view.width
                height: 30
                radius: Theme.metrics.radiusMd
                color: optionHover.hovered ? Theme.palette.hoverOverlay : "transparent"

                Text {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideRight
                    text: option.modelData.title
                    color: Theme.palette.textPrimary
                    font.family: Theme.typography.family
                    font.pixelSize: Theme.typography.bodySize
                    font.weight: option.modelData.id === root.currentId ? Font.DemiBold : Font.Normal
                }

                HoverHandler { id: optionHover; cursorShape: Qt.PointingHandCursor }
                TapHandler {
                    onTapped: {
                        list.close();
                        root.activated(option.modelData.id);
                    }
                }
            }
        }
    }
}
