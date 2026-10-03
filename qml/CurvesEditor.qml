import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Tone curve editor. The diagonal is "no change"; drag the curve up/down to
// brighten/darken that part of the tonal range. Click empty space to add a
// point, drag a point to move it, double-click an inner point to remove it.
// Works on the combined RGB channel or on R, G and B separately. The curve
// shown is sampled by the same C++ code that builds the exported image
// (appController.curveSamples), so what you see is what gets saved.
Column {
    id: root

    property int channel: 0
    // Working copy of the control points while editing: [{x, y}, ...] sorted
    // by x, always at least the two end points. The C++ state is the source of
    // truth; this is re-read from it whenever no drag is in progress.
    property var points: [{ x: 0, y: 0 }, { x: 1, y: 1 }]
    property int activeIndex: -1

    readonly property var modelPoints: appController.adjust.curves[channel]
    readonly property color curveColor: ["#f2f2f4", "#e5534b", "#4caf50", "#5b8def"][channel]
    readonly property string histKey: ["luma", "r", "g", "b"][channel]
    // Space kept free around the plot so the corner points are fully visible.
    readonly property real pad: 7

    spacing: 8

    function syncFromModel() {
        if (activeIndex >= 0)
            return;
        const m = modelPoints;
        points = (m && m.length >= 2)
            ? m.map(function (p) { return { x: p.x, y: p.y }; })
            : [{ x: 0, y: 0 }, { x: 1, y: 1 }];
        plot.requestPaint();
    }
    onModelPointsChanged: syncFromModel()
    onChannelChanged: { activeIndex = -1; syncFromModel(); }
    Component.onCompleted: syncFromModel()

    ChannelSelector {
        width: root.width
        channel: root.channel
        onPicked: function (c) { root.channel = c; }
    }

    Item {
        width: root.width
        height: root.width

        Canvas {
            id: plot
            anchors.fill: parent

            Connections {
                target: appController
                function onHistogramChanged() { plot.requestPaint(); }
            }
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            Component.onCompleted: requestPaint()

            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                ctx.fillStyle = "#17181a";
                ctx.fillRect(0, 0, width, height);
                ctx.translate(root.pad, root.pad);
                const w = width - 2 * root.pad;
                const h = height - 2 * root.pad;

                // The image's own histogram, faint, so it is clear which
                // tones each part of the curve acts on.
                const data = appController.histogramChannels[root.histKey];
                if (data && data.length === 256) {
                    ctx.beginPath();
                    ctx.moveTo(0, h);
                    for (let i = 0; i < 256; i++)
                        ctx.lineTo(i / 255 * w, h - data[i] * (h - 4) * 0.6);
                    ctx.lineTo(w, h);
                    ctx.closePath();
                    ctx.fillStyle = "rgba(255, 255, 255, 0.16)";
                    ctx.fill();
                }

                ctx.lineWidth = 1;
                ctx.strokeStyle = "rgba(255, 255, 255, 0.10)";
                ctx.beginPath();
                for (let i = 1; i < 4; i++) {
                    ctx.moveTo(w * i / 4, 0); ctx.lineTo(w * i / 4, h);
                    ctx.moveTo(0, h * i / 4); ctx.lineTo(w, h * i / 4);
                }
                ctx.stroke();

                ctx.strokeStyle = "rgba(255, 255, 255, 0.22)";
                ctx.beginPath();
                ctx.moveTo(0, h);
                ctx.lineTo(w, 0);
                ctx.stroke();

                const samples = appController.curveSamples(root.points);
                ctx.lineWidth = 2;
                ctx.strokeStyle = root.curveColor;
                ctx.beginPath();
                for (let i = 0; i < 256; i++) {
                    const x = i / 255 * w;
                    const y = (1 - samples[i]) * h;
                    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
                }
                ctx.stroke();

                for (let i = 0; i < root.points.length; i++) {
                    const p = root.points[i];
                    ctx.beginPath();
                    ctx.arc(p.x * w, (1 - p.y) * h, 4.5, 0, 2 * Math.PI);
                    ctx.fillStyle = i === root.activeIndex ? "#ffffff" : root.curveColor;
                    ctx.fill();
                    ctx.lineWidth = 1.5;
                    ctx.strokeStyle = "#101112";
                    ctx.stroke();
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.CrossCursor
            // Dragging a point must never turn into scrolling the panel this
            // sits in (the Flickable would steal the drag as soon as the
            // pointer moved a few pixels up or down).
            preventStealing: true

            readonly property real plotW: width - 2 * root.pad
            readonly property real plotH: height - 2 * root.pad
            function toX(mx) { return Math.min(1, Math.max(0, (mx - root.pad) / plotW)); }
            function toY(my) { return Math.min(1, Math.max(0, 1 - (my - root.pad) / plotH)); }

            function nearest(mx, my) {
                let best = -1;
                let bestDist = 12 * 12;
                for (let i = 0; i < root.points.length; i++) {
                    const dx = root.pad + root.points[i].x * plotW - mx;
                    const dy = root.pad + (1 - root.points[i].y) * plotH - my;
                    const d = dx * dx + dy * dy;
                    if (d < bestDist) { best = i; bestDist = d; }
                }
                return best;
            }

            function commit() {
                appController.setCurvePoints(root.channel, root.points);
                plot.requestPaint();
            }

            function moveActive(mx, my) {
                const i = root.activeIndex;
                if (i < 0)
                    return;
                const pts = root.points.slice();
                const last = pts.length - 1;
                // The two end points only slide up and down; inner points
                // stay strictly between their neighbors so the curve remains
                // a function of x.
                let x = pts[i].x;
                if (i > 0 && i < last)
                    x = Math.min(pts[i + 1].x - 0.02, Math.max(pts[i - 1].x + 0.02, toX(mx)));
                pts[i] = { x: x, y: toY(my) };
                root.points = pts;
                commit();
            }

            onPressed: function (mouse) {
                let i = nearest(mouse.x, mouse.y);
                if (i < 0) {
                    if (root.points.length >= 12)
                        return;
                    const nx = Math.min(0.98, Math.max(0.02, toX(mouse.x)));
                    const pts = root.points.slice();
                    let at = pts.findIndex(function (p) { return p.x > nx; });
                    if (at < 0)
                        at = pts.length;
                    pts.splice(at, 0, { x: nx, y: toY(mouse.y) });
                    root.points = pts;
                    i = at;
                }
                root.activeIndex = i;
                moveActive(mouse.x, mouse.y);
            }
            onPositionChanged: function (mouse) {
                if (pressed)
                    moveActive(mouse.x, mouse.y);
            }
            onReleased: {
                root.activeIndex = -1;
                root.syncFromModel();
            }
            onDoubleClicked: function (mouse) {
                const i = nearest(mouse.x, mouse.y);
                if (i > 0 && i < root.points.length - 1) {
                    const pts = root.points.slice();
                    pts.splice(i, 1);
                    root.points = pts;
                    commit();
                }
            }
        }
    }

    AppButton {
        text: qsTr("Restablecer curva")
        width: root.width
        onClicked: appController.setCurvePoints(root.channel, [])
    }
}
