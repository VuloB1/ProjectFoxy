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
    function fractionOf(index, lo, hi) {
        const v = root.values[index];
        if (v === undefined || hi === lo)
            return 0.5;
        return Math.min(1, Math.max(0, (v - lo) / (hi - lo)));
    }

    Repeater {
        model: root.overlays
        delegate: Item {
            id: overlay
            required property var modelData
            readonly property bool isPoint: modelData.kind === "point"
            readonly property bool isV: modelData.kind === "vline"
            readonly property real fx: isPoint || isV ? root.fractionOf(modelData.x, modelData.xmin, modelData.xmax) : 0.5
            readonly property real fy: isPoint ? root.fractionOf(modelData.y, modelData.ymin, modelData.ymax)
                                       : !isV ? root.fractionOf(modelData.y, modelData.ymin, modelData.ymax) : 0.5

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
                        const fx = Math.min(1, Math.max(0, p.x / root.imageItem.width));
                        const fy = Math.min(1, Math.max(0, p.y / root.imageItem.height));
                        if (overlay.isPoint || overlay.isV)
                            appController.setEffectValue(overlay.modelData.x,
                                overlay.modelData.xmin + fx * (overlay.modelData.xmax - overlay.modelData.xmin));
                        if (overlay.isPoint || !overlay.isV)
                            appController.setEffectValue(overlay.modelData.y,
                                overlay.modelData.ymin + fy * (overlay.modelData.ymax - overlay.modelData.ymin));
                    }
                    AppToolTip { visible: parent.containsMouse && overlay.modelData.label.length > 0; text: overlay.modelData.label }
                }
            }
        }
    }
}
