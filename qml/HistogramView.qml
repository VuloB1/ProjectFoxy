import QtQuick
import ImageViewerApp

// Filled-area histogram. `channels` is AppController's {luma, r, g, b} map of
// 256 normalized heights. `mode` picks "rgb" (the three color channels
// overlaid, adding up to white where they agree) or a single "luma" / "r" /
// "g" / "b". Always drawn on a dark plate, like every photo editor does, so the
// colors read the same in any theme.
Canvas {
    id: root

    property var channels: ({})
    property string mode: "rgb"

    implicitHeight: 64

    onChannelsChanged: requestPaint()
    onModeChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    Component.onCompleted: requestPaint()

    function fillSeries(ctx, data, color) {
        if (!data || data.length < 256)
            return;
        const w = width;
        const h = height;
        ctx.beginPath();
        ctx.moveTo(0, h);
        for (let i = 0; i < 256; i++)
            ctx.lineTo(i / 255 * w, h - data[i] * (h - 3));
        ctx.lineTo(w, h);
        ctx.closePath();
        ctx.fillStyle = color;
        ctx.fill();
    }

    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        ctx.fillStyle = "#17181a";
        ctx.fillRect(0, 0, width, height);

        if (root.mode === "rgb") {
            ctx.globalCompositeOperation = "lighter";
            fillSeries(ctx, channels.r, "rgba(229, 83, 75, 0.85)");
            fillSeries(ctx, channels.g, "rgba(76, 175, 80, 0.85)");
            fillSeries(ctx, channels.b, "rgba(66, 133, 244, 0.85)");
        } else {
            const colors = { luma: "#b8bcc4", r: "#e5534b", g: "#4caf50", b: "#5b8def" };
            fillSeries(ctx, channels[root.mode], colors[root.mode] || "#b8bcc4");
        }
    }
}
