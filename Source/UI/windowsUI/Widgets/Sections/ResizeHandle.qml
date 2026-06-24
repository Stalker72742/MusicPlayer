import QtQuick
import SoundLink 1.0

Item {
    id: root

    property var window: null

    width: 16
    height: 16

    HoverHandler { cursorShape: Qt.SizeFDiagCursor }

    DragHandler {
        target: null
        grabPermissions: TapHandler.TakeOverForbidden
        onActiveChanged: {
            if (active && root.window)
                root.window.startSystemResize(Qt.RightEdge | Qt.BottomEdge);
        }
    }

    Canvas {
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.strokeStyle = Theme.palette.textTertiary;
            ctx.lineWidth = 1.5;
            ctx.lineCap = "round";
            for (let i = 4; i <= 12; i += 4) {
                ctx.beginPath();
                ctx.moveTo(i, 16);
                ctx.lineTo(16, i);
                ctx.stroke();
            }
        }
    }
}
