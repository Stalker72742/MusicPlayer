import QtQuick
import SoundLink 1.0

Item {
    id: root

    property string name: ""
    property color color: Theme.palette.iconDefault
    property real strokeWidth: 1.5

    implicitWidth: 18
    implicitHeight: 18

    Canvas {
        id: canvas
        anchors.fill: parent
        contextType: "2d"

        property color drawColor: root.color
        property string iconName: root.name
        property real sw: root.strokeWidth

        onDrawColorChanged: requestPaint()
        onIconNameChanged:  requestPaint()
        onSwChanged:        requestPaint()
        onWidthChanged:     requestPaint()
        onHeightChanged:    requestPaint()

        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.lineWidth   = sw;
            ctx.strokeStyle = drawColor;
            ctx.fillStyle   = drawColor;
            ctx.lineCap     = "round";
            ctx.lineJoin    = "round";

            const w = width, h = height;
            const cx = w / 2, cy = h / 2;
            const u = (v) => v * w / 24; // 24px design grid

            const stroke = (fn) => { ctx.beginPath(); fn(ctx); ctx.stroke(); };
            const fill   = (fn) => { ctx.beginPath(); fn(ctx); ctx.fill(); };

            switch (iconName) {

            case "menu":
                stroke(c => { c.moveTo(u(4), u(7));  c.lineTo(u(20), u(7)); });
                stroke(c => { c.moveTo(u(4), u(12)); c.lineTo(u(20), u(12)); });
                stroke(c => { c.moveTo(u(4), u(17)); c.lineTo(u(20), u(17)); });
                break;

            case "chevron-left":
                stroke(c => { c.moveTo(u(15), u(6)); c.lineTo(u(9), u(12)); c.lineTo(u(15), u(18)); });
                break;
            case "chevron-right":
                stroke(c => { c.moveTo(u(9), u(6)); c.lineTo(u(15), u(12)); c.lineTo(u(9), u(18)); });
                break;
            case "chevron-down":
                stroke(c => { c.moveTo(u(6), u(9)); c.lineTo(u(12), u(15)); c.lineTo(u(18), u(9)); });
                break;
            case "chevron-up":
                stroke(c => { c.moveTo(u(6), u(15)); c.lineTo(u(12), u(9)); c.lineTo(u(18), u(15)); });
                break;

            case "search":
                ctx.beginPath();
                ctx.arc(u(11), u(11), u(6), 0, Math.PI * 2);
                ctx.stroke();
                stroke(c => { c.moveTo(u(15.5), u(15.5)); c.lineTo(u(20), u(20)); });
                break;

            case "library": // play in circle
                ctx.beginPath();
                ctx.arc(cx, cy, u(9), 0, Math.PI * 2);
                ctx.stroke();
                fill(c => {
                    c.moveTo(u(10), u(8));
                    c.lineTo(u(16), u(12));
                    c.lineTo(u(10), u(16));
                    c.closePath();
                });
                break;

            case "list": // playlists
                ctx.strokeRect(u(4), u(5), u(16), u(14));
                stroke(c => { c.moveTo(u(8), u(9)); c.lineTo(u(17), u(9)); });
                stroke(c => { c.moveTo(u(8), u(12)); c.lineTo(u(17), u(12)); });
                stroke(c => { c.moveTo(u(8), u(15)); c.lineTo(u(13), u(15)); });
                break;

            case "heart":
                ctx.beginPath();
                ctx.moveTo(u(12), u(20));
                ctx.bezierCurveTo(u(2), u(13.5),  u(2), u(7),     u(7), u(5));
                ctx.bezierCurveTo(u(9.5), u(4),   u(11), u(5.5),  u(12), u(7.5));
                ctx.bezierCurveTo(u(13), u(5.5),  u(14.5), u(4),  u(17), u(5));
                ctx.bezierCurveTo(u(22), u(7),    u(22), u(13.5), u(12), u(20));
                ctx.closePath();
                ctx.stroke();
                break;

            case "heart-filled":
                ctx.beginPath();
                ctx.moveTo(u(12), u(20));
                ctx.bezierCurveTo(u(2), u(13.5),  u(2), u(7),     u(7), u(5));
                ctx.bezierCurveTo(u(9.5), u(4),   u(11), u(5.5),  u(12), u(7.5));
                ctx.bezierCurveTo(u(13), u(5.5),  u(14.5), u(4),  u(17), u(5));
                ctx.bezierCurveTo(u(22), u(7),    u(22), u(13.5), u(12), u(20));
                ctx.closePath();
                ctx.fill();
                break;

            case "clock":
                ctx.beginPath();
                ctx.arc(cx, cy, u(8), 0, Math.PI * 2);
                ctx.stroke();
                stroke(c => { c.moveTo(cx, cy); c.lineTo(cx, cy - u(5)); });
                stroke(c => { c.moveTo(cx, cy); c.lineTo(cx + u(4), cy); });
                break;

            case "settings": // sun-like
                ctx.beginPath();
                ctx.arc(cx, cy, u(3.5), 0, Math.PI * 2);
                ctx.stroke();
                for (let i = 0; i < 8; i++) {
                    const a = i * Math.PI / 4;
                    const r1 = u(6), r2 = u(8.5);
                    stroke(c => {
                        c.moveTo(cx + Math.cos(a) * r1, cy + Math.sin(a) * r1);
                        c.lineTo(cx + Math.cos(a) * r2, cy + Math.sin(a) * r2);
                    });
                }
                break;

            case "audio": // speaker with waves
                fill(c => {
                    c.moveTo(u(4),  u(9));
                    c.lineTo(u(8),  u(9));
                    c.lineTo(u(12), u(5));
                    c.lineTo(u(12), u(19));
                    c.lineTo(u(8),  u(15));
                    c.lineTo(u(4),  u(15));
                    c.closePath();
                });
                ctx.beginPath();
                ctx.arc(u(13), u(12), u(3), -Math.PI/3, Math.PI/3);
                ctx.stroke();
                ctx.beginPath();
                ctx.arc(u(13), u(12), u(6), -Math.PI/3, Math.PI/3);
                ctx.stroke();
                break;

            case "info":
                ctx.beginPath();
                ctx.arc(cx, cy, u(9), 0, Math.PI * 2);
                ctx.stroke();
                stroke(c => { c.moveTo(cx, cy - u(3)); c.lineTo(cx, cy + u(4)); });
                ctx.beginPath();
                ctx.arc(cx, cy - u(6), u(0.5), 0, Math.PI * 2);
                ctx.fill();
                break;

            case "download":
                stroke(c => { c.moveTo(u(12), u(4)); c.lineTo(u(12), u(15)); });
                stroke(c => { c.moveTo(u(7), u(11)); c.lineTo(u(12), u(16)); c.lineTo(u(17), u(11)); });
                stroke(c => { c.moveTo(u(5), u(20)); c.lineTo(u(19), u(20)); });
                break;

            case "play":
                fill(c => {
                    c.moveTo(u(7),  u(5));
                    c.lineTo(u(19), u(12));
                    c.lineTo(u(7),  u(19));
                    c.closePath();
                });
                break;

            case "pause":
                ctx.fillRect(u(7), u(5),  u(3), u(14));
                ctx.fillRect(u(14), u(5), u(3), u(14));
                break;

            case "next":
                fill(c => {
                    c.moveTo(u(5),  u(5));
                    c.lineTo(u(15), u(12));
                    c.lineTo(u(5),  u(19));
                    c.closePath();
                });
                ctx.fillRect(u(16), u(5), u(2.5), u(14));
                break;

            case "prev":
                ctx.fillRect(u(5.5), u(5), u(2.5), u(14));
                fill(c => {
                    c.moveTo(u(19), u(5));
                    c.lineTo(u(9),  u(12));
                    c.lineTo(u(19), u(19));
                    c.closePath();
                });
                break;

            case "shuffle":
                stroke(c => { c.moveTo(u(3), u(7));  c.lineTo(u(7), u(7));   c.lineTo(u(17), u(17)); c.lineTo(u(21), u(17)); });
                stroke(c => { c.moveTo(u(3), u(17)); c.lineTo(u(7), u(17));  c.lineTo(u(11), u(13)); });
                stroke(c => { c.moveTo(u(13), u(11)); c.lineTo(u(17), u(7)); c.lineTo(u(21), u(7));  });
                stroke(c => { c.moveTo(u(18), u(4)); c.lineTo(u(21), u(7));  c.lineTo(u(18), u(10)); });
                stroke(c => { c.moveTo(u(18), u(14)); c.lineTo(u(21), u(17)); c.lineTo(u(18), u(20)); });
                break;

            case "repeat":
                stroke(c => { c.moveTo(u(7), u(8)); c.lineTo(u(17), u(8)); c.lineTo(u(17), u(11)); });
                stroke(c => { c.moveTo(u(20), u(8)); c.lineTo(u(17), u(5)); });
                stroke(c => { c.moveTo(u(20), u(8)); c.lineTo(u(17), u(11)); });
                stroke(c => { c.moveTo(u(17), u(16)); c.lineTo(u(7), u(16)); c.lineTo(u(7), u(13)); });
                stroke(c => { c.moveTo(u(4), u(16)); c.lineTo(u(7), u(13)); });
                stroke(c => { c.moveTo(u(4), u(16)); c.lineTo(u(7), u(19)); });
                break;

            case "queue":
                stroke(c => { c.moveTo(u(4), u(7));  c.lineTo(u(16), u(7)); });
                stroke(c => { c.moveTo(u(4), u(12)); c.lineTo(u(16), u(12)); });
                stroke(c => { c.moveTo(u(4), u(17)); c.lineTo(u(11), u(17)); });
                ctx.fillRect(u(18), u(13), u(3), u(7));
                ctx.fillRect(u(15), u(15), u(2), u(5));
                break;

            case "volume":
                fill(c => {
                    c.moveTo(u(4),  u(9));
                    c.lineTo(u(8),  u(9));
                    c.lineTo(u(12), u(5));
                    c.lineTo(u(12), u(19));
                    c.lineTo(u(8),  u(15));
                    c.lineTo(u(4),  u(15));
                    c.closePath();
                });
                ctx.beginPath();
                ctx.arc(u(13), u(12), u(3.5), -Math.PI/3, Math.PI/3);
                ctx.stroke();
                break;

            case "more":
                ctx.beginPath(); ctx.arc(u(6),  u(12), u(1.3), 0, Math.PI*2); ctx.fill();
                ctx.beginPath(); ctx.arc(u(12), u(12), u(1.3), 0, Math.PI*2); ctx.fill();
                ctx.beginPath(); ctx.arc(u(18), u(12), u(1.3), 0, Math.PI*2); ctx.fill();
                break;

            case "playing": // 3 bars equalizer
                ctx.fillRect(u(5),  u(8),  u(2), u(8));
                ctx.fillRect(u(11), u(5),  u(2), u(14));
                ctx.fillRect(u(17), u(10), u(2), u(6));
                break;

            case "minimize":
                stroke(c => { c.moveTo(u(5), u(12)); c.lineTo(u(19), u(12)); });
                break;

            case "maximize":
                ctx.strokeRect(u(5), u(5), u(14), u(14));
                break;

            case "restore":
                ctx.strokeRect(u(5),  u(7), u(12), u(12));
                stroke(c => { c.moveTo(u(8), u(7)); c.lineTo(u(8), u(4)); c.lineTo(u(20), u(4)); c.lineTo(u(20), u(16)); c.lineTo(u(17), u(16)); });
                break;

            case "close":
                stroke(c => { c.moveTo(u(6),  u(6));  c.lineTo(u(18), u(18)); });
                stroke(c => { c.moveTo(u(18), u(6));  c.lineTo(u(6),  u(18)); });
                break;

            default:
                ctx.strokeRect(u(4), u(4), u(16), u(16));
                break;
            }
        }
    }
}
