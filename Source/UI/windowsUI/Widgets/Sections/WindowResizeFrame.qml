import QtQuick

// Invisible border of a frameless window: dragging any edge or corner resizes it.
// Fill the window with it and keep it above the content; it only takes input along the edges.
Item {
    id: root

    property var window: null
    // Thickness of the edges and size of the corners that resize.
    property int edge: 6
    property int corner: 14
    // Area in the top right corner that does not resize (the title bar buttons take the clicks there).
    property real topRightWidth: 0
    property real topRightHeight: 0

    readonly property real topRightX: parent.width - Math.max(corner, topRightWidth)
    readonly property real rightY: Math.max(corner, topRightHeight)
    readonly property bool topRightCorner: topRightWidth <= 0 || topRightHeight <= 0

    enabled: window !== null && window.visibility === Window.Windowed
    visible: enabled

    component Zone: MouseArea {
        property int edges: 0
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        onPressed: if (root.window) root.window.startSystemResize(edges)
    }

    Zone { edges: Qt.LeftEdge;   cursorShape: Qt.SizeHorCursor; x: 0; y: root.corner; width: root.edge; height: parent.height - 2 * root.corner }
    Zone { edges: Qt.RightEdge;  cursorShape: Qt.SizeHorCursor; x: parent.width - root.edge; y: root.rightY; width: root.edge; height: parent.height - root.rightY - root.corner }
    Zone { edges: Qt.TopEdge;    cursorShape: Qt.SizeVerCursor; x: root.corner; y: 0; width: root.topRightX - root.corner; height: root.edge }
    Zone { edges: Qt.BottomEdge; cursorShape: Qt.SizeVerCursor; x: root.corner; y: parent.height - root.edge; width: parent.width - 2 * root.corner; height: root.edge }

    Zone { edges: Qt.LeftEdge | Qt.TopEdge;     cursorShape: Qt.SizeFDiagCursor; x: 0; y: 0; width: root.corner; height: root.edge }
    Zone { edges: Qt.LeftEdge | Qt.TopEdge;     cursorShape: Qt.SizeFDiagCursor; x: 0; y: 0; width: root.edge; height: root.corner }
    Zone { edges: Qt.RightEdge | Qt.TopEdge;    cursorShape: Qt.SizeBDiagCursor; visible: root.topRightCorner; x: parent.width - root.corner; y: 0; width: root.corner; height: root.edge }
    Zone { edges: Qt.RightEdge | Qt.TopEdge;    cursorShape: Qt.SizeBDiagCursor; visible: root.topRightCorner; x: parent.width - root.edge; y: 0; width: root.edge; height: root.corner }
    Zone { edges: Qt.LeftEdge | Qt.BottomEdge;  cursorShape: Qt.SizeBDiagCursor; x: 0; y: parent.height - root.edge; width: root.corner; height: root.edge }
    Zone { edges: Qt.LeftEdge | Qt.BottomEdge;  cursorShape: Qt.SizeBDiagCursor; x: 0; y: parent.height - root.corner; width: root.edge; height: root.corner }
    Zone { edges: Qt.RightEdge | Qt.BottomEdge; cursorShape: Qt.SizeFDiagCursor; x: parent.width - root.corner; y: parent.height - root.edge; width: root.corner; height: root.edge }
    Zone { edges: Qt.RightEdge | Qt.BottomEdge; cursorShape: Qt.SizeFDiagCursor; x: parent.width - root.edge; y: parent.height - root.corner; width: root.edge; height: root.corner }
}
