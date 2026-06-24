import QtQuick
import SoundLink 1.0
import "../Components"

Rectangle {
    id: root

    property var window: null
    property alias searchText: search.text
    property bool maximized: false

    signal menuClicked

    implicitHeight: Theme.metrics.titleBarHeight
    height: Theme.metrics.titleBarHeight
    color: Theme.palette.titleBar

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.palette.hairline
    }

    DragHandler {
        id: dragHandler
        target: null
        onActiveChanged: {
            if (active && root.window)
                root.window.startSystemMove();
        }
    }

    IconButton {
        id: menuBtn
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 6
        implicitWidth: 36
        implicitHeight: 32
        iconName: "menu"
        iconSize: 16
        onClicked: root.menuClicked()
    }

    SearchField {
        id: search
        anchors.centerIn: parent
        width: Math.min(560, parent.width * 0.45)
        height: 30
        placeholder: "Search tracks, albums, artists..."
    }

    Row {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        spacing: 0

        TitleBarButton {
            iconName: "minimize"
            onClicked: if (root.window) root.window.showMinimized()
        }
        TitleBarButton {
            iconName: root.maximized ? "restore" : "maximize"
            onClicked: {
                if (!root.window) return;
                if (root.window.visibility === Window.Maximized)
                    root.window.showNormal();
                else
                    root.window.showMaximized();
            }
        }
        TitleBarButton {
            iconName: "close"
            isClose: true
            onClicked: if (root.window) root.window.close()
        }
    }
}
