import QtQuick
import QtQuick.Controls
import ImageViewerApp

// The crop frame over the video preview (qml/VideoToGifPage.qml). It lives in the rectangle `area` where the picture is
// painted and keeps its place as fractions of the picture (cx, cy, cw, ch), so the page reads it as it is. Drag the frame to
// move it, drag a corner to make it bigger or smaller; with a fixed proportion (`aspect`) the corners keep it.
Item {
    id: root

    // where the picture is painted, in this item's coordinates, and its size in real pixels
    property rect area: Qt.rect(0, 0, width, height)
    property int srcWidth: 16
    property int srcHeight: 9
    // width / height of the frame in real pixels; 0 = free
    property real aspect: 0

    // the frame, as fractions of the picture
    property real cx: 0
    property real cy: 0
    property real cw: 1
    property real ch: 1
    readonly property bool whole: cw > 0.999 && ch > 0.999
    readonly property real minSize: 0.06

    // the proportion in the picture's own fractions: cw / ch = aspect * srcHeight / srcWidth
    readonly property real ratio: aspect > 0 ? aspect * srcHeight / srcWidth : 0

    function reset() { cx = 0; cy = 0; cw = 1; ch = 1; }

    // The largest frame of the proportion that fits, around the middle of the current one.
    function applyAspect(newAspect) {
        aspect = newAspect;
        if (newAspect <= 0)
            return;
        const k = ratio;
        let w = k <= 1 ? k : 1, h = k <= 1 ? 1 : 1 / k;
        const midX = cx + cw / 2, midY = cy + ch / 2;
        cw = w; ch = h;
        cx = Math.max(0, Math.min(1 - w, midX - w / 2));
        cy = Math.max(0, Math.min(1 - h, midY - h / 2));
    }

    // the frame in this item's pixels
    readonly property real boxX: area.x + cx * area.width
    readonly property real boxY: area.y + cy * area.height
    readonly property real boxW: cw * area.width
    readonly property real boxH: ch * area.height

    // outside the frame: dimmed
    Repeater {
        model: root.whole ? [] : [
            { x: root.area.x, y: root.area.y, w: root.area.width, h: root.boxY - root.area.y },
            { x: root.area.x, y: root.boxY + root.boxH, w: root.area.width, h: root.area.y + root.area.height - root.boxY - root.boxH },
            { x: root.area.x, y: root.boxY, w: root.boxX - root.area.x, h: root.boxH },
            { x: root.boxX + root.boxW, y: root.boxY, w: root.area.x + root.area.width - root.boxX - root.boxW, h: root.boxH }
        ]
        delegate: Rectangle {
            required property var modelData
            x: modelData.x; y: modelData.y; width: Math.max(0, modelData.w); height: Math.max(0, modelData.h)
            color: "#000000"
            opacity: 0.6
        }
    }

    Rectangle {
        id: frame
        visible: !root.whole
        x: root.boxX; y: root.boxY; width: root.boxW; height: root.boxH
        color: "transparent"
        border.color: "#f2c230"
        border.width: 2

        // thirds, to help placing the subject
        Repeater {
            model: 2
            delegate: Item {
                required property int index
                Rectangle { x: frame.width * (index + 1) / 3; width: 1; height: frame.height; color: "#f2c230"; opacity: 0.35 }
                Rectangle { y: frame.height * (index + 1) / 3; height: 1; width: frame.width; color: "#f2c230"; opacity: 0.35 }
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            property real startX: 0
            property real startY: 0
            property real startCx: 0
            property real startCy: 0
            onPressed: function (m) {
                const p = mapToItem(root, m.x, m.y);
                startX = p.x; startY = p.y; startCx = root.cx; startCy = root.cy;
            }
            onPositionChanged: function (m) {
                if (!pressed || root.area.width <= 0)
                    return;
                const p = mapToItem(root, m.x, m.y);
                root.cx = Math.max(0, Math.min(1 - root.cw, startCx + (p.x - startX) / root.area.width));
                root.cy = Math.max(0, Math.min(1 - root.ch, startCy + (p.y - startY) / root.area.height));
            }
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 4
            text: Math.round(root.cw * root.srcWidth) + " × " + Math.round(root.ch * root.srcHeight)
            color: "#f2c230"
            font.pixelSize: 11
            style: Text.Outline
            styleColor: "#000000"
        }
    }

    // the four corners
    Repeater {
        model: root.whole ? [] : [{ h: 0, v: 0 }, { h: 1, v: 0 }, { h: 0, v: 1 }, { h: 1, v: 1 }]
        delegate: Rectangle {
            id: grip
            required property var modelData
            width: 14; height: 14
            radius: 3
            color: "#f2c230"
            border.color: "#000000"
            border.width: 1
            x: root.boxX + modelData.h * root.boxW - width / 2
            y: root.boxY + modelData.v * root.boxH - height / 2

            MouseArea {
                anchors.fill: parent
                anchors.margins: -6
                cursorShape: (grip.modelData.h === grip.modelData.v) ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
                onPositionChanged: function (m) {
                    if (!pressed || root.area.width <= 0)
                        return;
                    const p = mapToItem(root, m.x, m.y);
                    root.dragCorner(grip.modelData.h, grip.modelData.v,
                                    (p.x - root.area.x) / root.area.width, (p.y - root.area.y) / root.area.height);
                }
            }
        }
    }

    // Moves the corner (h, v: 0 = left/top, 1 = right/bottom) to the pointer (px, py as fractions of the picture); the
    // opposite corner stays where it is.
    function dragCorner(h, v, px, py) {
        const ox = h === 1 ? cx : cx + cw;
        const oy = v === 1 ? cy : cy + ch;
        px = Math.max(0, Math.min(1, px));
        py = Math.max(0, Math.min(1, py));
        let w = Math.abs(px - ox), hh = Math.abs(py - oy);
        const dirX = px >= ox ? 1 : -1, dirY = py >= oy ? 1 : -1;
        // how far the frame may grow from that corner before it leaves the picture
        const roomX = dirX > 0 ? 1 - ox : ox, roomY = dirY > 0 ? 1 - oy : oy;
        if (ratio > 0) {
            // follow the pointer by the side that asks for more, keep the proportion
            let nw = Math.max(w, hh * ratio);
            nw = Math.min(nw, roomX, roomY * ratio);
            nw = Math.max(nw, minSize);
            w = nw;
            hh = nw / ratio;
            if (hh > roomY) { hh = roomY; w = hh * ratio; }
        } else {
            w = Math.max(minSize, Math.min(w, roomX));
            hh = Math.max(minSize, Math.min(hh, roomY));
        }
        const nx = dirX > 0 ? ox : ox - w, ny = dirY > 0 ? oy : oy - hh;
        cw = w; ch = hh; cx = nx; cy = ny;
    }
}
