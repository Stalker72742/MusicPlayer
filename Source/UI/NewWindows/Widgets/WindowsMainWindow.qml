import QtQuick
import QtQuick.Layouts 1.15
import QtQuick.Controls

ApplicationWindow {
    id: root

    readonly property real baseHeight: 900
    readonly property real baseWidth: 1600
    readonly property real scaleX: width / baseWidth
    readonly property real scaleY: height / baseHeight

    color: "#121212"
    flags: Qt.Window | Qt.FramelessWindowHint
    height: 900
    width: 1600

    minimumHeight: 400
    minimumWidth: 200
    visible: true

    Rectangle {
        id: titleBar

        color: "#141414"
        height: 40

        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
        }

        DragHandler {
            id: dragHandler

            onActiveChanged: {
                if (active)
                    root.startSystemMove();
            }
        }
        
        RowLayout {
            anchors.fill: parent
            spacing: 0

            // Иконка / логотип
            Item {
                width: 12
            }
            // Rectangle {
            //     Layout.alignment: Qt.AlignVCenter
            //     color: "#888888"
            //     height: 16
            //     radius: 4
            //     width: 16
            // }

            Canvas{

                height: 16
                width: 16
                
                onPaint: {
                    let ctx = getContext("2d");
                    ctx.strokeStyle = "#888888";
                    ctx.lineWidth = 1.5;

                    for (let i = 0; i < 3; i++) {
                        let y = 3 + i * 5;
                        ctx.beginPath();
                        ctx.moveTo(2, y);
                        ctx.lineTo(14, y); 
                        ctx.stroke();
                    }
                }
            }
            Item {
                width: 8
            }

            // Заголовок
            Text {
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true
                color: "#cdd6f4"
                elide: Text.ElideRight
                font.family: "Segoe UI"
                font.pixelSize: 13
                text: root.title
            }

            // Кнопки управления окном
            TitleBarButton {
                text: "─"

                onClicked: root.showMinimized()
            }
            TitleBarButton {
                text: root.visibility === Window.Maximized ? "❐" : "□"

                onClicked: {
                    if (root.visibility === Window.Maximized)
                        root.showNormal();
                    else
                        root.showMaximized();
                }
            }
            TitleBarButton {
                isClose: true
                text: "✕"

                onClicked: root.close()
            }
        }
    }

    Rectangle {
        id: bottomPanel

        color: "#161616"
        height: 90 * root.scaleY

        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }
    }
    Rectangle {
        id: leftPanel

        color: "#111111"
        width: 220 * root.scaleX

        anchors {
            bottom: bottomPanel.top
            left: parent.left
            top: titleBar.bottom
        }

        ColumnLayout {
            RowLayout {
                id: buttonLayout

                property bool opened: true

                Button {
                    onClicked: {
                        buttonLayout.opened = !buttonLayout.opened;
                        leftPanel.width = buttonLayout.opened ? 220 * root.scaleX : 50 * root.scaleX;
                    }

                    Text {
                        text: buttonLayout.opened ? "<" : ">"
                    }
                }
                Text {
                    id: text1

                    text: buttonLayout.opened ? "Collapse" : ""
                }
            }
        }
    }

    Rectangle {
        id: middlePanel

        color: "#0A0A0A"

        anchors {
            bottom: bottomPanel.top
            left: leftPanel.right
            right: parent.right
            top: titleBar.bottom
        }
    }

    Item {
        height: 16
        width: 16
        z: 10

        anchors {
            bottom: parent.bottom
            right: parent.right
        }

        HoverHandler {
            cursorShape: Qt.SizeFDiagCursor
        }

        DragHandler {
            grabPermissions: TapHandler.TakeOverForbidden

            onActiveChanged: {
                if (active)
                    root.startSystemResize(Qt.RightEdge | Qt.BottomEdge);
            }
        }

        Canvas {
            anchors.fill: parent

            onPaint: {
                let ctx = getContext("2d");
                ctx.strokeStyle = "#45475AFF";
                ctx.lineWidth = 1.5;
                for (let i = 4; i <= 12; i += 4) {
                    ctx.beginPath();
                    ctx.moveTo(i, 16);
                    ctx.lineTo(16, i);
                    ctx.stroke();
                }
            }
        }
    }
}