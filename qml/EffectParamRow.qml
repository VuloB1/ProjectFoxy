import QtQuick
import QtQuick.Controls
import ImageViewerApp

// One control of the effect picked in the Efectos tab: a slider, an on/off switch, a choice (a
// few buttons, or a drop-down when there are many), a colour or a random seed. What it is - caption,
// range, suffix - comes from `spec`, one entry of appController.effectParams; its current value is
// appController.effectValues[paramIndex], written back through setEffectValue. Double-click the
// caption to put it back to the effect's default. A control that only makes sense for some choice
// of another one (spec.dependsOn / dependsMask) hides itself the rest of the time.
Item {
    id: root

    // Which of the effect's controls this row is. Not called `index`: the Repeater that creates the
    // rows hands every delegate its own `index`, which would shadow this property (every row then
    // reads and writes slider 0).
    property int paramIndex: 0
    property var spec: ({ label: "", min: 0, max: 1, def: 0, suffix: "", integer: false, toggle: false, options: [],
                          color: false, seed: false, dependsOn: -1, dependsMask: 0, hint: "" })
    readonly property bool isChoice: spec.options !== undefined && spec.options.length > 0
    readonly property bool isColor: spec.color === true
    readonly property bool isSeed: spec.seed === true
    // Few short names read best as buttons; many (or long) ones as a drop-down.
    readonly property bool useCombo: isChoice && (spec.options.length > 5 || spec.options.join("").length > 36)
    readonly property bool isSlider: !spec.toggle && !isChoice && !isColor

    // Hidden while the control it depends on has another value.
    readonly property bool shown: {
        if (spec.dependsOn === undefined || spec.dependsOn < 0)
            return true;
        const v = appController.effectValues[spec.dependsOn];
        const controlling = v === undefined ? 0 : Math.round(v);
        return ((spec.dependsMask >> controlling) & 1) === 1;
    }
    visible: shown
    implicitHeight: !shown ? 0
        : spec.toggle ? 30
        : isColor ? 34
        : useCombo ? 58
        : isChoice ? choiceCaption.implicitHeight + 8 + choiceFlow.childrenRect.height
        : 42

    readonly property real modelValue: {
        const v = appController.effectValues[root.paramIndex];
        return v === undefined ? root.spec.def : v;
    }
    readonly property bool modified: Math.abs(modelValue - spec.def) > 0.0005

    function formatted(v) {
        const n = Math.round(v);
        return (n > 0 && root.spec.min < 0 ? "+" : "") + n + (root.spec.suffix || "");
    }
    function colorOf(v) {
        const n = Math.round(v);
        return Qt.rgba(((n >> 16) & 255) / 255, ((n >> 8) & 255) / 255, (n & 255) / 255, 1);
    }

    // Follow changes that did not come from this control (Restablecer, picking
    // another effect). Skipped when already in step so dragging never fights it.
    function syncFromModel() {
        if (root.isChoice || root.isColor) {
            return; // these read modelValue directly
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
        visible: root.shown && root.spec.toggle
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        text: root.spec.label
        onToggled: appController.setEffectValue(root.paramIndex, checked ? 1 : 0)
    }

    // --- a colour -------------------------------------------------------------
    Item {
        anchors.fill: parent
        visible: root.shown && root.isColor

        EffectCaption {
            id: colorCaption
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - colorWell.width - 10
            text: root.spec.label
            paramIndex: root.paramIndex; defaultValue: root.spec.def; modified: root.modified; hint: root.spec.hint
        }
        AppColorSwatch {
            id: colorWell
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 76
            height: 26
            value: root.colorOf(root.modelValue)
            onPicked: function (c) {
                appController.setEffectValue(root.paramIndex,
                    (Math.round(c.r * 255) << 16) | (Math.round(c.g * 255) << 8) | Math.round(c.b * 255));
            }
        }
    }

    // --- a choice among a few named options -----------------------------------
    Item {
        anchors.fill: parent
        visible: root.shown && root.isChoice && !root.useCombo

        EffectCaption {
            id: choiceCaption
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            text: root.spec.label
            paramIndex: root.paramIndex; defaultValue: root.spec.def; modified: root.modified; hint: root.spec.hint
        }
        Flow {
            id: choiceFlow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: choiceCaption.bottom
            anchors.topMargin: 4
            spacing: 4
            Repeater {
                model: root.isChoice && !root.useCombo ? root.spec.options : []
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

    // --- a choice among many: a drop-down ---------------------------------------
    Item {
        anchors.fill: parent
        visible: root.shown && root.useCombo

        EffectCaption {
            id: comboCaption
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            text: root.spec.label
            paramIndex: root.paramIndex; defaultValue: root.spec.def; modified: root.modified; hint: root.spec.hint
        }
        AppComboBox {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: comboCaption.bottom
            anchors.topMargin: 4
            height: 30
            model: root.useCombo ? root.spec.options : []
            currentIndex: Math.round(root.modelValue)
            onActivated: function (i) { appController.setEffectValue(root.paramIndex, i); }
        }
    }

    // --- slider (with a dice button when it is a random seed) ---------------------
    Item {
        anchors.fill: parent
        visible: root.shown && root.isSlider

        EffectCaption {
            id: caption
            anchors.left: parent.left
            anchors.top: parent.top
            width: parent.width - valueLabel.implicitWidth - (root.isSeed ? diceButton.width + 12 : 0) - 8
            text: root.spec.label
            paramIndex: root.paramIndex; defaultValue: root.spec.def; modified: root.modified; hint: root.spec.hint
        }
        Label {
            id: valueLabel
            anchors.right: root.isSeed ? diceButton.left : parent.right
            anchors.rightMargin: root.isSeed ? 8 : 0
            anchors.top: parent.top
            text: root.isSeed ? Math.round(slider.value).toString() : root.formatted(slider.value)
            color: root.modified ? themeManager.accent : themeManager.textPrimary
            font.bold: true
        }
        AppButton {
            id: diceButton
            visible: root.isSeed
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: -3
            width: 74
            height: 22
            text: qsTr("Aleatoria")
            onClicked: appController.setEffectValue(root.paramIndex, Math.floor(Math.random() * (root.spec.max + 1)))
            AppToolTip { visible: parent.hovered; text: qsTr("Prueba otra disposición al azar") }
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
