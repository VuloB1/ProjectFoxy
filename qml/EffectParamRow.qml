import QtQuick
import QtQuick.Controls
import ImageViewerApp

// One slider (or on/off switch) of the effect picked in the Efectos tab. What it
// is - caption, range, suffix - comes from `spec`, one entry of
// appController.effectParams; its current value is appController.effectValues
// [paramIndex], written back through setEffectValue. Double-click the caption to put
// it back to the effect's default.
Item {
    id: root

    // Which of the effect's sliders this row is (0..2). Not called `index`: the
    // Repeater that creates the rows hands every delegate its own `index`, which
    // would shadow this property (every row then reads and writes slider 0).
    property int paramIndex: 0
    property var spec: ({ label: "", min: 0, max: 1, def: 0, suffix: "", integer: false, toggle: false, options: [] })
    readonly property bool isChoice: spec.options !== undefined && spec.options.length > 0

    implicitHeight: spec.toggle ? 30 : isChoice ? 62 : 42

    readonly property real modelValue: {
        const v = appController.effectValues[root.paramIndex];
        return v === undefined ? root.spec.def : v;
    }
    readonly property bool modified: Math.abs(modelValue - spec.def) > 0.0005

    function formatted(v) {
        const n = Math.round(v);
        return (n > 0 && root.spec.min < 0 ? "+" : "") + n + (root.spec.suffix || "");
    }

    // Follow changes that did not come from this control (Restablecer, picking
    // another effect). Skipped when already in step so dragging never fights it.
    function syncFromModel() {
        if (root.isChoice) {
            return; // the buttons read modelValue directly
        } else if (root.spec.toggle) {
            const on = root.modelValue >= 0.5;
            if (toggleBox.checked !== on)
                toggleBox.checked = on;
        } else if (Math.abs(slider.value - root.modelValue) > 0.0005) {
            slider.value = root.modelValue;
        }
    }
    Connections {
        target: appController
        function onEffectValuesChanged() { root.syncFromModel(); }
    }
    Component.onCompleted: syncFromModel()

    // --- on/off switch ----------------------------------------------------
    AppCheckBox {
        id: toggleBox
        visible: root.spec.toggle
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        text: root.spec.label
        onToggled: appController.setEffectValue(root.paramIndex, checked ? 1 : 0)
    }

    // --- a choice among a few named options --------------------------------
    Item {
        anchors.fill: parent
        visible: root.isChoice

        Label {
            id: choiceCaption
            anchors.left: parent.left
            anchors.top: parent.top
            text: root.spec.label
            color: themeManager.textSecondary
        }
        Flow {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: choiceCaption.bottom
            anchors.topMargin: 4
            spacing: 4
            Repeater {
                model: root.isChoice ? root.spec.options : []
                delegate: AppToolButton {
                    required property int index
                    required property string modelData
                    text: modelData
                    checked: Math.round(root.modelValue) === index
                    onClicked: appController.setEffectValue(root.paramIndex, index)
                }
            }
        }
    }

    // --- slider -----------------------------------------------------------
    Item {
        anchors.fill: parent
        visible: !root.spec.toggle && !root.isChoice

        Label {
            id: caption
            anchors.left: parent.left
            anchors.top: parent.top
            text: root.spec.label
            color: themeManager.textSecondary
        }
        Label {
            anchors.right: parent.right
            anchors.top: parent.top
            text: root.formatted(slider.value)
            color: root.modified ? themeManager.accent : themeManager.textPrimary
            font.bold: true
        }
        MouseArea {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: caption.height
            hoverEnabled: true
            onDoubleClicked: appController.setEffectValue(root.paramIndex, root.spec.def)

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
            from: root.spec.min
            to: root.spec.max
            value: root.spec.def
            fillFrom: root.spec.min < 0 ? 0 : root.spec.min
            stepSize: root.spec.integer ? 1 : 0
            snapMode: root.spec.integer ? Slider.SnapAlways : Slider.NoSnap
            onMoved: appController.setEffectValue(root.paramIndex, value)
        }
    }
}
