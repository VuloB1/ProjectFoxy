import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Recortar: the crop frame, the quarter turns / mirrors and the fine "Enderezar" slider - and, on its
// second page, Marco: cutting the picture to a shape and putting an outline, shadow and background around it.
// Everything here only takes effect through the panel's Aplicar (a pending straighten, else the drawn crop
// frame, else the frame being previewed; turns and mirrors are applied the moment they are pressed, but
// count as part of the session, so Cancelar undoes them too).
Item {
    id: root

    property var canvas: null

    // "crop" or "frame"
    property string mode: "crop"

    readonly property bool canApply: mode === "frame"
        ? (appController.effectId === "frame" && !appController.effectBusy)
        : (appController.toolSessionDirty || straightenSlider.value !== 0 || (canvas !== null && canvas.cropHasSelection))

    // Takes in whatever the crop page has pending: a tilt that was dialled in, else the frame that was drawn.
    function settleCrop() {
        if (!canvas)
            return;
        if (straightenSlider.value !== 0) {
            canvas.commitStraighten(straightenSlider.value);
            canvas.straightenActive = false;
            straightenSlider.value = 0;
        } else if (canvas.cropHasSelection) {
            canvas.confirmCrop();
        }
        if (canvas.cropActive)
            canvas.cancelCrop();
    }

    function apply() {
        if (!canvas)
            return;
        if (mode === "frame") {
            appController.commitEffect();
            return;
        }
        settleCrop();
    }

    function cancel() {
        if (!canvas)
            return;
        appController.cancelEffect();
        canvas.cancelStraighten();
        canvas.straightenActive = false;
        straightenSlider.value = 0;
        canvas.cancelCrop();
    }

    // Marco works on the picture as it is: what was drawn or dialled in on the first page is applied
    // first (Cancelar of the tool still takes all of it back).
    function setMode(next) {
        if (next === mode || !canvas)
            return;
        if (next === "frame") {
            settleCrop();
            mode = "frame";
            appController.selectEffect("frame");
        } else {
            appController.cancelEffect();
            mode = "crop";
            canvas.startCrop(0, 0);
        }
    }

    Component.onCompleted: if (canvas) canvas.startCrop(0, 0) // freeform by default

    Row {
        id: modeRow
        spacing: 4
        AppToolButton { text: qsTr("Recorte"); checked: root.mode === "crop"; onClicked: root.setMode("crop") }
        AppToolButton { text: qsTr("Marco"); checked: root.mode === "frame"; onClicked: root.setMode("frame") }
    }

    Component { id: frameComponent; FramePanel {} }
    Loader {
        id: framePage
        anchors.top: modeRow.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        active: root.mode === "frame"
        sourceComponent: frameComponent
    }

    // --- Recortar: aspect-ratio presets + rotate/flip -----------------
    Column {
        id: cropPage
        anchors.top: modeRow.bottom
        anchors.topMargin: 10
        width: root.width
        spacing: 10
        visible: root.mode === "crop"

        Label { text: qsTr("Recorte - arrastrá el rectángulo o sus bordes/esquinas sobre la imagen"); color: themeManager.textSecondary; wrapMode: Text.WordWrap; width: parent.width }
        // Icon-only (the text becomes the hover tooltip): each frame
        // is drawn at its own proportions, so the shape says which ratio
        // it is.
        // The frame the crop is using right now stays lit.
        Row {
            spacing: 6
            width: parent.width
            readonly property real cell: (width - spacing * 4) / 5
            readonly property bool free: root.canvas ? root.canvas.cropFreeform : true
            function isRatio(w, h) {
                return root.canvas !== null && !root.canvas.cropFreeform
                       && root.canvas.cropAspectW === w && root.canvas.cropAspectH === h;
            }
            AppButton { text: qsTr("Libre"); iconName: "aspect-free"; iconOnly: true; width: parent.cell; checked: parent.free; onClicked: if (root.canvas) root.canvas.setCropAspect(0, 0) }
            AppButton { text: qsTr("1:1"); iconName: "aspect-1-1"; iconOnly: true; width: parent.cell; checked: parent.isRatio(1, 1); onClicked: if (root.canvas) root.canvas.setCropAspect(1, 1) }
            AppButton { text: qsTr("4:3"); iconName: "aspect-4-3"; iconOnly: true; width: parent.cell; checked: parent.isRatio(4, 3); onClicked: if (root.canvas) root.canvas.setCropAspect(4, 3) }
            AppButton { text: qsTr("16:9"); iconName: "aspect-16-9"; iconOnly: true; width: parent.cell; checked: parent.isRatio(16, 9); onClicked: if (root.canvas) root.canvas.setCropAspect(16, 9) }
            AppButton { text: qsTr("3:4"); iconName: "aspect-3-4"; iconOnly: true; width: parent.cell; checked: parent.isRatio(3, 4); onClicked: if (root.canvas) root.canvas.setCropAspect(3, 4) }
        }

        Label { text: qsTr("Transformar"); color: themeManager.textSecondary }
        // Each one plays its own motion on the canvas (quarter turn /
        // mirror) before the edit is applied - see
        // ImageCanvas.playEditTransform(), which ignores clicks while one
        // is already in flight.
        Row {
            spacing: 6
            width: parent.width
            readonly property real cell: (width - spacing * 3) / 4
            AppButton { text: qsTr("Rotar a la izquierda"); iconName: "rotate-left"; iconOnly: true; width: parent.cell; onClicked: root.canvas.playEditTransform("rotateL") }
            AppButton { text: qsTr("Rotar a la derecha"); iconName: "rotate-right"; iconOnly: true; width: parent.cell; onClicked: root.canvas.playEditTransform("rotate") }
            AppButton { text: qsTr("Voltear horizontal"); iconName: "flip-h"; iconOnly: true; width: parent.cell; onClicked: root.canvas.playEditTransform("flipH") }
            AppButton { text: qsTr("Voltear vertical"); iconName: "flip-v"; iconOnly: true; width: parent.cell; onClicked: root.canvas.playEditTransform("flipV") }
        }

        Row {
            width: parent.width
            Label { text: qsTr("Enderezar"); color: themeManager.textSecondary; width: parent.width - straightenValueLabel.width }
            Label {
                id: straightenValueLabel
                text: (straightenSlider.value > 0 ? "+" : "") + Math.round(straightenSlider.value) + "°"
                color: themeManager.textPrimary
                font.bold: true
            }
        }
        AppSlider {
            id: straightenSlider
            width: parent.width
            from: -45; to: 45; value: 0
            // Magnetic zero (a few degrees wide) with a tick on the groove to show where it is.
            snapThreshold: 3
            neutralMark: true
            onMoved: {
                if (!root.canvas) return;
                // The guide lines are only there while the picture is actually tilted.
                root.canvas.straightenActive = value !== 0;
                root.canvas.previewStraighten(value);
            }
        }
    }
}
