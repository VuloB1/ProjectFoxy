import QtQuick

// Small hand-drawn line-icon set, rendered on a Canvas so every icon shares
// exactly the same stroke weight and color (driven by the theme) instead of
// relying on font glyphs, which don't have a consistent visual weight and
// aren't always intuitive (e.g. there's no good "open a file" character).
// Every path is designed on a fixed 24x24 grid, then scaled to fit `size`.
Canvas {
    id: root

    property string name: ""
    property color color: "black"
    property real strokeWidth: 1.8
    // Convenience for the common case (a square icon) - sets both dimensions
    // at once. Leave unset and bind width/height directly for anything else.
    property real size: 22

    implicitWidth: size
    implicitHeight: size

    onNameChanged: requestPaint()
    onColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onStrokeWidthChanged: requestPaint()
    Component.onCompleted: requestPaint()

    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        ctx.strokeStyle = root.color;
        ctx.fillStyle = root.color;
        ctx.lineCap = "round";
        ctx.lineJoin = "round";

        const grid = 24;
        const s = Math.min(width, height) / grid;
        ctx.save();
        ctx.translate((width - grid * s) / 2, (height - grid * s) / 2);
        ctx.scale(s, s);
        ctx.lineWidth = root.strokeWidth / s;

        // Mirrors everything drawn afterward left-to-right across the grid, so
        // a "left" icon is always the exact reflection of its "right" twin.
        // Callers wrap it in save()/restore().
        function mirrorX() {
            ctx.translate(grid, 0);
            ctx.scale(-1, 1);
        }

        // Three-quarter ring that sweeps out of the top and tapers into a
        // short tail ending in a corner-style arrowhead (the same head the
        // "resize" icon uses). Drawn clockwise; the counter-clockwise variant
        // is the exact mirror image so the pair always looks like a set.
        function rotateIcon(ccw) {
            ctx.save();
            if (ccw)
                mirrorX();
            ctx.beginPath();
            ctx.arc(12, 12, 9, 0, 1.5 * Math.PI);
            ctx.bezierCurveTo(14.52, 3, 16.93, 4, 18.74, 5.74);
            ctx.lineTo(21, 8);
            ctx.moveTo(21, 3);
            ctx.lineTo(21, 8);
            ctx.lineTo(16, 8);
            ctx.stroke();
            ctx.restore();
        }

        // A chevron pointing back at a hook-shaped tail: the arrow leaves
        // the head, runs right, and U-turns back underneath itself. Undo
        // points left; redo is the mirror image.
        function undoIcon(redo) {
            ctx.save();
            if (redo)
                mirrorX();
            ctx.beginPath();
            ctx.moveTo(9, 14);
            ctx.lineTo(4, 9);
            ctx.lineTo(9, 4);
            ctx.stroke();
            ctx.beginPath();
            ctx.moveTo(4, 9);
            ctx.lineTo(14.5, 9);
            ctx.arc(14.5, 14.5, 5.5, -Math.PI / 2, Math.PI / 2);
            ctx.lineTo(11, 20);
            ctx.stroke();
            ctx.restore();
        }

        // Outline of a crop frame with the given proportions, centered on
        // the grid. `w`/`h` are the frame's full size (before stroke). Drawn
        // with a hairline (about 60% of the normal weight) so a frame that
        // only differs from its neighbor by proportions reads as a clean
        // outline instead of a chunky box.
        function frameIcon(w, h) {
            ctx.save();
            ctx.lineWidth = root.strokeWidth * 0.6 / s;
            ctx.beginPath();
            ctx.roundedRect((grid - w) / 2, (grid - h) / 2, w, h, 1.2, 1.2);
            ctx.stroke();
            ctx.restore();
        }

        // One row of the "sliders" icon: a track interrupted by a hollow knob.
        function sliderRow(y, knobX) {
            const r = 2.4;
            ctx.beginPath();
            ctx.moveTo(3, y); ctx.lineTo(knobX - r, y);
            ctx.moveTo(knobX + r, y); ctx.lineTo(21, y);
            ctx.stroke();
            ctx.beginPath();
            ctx.arc(knobX, y, r, 0, 2 * Math.PI);
            ctx.stroke();
        }

        // A four-pointed star with concave sides (the usual "sparkle").
        function sparkle(cx, cy, r) {
            const k = r * 0.22;
            ctx.beginPath();
            ctx.moveTo(cx, cy - r);
            ctx.quadraticCurveTo(cx + k, cy - k, cx + r, cy);
            ctx.quadraticCurveTo(cx + k, cy + k, cx, cy + r);
            ctx.quadraticCurveTo(cx - k, cy + k, cx - r, cy);
            ctx.quadraticCurveTo(cx - k, cy - k, cx, cy - r);
            ctx.closePath();
            ctx.stroke();
        }

        function closedShape(points) {
            ctx.beginPath();
            ctx.moveTo(points[0], points[1]);
            for (let i = 2; i < points.length; i += 2)
                ctx.lineTo(points[i], points[i + 1]);
            ctx.closePath();
            ctx.stroke();
        }

        switch (root.name) {
        case "open":
            ctx.beginPath();
            ctx.moveTo(3, 7); ctx.lineTo(9, 7); ctx.lineTo(11, 9); ctx.lineTo(21, 9);
            ctx.lineTo(21, 19); ctx.lineTo(3, 19); ctx.closePath();
            ctx.stroke();
            break;
        case "prev":
            ctx.beginPath(); ctx.moveTo(14, 5); ctx.lineTo(8, 12); ctx.lineTo(14, 19); ctx.stroke();
            break;
        case "next":
            ctx.beginPath(); ctx.moveTo(10, 5); ctx.lineTo(16, 12); ctx.lineTo(10, 19); ctx.stroke();
            break;
        case "zoom-out":
        case "zoom-in":
            ctx.beginPath(); ctx.arc(10, 10, 6, 0, 2 * Math.PI); ctx.stroke();
            ctx.beginPath(); ctx.moveTo(14.2, 14.2); ctx.lineTo(20, 20); ctx.stroke();
            ctx.beginPath(); ctx.moveTo(7, 10); ctx.lineTo(13, 10);
            if (root.name === "zoom-in") { ctx.moveTo(10, 7); ctx.lineTo(10, 13); }
            ctx.stroke();
            break;
        case "rotate-left":
        case "reset":
            rotateIcon(true);
            break;
        case "rotate-right":
            rotateIcon(false);
            break;
        case "fit":
            ctx.beginPath();
            ctx.moveTo(4, 9); ctx.lineTo(4, 4); ctx.lineTo(9, 4);
            ctx.moveTo(15, 4); ctx.lineTo(20, 4); ctx.lineTo(20, 9);
            ctx.moveTo(4, 15); ctx.lineTo(4, 20); ctx.lineTo(9, 20);
            ctx.moveTo(15, 20); ctx.lineTo(20, 20); ctx.lineTo(20, 15);
            ctx.stroke();
            break;
        case "play":
            ctx.beginPath(); ctx.moveTo(8, 5); ctx.lineTo(19, 12); ctx.lineTo(8, 19); ctx.closePath(); ctx.fill();
            break;
        case "pause":
            ctx.fillRect(7, 5, 3.6, 14); ctx.fillRect(13.4, 5, 3.6, 14);
            break;
        case "compare":
            ctx.beginPath(); ctx.arc(12, 12, 8, 0, 2 * Math.PI); ctx.stroke();
            ctx.beginPath(); ctx.moveTo(12, 12); ctx.arc(12, 12, 8, -Math.PI / 2, Math.PI / 2, true); ctx.closePath(); ctx.fill();
            break;
        case "undo":
            undoIcon(false);
            break;
        case "redo":
            undoIcon(true);
            break;
        case "aspect-free":
            // A selection box with a grab handle on every corner: no fixed
            // proportions, drag any of them.
            frameIcon(16, 12);
            for (const corner of [[4, 6], [20, 6], [4, 18], [20, 18]])
                ctx.fillRect(corner[0] - 1.5, corner[1] - 1.5, 3, 3);
            break;
        // The four fixed ratios share one visual language: the frame is
        // as tall/wide as its ratio says, with a big enough spread between
        // neighbors (15 vs 20 vs 22 units) that 1:1 can't be mistaken for
        // 4:3 or 3:4 at a glance.
        case "aspect-1-1":
            frameIcon(15, 15);
            break;
        case "aspect-4-3":
            frameIcon(20, 15);
            break;
        case "aspect-16-9":
            frameIcon(22, 12.4);
            break;
        case "aspect-3-4":
            frameIcon(15, 20);
            break;
        case "flip-h":
            // Two triangles facing each other across a dashed mirror line.
            closedShape([3, 6, 9.5, 12, 3, 18]);
            closedShape([21, 6, 14.5, 12, 21, 18]);
            ctx.beginPath();
            for (const y of [2, 8, 14, 20]) { ctx.moveTo(12, y); ctx.lineTo(12, y + 2); }
            ctx.stroke();
            break;
        case "flip-v":
            closedShape([6, 3, 12, 9.5, 18, 3]);
            closedShape([6, 21, 12, 14.5, 18, 21]);
            ctx.beginPath();
            for (const x of [2, 8, 14, 20]) { ctx.moveTo(x, 12); ctx.lineTo(x + 2, 12); }
            ctx.stroke();
            break;
        case "cancel":
        case "close":
            ctx.beginPath();
            ctx.moveTo(6, 6); ctx.lineTo(18, 18); ctx.moveTo(18, 6); ctx.lineTo(6, 18);
            ctx.stroke();
            break;
        case "check":
            ctx.beginPath(); ctx.moveTo(5, 13); ctx.lineTo(10, 18); ctx.lineTo(19, 6); ctx.stroke();
            break;
        case "save":
            ctx.beginPath();
            ctx.moveTo(5, 4); ctx.lineTo(16, 4); ctx.lineTo(20, 8); ctx.lineTo(20, 20); ctx.lineTo(5, 20); ctx.closePath();
            ctx.stroke();
            ctx.beginPath(); ctx.moveTo(8, 4); ctx.lineTo(8, 10); ctx.lineTo(16, 10); ctx.lineTo(16, 4); ctx.stroke();
            ctx.beginPath(); ctx.moveTo(8, 20); ctx.lineTo(8, 14); ctx.lineTo(16, 14); ctx.lineTo(16, 20); ctx.stroke();
            break;
        case "save-as":
            ctx.beginPath();
            ctx.moveTo(4, 4); ctx.lineTo(12, 4); ctx.lineTo(16, 8); ctx.lineTo(16, 19); ctx.lineTo(4, 19); ctx.closePath();
            ctx.stroke();
            ctx.beginPath(); ctx.moveTo(7, 4); ctx.lineTo(7, 9); ctx.lineTo(13, 9); ctx.lineTo(13, 4); ctx.stroke();
            ctx.beginPath(); ctx.arc(18.5, 17.5, 4.3, 0, 2 * Math.PI); ctx.stroke();
            ctx.beginPath(); ctx.moveTo(18.5, 15.6); ctx.lineTo(18.5, 19.4); ctx.moveTo(16.6, 17.5); ctx.lineTo(20.4, 17.5); ctx.stroke();
            break;
        case "crop":
            // The classic crop tool: two offset L-shapes with softened corners.
            ctx.beginPath();
            ctx.moveTo(6, 2); ctx.lineTo(6, 16); ctx.arcTo(6, 18, 8, 18, 2); ctx.lineTo(22, 18);
            ctx.moveTo(18, 22); ctx.lineTo(18, 8); ctx.arcTo(18, 6, 16, 6, 2); ctx.lineTo(2, 6);
            ctx.stroke();
            break;
        case "resize":
            // A frame whose top-right corner is open, a smaller square
            // tucked in its bottom-left and an arrow leaving through the
            // gap: "this, but bigger".
            ctx.beginPath();
            ctx.moveTo(12, 4); ctx.lineTo(6, 4); ctx.arcTo(4, 4, 4, 6, 2);
            ctx.lineTo(4, 18); ctx.arcTo(4, 20, 6, 20, 2);
            ctx.lineTo(18, 20); ctx.arcTo(20, 20, 20, 18, 2);
            ctx.lineTo(20, 12);
            ctx.moveTo(4, 12); ctx.lineTo(12, 12); ctx.lineTo(12, 20);
            ctx.stroke();
            ctx.beginPath();
            ctx.moveTo(14.5, 9.5); ctx.lineTo(20, 4);
            ctx.moveTo(15, 4); ctx.lineTo(20, 4); ctx.lineTo(20, 9);
            ctx.stroke();
            break;
        case "sliders":
            sliderRow(6, 8);
            sliderRow(12, 16);
            sliderRow(18, 10.5);
            break;
        case "effects":
            sparkle(10, 14, 7.5);
            sparkle(18.5, 5.5, 3.3);
            break;
        case "copy":
            closedShape([4, 9, 15, 9, 15, 20, 4, 20]);
            ctx.beginPath(); ctx.moveTo(9, 9); ctx.lineTo(9, 4); ctx.lineTo(20, 4); ctx.lineTo(20, 15); ctx.lineTo(15, 15); ctx.stroke();
            break;
        case "paste":
            closedShape([5, 5, 19, 5, 19, 21, 5, 21]);
            ctx.beginPath(); ctx.rect(9, 3, 6, 4); ctx.fillStyle = root.color; ctx.fill();
            ctx.beginPath(); ctx.moveTo(8.5, 12); ctx.lineTo(15.5, 12); ctx.moveTo(8.5, 16); ctx.lineTo(13.5, 16); ctx.stroke();
            break;
        case "wallpaper":
            closedShape([3, 4, 21, 4, 21, 16, 3, 16]);
            ctx.beginPath(); ctx.moveTo(12, 16); ctx.lineTo(12, 20); ctx.moveTo(8, 20); ctx.lineTo(16, 20); ctx.stroke();
            ctx.beginPath(); ctx.moveTo(6, 13); ctx.lineTo(10, 8.5); ctx.lineTo(13, 12); ctx.lineTo(15, 10); ctx.lineTo(18, 13); ctx.stroke();
            break;
        case "trash":
            ctx.beginPath(); ctx.moveTo(4, 7); ctx.lineTo(20, 7); ctx.moveTo(9, 7); ctx.lineTo(9, 4); ctx.lineTo(15, 4); ctx.lineTo(15, 7); ctx.stroke();
            ctx.beginPath(); ctx.moveTo(6, 7); ctx.lineTo(7, 20); ctx.lineTo(17, 20); ctx.lineTo(18, 7); ctx.stroke();
            ctx.beginPath(); ctx.moveTo(10, 11); ctx.lineTo(10, 16); ctx.moveTo(14, 11); ctx.lineTo(14, 16); ctx.stroke();
            break;
        case "export-batch":
            ctx.beginPath(); ctx.moveTo(4, 14); ctx.lineTo(4, 20); ctx.lineTo(20, 20); ctx.lineTo(20, 14); ctx.stroke();
            ctx.beginPath(); ctx.moveTo(12, 16); ctx.lineTo(12, 4); ctx.moveTo(7.5, 8.5); ctx.lineTo(12, 4); ctx.lineTo(16.5, 8.5); ctx.stroke();
            break;
        case "rename-batch":
            closedShape([3, 7, 21, 7, 21, 17, 3, 17]);
            ctx.beginPath(); ctx.moveTo(8, 10); ctx.lineTo(8, 14); ctx.moveTo(6.3, 10); ctx.lineTo(9.7, 10); ctx.moveTo(6.3, 14); ctx.lineTo(9.7, 14); ctx.stroke();
            ctx.beginPath(); ctx.moveTo(12.5, 12); ctx.lineTo(18, 12); ctx.stroke();
            break;
        case "edit": {
            // A rounded square open at the top-right corner, with a solid pencil
            // crossing out through the gap.
            ctx.beginPath();
            ctx.moveTo(12, 4); ctx.lineTo(7, 4); ctx.quadraticCurveTo(4, 4, 4, 7);
            ctx.lineTo(4, 17); ctx.quadraticCurveTo(4, 20, 7, 20);
            ctx.lineTo(17, 20); ctx.quadraticCurveTo(20, 20, 20, 17);
            ctx.lineTo(20, 12);
            ctx.stroke();
            ctx.beginPath();
            ctx.moveTo(12.3, 13.9); ctx.lineTo(19.5, 6.7);        // body, lower edge
            ctx.lineTo(17.3, 4.5); ctx.lineTo(10.1, 11.7);       // body, upper edge
            ctx.closePath();
            ctx.fill();
            ctx.beginPath();
            ctx.moveTo(10.1, 11.7); ctx.lineTo(12.3, 13.9); ctx.lineTo(8.4, 15.6); ctx.closePath(); // tip
            ctx.fill();
            break;
        }
        case "info":
            ctx.beginPath(); ctx.arc(12, 12, 9, 0, 2 * Math.PI); ctx.stroke();
            ctx.beginPath(); ctx.moveTo(12, 11); ctx.lineTo(12, 16.5); ctx.stroke();
            ctx.beginPath(); ctx.arc(12, 7.6, 1.1, 0, 2 * Math.PI); ctx.fill();
            break;
        case "eyedropper": {
            // A pipette: a hollow tube narrowing to the tip (bottom left), a collar and a solid bulb.
            ctx.save();
            ctx.translate(3.6, 20.4);
            ctx.rotate(Math.PI / 4);
            ctx.beginPath();
            ctx.moveTo(0, 0); ctx.lineTo(-1.5, -6); ctx.lineTo(-2.7, -13.2); ctx.lineTo(2.7, -13.2); ctx.lineTo(1.5, -6);
            ctx.closePath();
            ctx.stroke();
            ctx.beginPath();
            ctx.roundedRect(-4.2, -16.2, 8.4, 2.6, 1, 1);
            ctx.fill();
            ctx.beginPath();
            ctx.roundedRect(-3.1, -23.2, 6.2, 7.6, 2.6, 2.6);
            ctx.fill();
            ctx.restore();
            break;
        }
        case "more":
            for (const x of [5.5, 12, 18.5]) {
                ctx.beginPath(); ctx.arc(x, 12, 1.7, 0, 2 * Math.PI); ctx.fill();
            }
            break;
        case "settings": {
            // A gear: a ring with eight teeth around it and a hub in the middle.
            ctx.beginPath(); ctx.arc(12, 12, 6.2, 0, 2 * Math.PI); ctx.stroke();
            ctx.beginPath(); ctx.arc(12, 12, 2.4, 0, 2 * Math.PI); ctx.stroke();
            ctx.save();
            ctx.lineWidth = 3 / s;
            ctx.lineCap = "butt";
            for (let i = 0; i < 8; ++i) {
                const a = i * Math.PI / 4;
                ctx.beginPath();
                ctx.moveTo(12 + Math.cos(a) * 7.2, 12 + Math.sin(a) * 7.2);
                ctx.lineTo(12 + Math.cos(a) * 9.6, 12 + Math.sin(a) * 9.6);
                ctx.stroke();
            }
            ctx.restore();
            break;
        }
        case "theme":
            // Half-filled disc: light and dark.
            ctx.beginPath(); ctx.arc(12, 12, 8.5, 0, 2 * Math.PI); ctx.stroke();
            ctx.beginPath(); ctx.arc(12, 12, 8.5, -Math.PI / 2, Math.PI / 2); ctx.closePath(); ctx.fill();
            break;
        case "slideshow":
            ctx.beginPath(); ctx.moveTo(4, 5); ctx.lineTo(20, 5); ctx.lineTo(20, 15); ctx.lineTo(4, 15); ctx.closePath(); ctx.stroke();
            ctx.beginPath(); ctx.moveTo(10, 8); ctx.lineTo(14.5, 10); ctx.lineTo(10, 12); ctx.closePath(); ctx.fill();
            ctx.beginPath(); ctx.moveTo(8, 19); ctx.lineTo(16, 19); ctx.stroke();
            break;
        case "folder-batch":
            ctx.beginPath();
            ctx.moveTo(3, 7); ctx.lineTo(9, 7); ctx.lineTo(11, 9); ctx.lineTo(21, 9);
            ctx.lineTo(21, 19); ctx.lineTo(3, 19); ctx.closePath();
            ctx.stroke();
            ctx.beginPath(); ctx.moveTo(8, 14); ctx.lineTo(16, 14); ctx.stroke();
            break;
        case "lens":
            // A camera lens seen from the front: barrel, glass rings and a glint.
            ctx.beginPath(); ctx.arc(12, 12, 9, 0, 2 * Math.PI); ctx.stroke();
            ctx.beginPath(); ctx.arc(12, 12, 5.4, 0, 2 * Math.PI); ctx.stroke();
            ctx.beginPath(); ctx.arc(12, 12, 1.7, 0, 2 * Math.PI); ctx.fill();
            ctx.beginPath(); ctx.arc(12, 12, 7.2, 3.75, 4.5); ctx.stroke();
            break;
        case "collage":
            // Pictures of different sizes fitted into one frame.
            ctx.beginPath(); ctx.roundedRect(3, 3, 8, 11, 1.5, 1.5); ctx.stroke();
            ctx.beginPath(); ctx.roundedRect(13, 3, 8, 6, 1.5, 1.5); ctx.stroke();
            ctx.beginPath(); ctx.roundedRect(13, 11, 8, 10, 1.5, 1.5); ctx.stroke();
            ctx.beginPath(); ctx.roundedRect(3, 16, 8, 5, 1.5, 1.5); ctx.stroke();
            break;
        case "film":
            // A strip of film: the frame and its two rows of sprocket holes.
            ctx.beginPath(); ctx.roundedRect(4, 3, 16, 18, 2, 2); ctx.stroke();
            ctx.beginPath(); ctx.moveTo(8, 3); ctx.lineTo(8, 21); ctx.moveTo(16, 3); ctx.lineTo(16, 21); ctx.stroke();
            for (const y of [6.5, 10.5, 14.5, 18])
                ctx.fillRect(5.1, y - 0.9, 1.8, 1.8), ctx.fillRect(17.1, y - 0.9, 1.8, 1.8);
            break;
        case "multi":
            // Two pictures side by side (the "Varias imágenes" view).
            ctx.beginPath(); ctx.roundedRect(3, 5, 8, 14, 1.8, 1.8); ctx.stroke();
            ctx.beginPath(); ctx.roundedRect(13, 5, 8, 14, 1.8, 1.8); ctx.stroke();
            break;
        case "link":
        case "link-off": {
            // Two chain links; apart, and crossed by a slash, when the link is off.
            const gap = root.name === "link-off" ? 2.6 : 0;
            ctx.save();
            ctx.translate(12, 12);
            ctx.rotate(-Math.PI / 4);
            ctx.beginPath(); ctx.roundedRect(-10 - gap, -3.4, 10.4, 6.8, 3.4, 3.4); ctx.stroke();
            ctx.beginPath(); ctx.roundedRect(-0.4 + gap, -3.4, 10.4, 6.8, 3.4, 3.4); ctx.stroke();
            ctx.restore();
            if (root.name === "link-off") {
                ctx.beginPath(); ctx.moveTo(4, 20); ctx.lineTo(20, 4); ctx.stroke();
            }
            break;
        }
        case "split-h":
            ctx.beginPath(); ctx.roundedRect(3.5, 5, 17, 14, 2, 2); ctx.stroke();
            ctx.beginPath(); ctx.moveTo(12, 5); ctx.lineTo(12, 19); ctx.stroke();
            break;
        case "split-v":
            ctx.beginPath(); ctx.roundedRect(3.5, 5, 17, 14, 2, 2); ctx.stroke();
            ctx.beginPath(); ctx.moveTo(3.5, 12); ctx.lineTo(20.5, 12); ctx.stroke();
            break;
        case "plus":
            ctx.beginPath(); ctx.moveTo(12, 5); ctx.lineTo(12, 19); ctx.moveTo(5, 12); ctx.lineTo(19, 12); ctx.stroke();
            break;
        case "minus":
            ctx.beginPath(); ctx.moveTo(5, 12); ctx.lineTo(19, 12); ctx.stroke();
            break;
        case "frame":
            // A picture with a rounded frame around it.
            ctx.beginPath(); ctx.roundedRect(3, 3, 18, 18, 5, 5); ctx.stroke();
            ctx.beginPath(); ctx.roundedRect(7.5, 7.5, 9, 9, 2, 2); ctx.stroke();
            break;
        // The shapes a picture can be cut to (Recortar > Marco), each outlined on the same grid.
        case "frame-rect":
            ctx.beginPath(); ctx.roundedRect(3.5, 5, 17, 14, 4, 4); ctx.stroke();
            break;
        case "frame-ellipse": {
            const pts = [];
            for (let i = 0; i < 48; ++i)
                pts.push(12 + 9 * Math.cos(i * Math.PI / 24), 12 + 7 * Math.sin(i * Math.PI / 24));
            closedShape(pts);
            break;
        }
        case "frame-hexagon":
            closedShape([3, 12, 7.5, 4.5, 16.5, 4.5, 21, 12, 16.5, 19.5, 7.5, 19.5]);
            break;
        case "frame-octagon":
            closedShape([8.7, 4, 15.3, 4, 20, 8.7, 20, 15.3, 15.3, 20, 8.7, 20, 4, 15.3, 4, 8.7]);
            break;
        case "frame-diamond":
            closedShape([12, 3, 21, 12, 12, 21, 3, 12]);
            break;
        case "frame-triangle":
            closedShape([12, 4, 20.5, 19.5, 3.5, 19.5]);
            break;
        case "frame-star": {
            const pts = [];
            for (let i = 0; i < 10; ++i) {
                const a = -Math.PI / 2 + i * Math.PI / 5, r = i % 2 === 0 ? 9.5 : 3.9;
                pts.push(12 + r * Math.cos(a), 12.8 + r * Math.sin(a));
            }
            closedShape(pts);
            break;
        }
        case "frame-heart": {
            const pts = [];
            for (let i = 0; i < 60; ++i) {
                const t = i * 2 * Math.PI / 60;
                const x = 16 * Math.pow(Math.sin(t), 3);
                const y = 13 * Math.cos(t) - 5 * Math.cos(2 * t) - 2 * Math.cos(3 * t) - Math.cos(4 * t);
                pts.push(12 + x * 0.58, 11.4 - y * 0.58 + 0.6);
            }
            closedShape(pts);
            break;
        }
        // One corner of a shape with each way of cutting it.
        case "corner-round":
            ctx.beginPath(); ctx.moveTo(4, 20); ctx.lineTo(4, 13); ctx.arcTo(4, 4, 13, 4, 9); ctx.lineTo(20, 4); ctx.stroke();
            break;
        case "corner-soft":
            ctx.beginPath(); ctx.moveTo(4, 20); ctx.lineTo(4, 16); ctx.bezierCurveTo(4, 7, 7, 4, 16, 4); ctx.lineTo(20, 4); ctx.stroke();
            break;
        case "corner-cut":
            ctx.beginPath(); ctx.moveTo(4, 20); ctx.lineTo(4, 12); ctx.lineTo(12, 4); ctx.lineTo(20, 4); ctx.stroke();
            break;
        case "corner-hollow":
            ctx.beginPath(); ctx.moveTo(4, 20); ctx.lineTo(4, 12); ctx.arc(4, 4, 8, Math.PI / 2, 0, true); ctx.lineTo(20, 4); ctx.stroke();
            break;
        case "filters":
            // Three overlapping discs, the usual "photo filters" sign.
            for (const c of [[12, 8.6], [8.3, 15.2], [15.7, 15.2]]) {
                ctx.beginPath();
                ctx.arc(c[0], c[1], 5, 0, 2 * Math.PI);
                ctx.stroke();
            }
            break;
        }
        ctx.restore();
    }
}
