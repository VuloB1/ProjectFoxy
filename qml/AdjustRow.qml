import QtQuick
import QtQuick.Controls
import ImageViewerApp

// One Ajustes slider: caption + current value on top, the slider underneath.
// Reads its value from appController.adjust[param] and writes it back through
// setAdjustParam, so it stays in step with everything else that changes the
// adjustments (Auto, Restablecer, a new image, undoing everything). Double-
// click the caption to put it back to neutral.
Item {
    id: root

    property string param: ""
    property string label: ""
    property real from: -1
    property real to: 1
    property real neutral: 0
    // The number shown next to the caption is value * displayScale
    // (-1..1 reads as -100..100).
    property real displayScale: 100
    property string displaySuffix: ""
    // Replaces the number shown (a function of the model value -> text), e.g. a temperature in kelvin.
    property var formatter: null
    // Three colours the slider's groove is painted with (left, middle, right).
    property var trackColors: []
    // The slider runs the other way round: its left end is the model's `to`.
    property bool inverted: false

    implicitHeight: 42

    readonly property real modelValue: {
        const v = appController.adjust[root.param];
        return v === undefined ? root.neutral : v;
    }
    readonly property bool modified: Math.abs(modelValue - neutral) > 0.0005

    function formatted(v) {
        if (root.formatter)
            return root.formatter(v);
        const n = Math.round(v * root.displayScale);
        return (n > 0 && root.from < 0 ? "+" : "") + n + root.displaySuffix;
    }

    Label {
        id: caption
        anchors.left: parent.left
        anchors.top: parent.top
        text: root.label
        color: themeManager.textSecondary
    }
    Label {
        anchors.right: parent.right
        anchors.top: parent.top
        text: root.formatted(root.inverted ? -slider.value : slider.value)
        color: root.modified ? themeManager.accent : themeManager.textPrimary
        font.bold: true
    }
    MouseArea {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: caption.height
        hoverEnabled: true
        onDoubleClicked: appController.setAdjustParam(root.param, root.neutral)

        AppToolTip {
            visible: parent.containsMouse && root.modified
            delay: 700
            text: qsTr("Doble clic para restablecer")
        }
    }

    AppSlider {
        id: slider
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: caption.bottom
        anchors.topMargin: 2
        from: root.inverted ? -root.to : root.from
        to: root.inverted ? -root.from : root.to
        value: root.inverted ? -root.neutral : root.neutral
        fillFrom: root.inverted ? -root.neutral : root.neutral
        trackColors: root.trackColors

        onMoved: {
            // "Magnetic" neutral: land within 2% of the range of it and it
            // snaps exactly onto it. Done here (not via AppSlider's own
            // snapThreshold) so the snapped value is what gets sent.
            let v = root.inverted ? -value : value;
            if (Math.abs(v - root.neutral) <= (root.to - root.from) * 0.02) {
                v = root.neutral;
                value = root.inverted ? -v : v;
            }
            appController.setAdjustParam(root.param, v);
        }
    }

    // Follow changes that did not come from this slider (Auto buttons, reset,
    // a new image). Skipped when already in step so dragging never fights it.
    function syncFromModel() {
        const shown = root.inverted ? -root.modelValue : root.modelValue;
        if (Math.abs(slider.value - shown) > 0.0005)
            slider.value = shown;
    }
    Connections {
        target: appController
        function onLiveAdjustChanged() { root.syncFromModel(); }
    }
    Component.onCompleted: syncFromModel()
}
