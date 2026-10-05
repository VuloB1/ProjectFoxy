import QtQuick
import QtQuick.Controls
import ImageViewerApp

// The handles an effect asks for (appController.effectOverlays) drawn over the picture: a point to
// drag (the centre of a vignette or a gradient), or a guide line (where to stretch). Dragging one
// writes the effect's parameters, so the preview follows. The positions are fractions of the
// picture, so they stay on the right spot while the view is zoomed or panned.
Item {
    id: root

    property Item imageItem: null
    property Item flickItem: null

    readonly property var overlays: appController.effectOverlays
    readonly property var values: appController.effectValues

    // A point of the picture, given as fractions (0..1), in this item's coordinates.
    function viewPoint(fx, fy) {
        // Read so that whoever calls this from a binding is told when the view moves.
        flickItem.contentX; flickItem.contentY; flickItem.contentWidth; flickItem.contentHeight;
        imageItem.scale; imageItem.width; imageItem.height; root.width; root.height;
        return imageItem.mapToItem(root, fx * imageItem.width, fy * imageItem.height);
    }

    // The guides of "Estirar" are set on the ORIGINAL picture, but what is on show is the stretched
    // one: the band between the guides has grown or shrunk and the picture with it. These work out
    // where the two guides of one axis (parameters ia, ib: positions in %, is: how much the band is
    // scaled in %) are on the picture as shown, and the reverse for dragging.
    function stretchState(ia, ib, is) {
        const g1 = (root.values[ia] || 0) / 100;
        const g2 = Math.max((root.values[ib] || 0) / 100, g1 + 0.02);
        const s = Math.max(0.05, (root.values[is] || 100) / 100);
        const keep = (root.values[7] || 0) >= 0.5;
        const inner = (g2 - g1) * s;
        const outer = 1 - (g2 - g1);
        const f = keep && outer > 1e-6 ? Math.max(0.05, (1 - inner) / outer) : 1;
        const total = keep ? 1 : 1 + (g2 - g1) * (s - 1);
        const p1 = keep ? g1 * f : g1 / total;
        const p2 = keep ? p1 + inner : (g1 + inner) / total;
        return { g1: g1, g2: g2, s: s, keep: keep, f: f, total: total, p1: p1, p2: p2 };
    }
    // The fraction of the picture as shown at which an overlay sits along its axis.
    function shownFraction(m, axisX) {
        if (m.mapping === "stretchX" || m.mapping === "stretchY") {
            const st = m.mapping === "stretchX" ? stretchState(0, 1, 4) : stretchState(2, 3, 5);
            const first = (m.mapping === "stretchX" ? m.x : m.y) === (m.mapping === "stretchX" ? 0 : 2);
            return first ? st.p1 : st.p2;
        }
        const index = axisX ? m.x : m.y, lo = axisX ? m.xmin : m.ymin, hi = axisX ? m.xmax : m.ymax;
        const v = root.values[index];
        if (v === undefined || hi === lo)
            return 0.5;
        return Math.min(1, Math.max(0, (v - lo) / (hi - lo)));
    }
    // Moves an overlay so that it sits at fraction `d` of the picture as shown, along its axis.
    function dragTo(m, axisX, d) {
        d = Math.min(1, Math.max(0, d));
        if (m.mapping === "stretchX" || m.mapping === "stretchY") {
            const x = m.mapping === "stretchX";
            const st = x ? stretchState(0, 1, 4) : stretchState(2, 3, 5);
            const first = (x ? m.x : m.y) === (x ? 0 : 2);
            let g1 = st.g1, g2 = st.g2;
            if (first) {
                g1 = st.keep ? d / st.f : d * st.total;
                g1 = Math.min(g1, g2 - 0.02);
            } else if (st.keep) {
                g2 = g1 + (d - st.p1) / st.s;
            } else {
                g2 = g1 + (d * st.total - g1) / st.s;
            }
            g1 = Math.min(1, Math.max(0, g1));
            g2 = Math.min(1, Math.max(g1 + 0.02, g2));
            appController.setEffectValue(x ? 0 : 2, g1 * 100);
            appController.setEffectValue(x ? 1 : 3, g2 * 100);
            return;
        }
        const index = axisX ? m.x : m.y, lo = axisX ? m.xmin : m.ymin, hi = axisX ? m.xmax : m.ymax;
        appController.setEffectValue(index, lo + d * (hi - lo));
    }

    Repeater {
        model: root.overlays
        delegate: Item {
            id: overlay
            required property var modelData
            readonly property bool isPoint: modelData.kind === "point"
            readonly property bool isV: modelData.kind === "vline"
            readonly property real fx: isPoint || isV ? root.shownFraction(modelData, true) : 0.5
            readonly property real fy: isPoint ? root.shownFraction(modelData, false)
                                       : !isV ? root.shownFraction(modelData, false) : 0.5

            // -- a guide line across the whole picture (vertical or horizontal)
            Rectangle {
                visible: !overlay.isPoint
                readonly property point a: root.viewPoint(overlay.isV ? overlay.fx : 0, overlay.isV ? 0 : overlay.fy)
                readonly property point b: root.viewPoint(overlay.isV ? overlay.fx : 1, overlay.isV ? 1 : overlay.fy)
                x: Math.min(a.x, b.x) - (overlay.isV ? 1 : 0)
                y: Math.min(a.y, b.y) - (overlay.isV ? 0 : 1)
                width: overlay.isV ? 2 : Math.abs(b.x - a.x)
                height: overlay.isV ? Math.abs(b.y - a.y) : 2
                color: themeManager.accent
                opacity: 0.9
            }

            // -- the handle: a dot for a point, a grip on a guide
            Rectangle {
                id: grip
                readonly property point at: root.viewPoint(overlay.isPoint ? overlay.fx : (overlay.isV ? overlay.fx : 0.5),
                                                           overlay.isPoint ? overlay.fy : (overlay.isV ? 0.5 : overlay.fy))
                width: overlay.isPoint ? 24 : (overlay.isV ? 14 : 34)
                height: overlay.isPoint ? 24 : (overlay.isV ? 34 : 14)
                radius: overlay.isPoint ? 12 : 7
                x: at.x - width / 2
                y: at.y - height / 2
                color: themeManager.accent
                border.width: 2
                border.color: "white"

                // crosshair inside a point's dot
                Rectangle { visible: overlay.isPoint; anchors.centerIn: parent; width: 10; height: 2; color: "white" }
                Rectangle { visible: overlay.isPoint; anchors.centerIn: parent; width: 2; height: 10; color: "white" }

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -8
                    hoverEnabled: true
                    cursorShape: overlay.isPoint ? Qt.SizeAllCursor : (overlay.isV ? Qt.SizeHorCursor : Qt.SizeVerCursor)
                    onPositionChanged: function (mouse) {
                        if (!pressed)
                            return;
                        const p = mapToItem(root.imageItem, mouse.x, mouse.y);
                        const fx = p.x / root.imageItem.width, fy = p.y / root.imageItem.height;
                        if (overlay.isPoint || overlay.isV)
                            root.dragTo(overlay.modelData, true, fx);
                        if (overlay.isPoint || !overlay.isV)
                            root.dragTo(overlay.modelData, false, fy);
                    }
                    AppToolTip { visible: parent.containsMouse && overlay.modelData.label.length > 0; text: overlay.modelData.label }
                }
            }
        }
    }
}
