import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Efectos: one-shot effects. Picking one previews it on the whole photo (calculated off
// the UI thread); the panel's Aplicar bakes it in as an undoable edit, Cancelar drops it.
Item {
    id: root

    property var canvas: null
    readonly property bool canApply: appController.effectId !== "" && !appController.effectBusy
    function apply() { appController.commitEffect(); }
    function cancel() { appController.cancelEffect(); }

    // --- Efectos ------------------------------------------------------------
    // One-shot effects (core/edit/Effects.h). Picking one previews it on the
    // whole photo - calculated off the UI thread - and only "Aplicar" bakes
    // it in as an undoable edit; leaving the tab or "Cancelar" throws it away.
    Flickable {
        id: effectFlick
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        // Whatever is left above the picked effect's controls (effectFooter below).
        anchors.bottom: effectFooter.visible ? effectFooter.top : parent.bottom
        anchors.bottomMargin: effectFooter.visible ? 12 : 0
        contentWidth: width
        contentHeight: effectColumn.implicitHeight + 8
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: AppScrollBar { id: effectBar }
        AppWheelScroll { view: effectFlick }

        // "" = every effect, otherwise a group id from appController.effectGroups
        property string group: ""
        readonly property var shownEffects: appController.effectList.filter(
            function (fx) { return !fx.hidden && (effectFlick.group === "" || fx.group === effectFlick.group); })
        readonly property var picked: appController.effectList.find(
            function (fx) { return fx.id === appController.effectId; })

        // Same previews machinery as the Filtros grid (a small square of the
        // current photo): refreshed when this tab is shown and whenever the
        // photo underneath changes - including an applied effect.
        function refreshPreviews() {
            if (visible)
                appController.ensureLookThumbnails();
        }
        onVisibleChanged: refreshPreviews()
        Component.onCompleted: refreshPreviews()
        Connections {
            target: appController
            function onStructuralImageChanged() { effectFlick.refreshPreviews(); }
            function onEditStackChanged() { effectFlick.refreshPreviews(); }
        }

        Column {
            id: effectColumn
            width: effectFlick.width - effectBar.implicitWidth - 4 // clear of the scroll bar
            spacing: 10

            Label {
                visible: appController.effectId === ""
                width: parent.width
                text: qsTr("Elegí un efecto para ver cómo queda en toda la imagen. Hasta que lo apliques, no cambia nada.")
                color: themeManager.textSecondary
                wrapMode: Text.WordWrap
            }

            Flow {
                width: parent.width
                spacing: 4
                Repeater {
                    model: [{ id: "", name: qsTr("Todos") }].concat(appController.effectGroups)
                    delegate: AppToolButton {
                        required property var modelData
                        text: modelData.name
                        checked: effectFlick.group === modelData.id
                        onClicked: effectFlick.group = modelData.id
                    }
                }
            }

            Grid {
                columns: 3
                spacing: 6
                width: parent.width
                readonly property real cell: (width - spacing * 2) / 3

                LookCell {
                    width: parent.cell
                    kind: "effect"
                    lookId: ""
                    label: qsTr("Ninguno")
                    selected: appController.effectId === ""
                    onPicked: appController.cancelEffect()
                }
                Repeater {
                    model: effectFlick.shownEffects
                    delegate: LookCell {
                        required property var modelData
                        width: parent.cell
                        kind: "effect"
                        lookId: modelData.id
                        label: modelData.name
                        selected: appController.effectId === modelData.id
                        onPicked: appController.selectEffect(modelData.id)
                    }
                }
            }


        }
    }

    // The picked effect's own controls, pinned under the grid so they are
    // always in reach without scrolling: its sliders, how much of it to mix
    // in, and the buttons that keep or discard it.
    // The picked effect's own controls, pinned under the grid. A tall set (the vignette has
    // six sliders) scrolls inside the room it is given instead of squeezing the grid away.
    Item {
        id: effectFooter
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: Math.min(footerColumn.implicitHeight + 11, root.height * 0.58)
        visible: appController.effectId !== ""

        Rectangle { id: footerRule; width: parent.width; height: 1; color: themeManager.border }

        Flickable {
            id: footerFlick
            anchors.top: footerRule.bottom
            anchors.topMargin: 10
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            contentWidth: width
            contentHeight: footerColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: AppScrollBar { id: footerBar }
            AppWheelScroll { view: footerFlick }

            Column {
                id: footerColumn
                width: footerFlick.width - (footerFlick.contentHeight > footerFlick.height ? footerBar.implicitWidth + 4 : 0)
                spacing: 10

            Row {
                width: parent.width
                Label {
                    text: effectFlick.picked ? effectFlick.picked.name : ""
                    color: themeManager.textPrimary
                    font.bold: true
                    width: parent.width - busyLabel.width
                }
                Label {
                    id: busyLabel
                    text: qsTr("Calculando…")
                    color: themeManager.accent
                    opacity: appController.effectBusy ? 1.0 : 0.0
                }
            }

            // Starting points that set several controls at once.
            Column {
                width: parent.width
                spacing: 4
                visible: appController.effectPresets.length > 0
                Label { text: qsTr("Preajustes"); color: themeManager.textSecondary }
                Flow {
                    width: parent.width
                    spacing: 4
                    Repeater {
                        model: appController.effectPresets
                        delegate: AppToolButton {
                            required property int index
                            required property string modelData
                            text: modelData
                            onClicked: appController.applyEffectPreset(index)
                        }
                    }
                }
            }

            Repeater {
                model: appController.effectParams
                delegate: EffectParamRow {
                    required property var modelData
                    required property int index
                    width: footerColumn.width
                    paramIndex: index
                    spec: modelData
                }
            }

            // How much of the effect is mixed into the original (there is nothing to mix a picture of
            // another size with, so effects that change the size have no Mezcla).
            Item {
                width: parent.width
                height: visible ? 42 : 0
                visible: !(effectFlick.picked && effectFlick.picked.changesSize)

                Label {
                    id: mixCaption
                    anchors.left: parent.left
                    anchors.top: parent.top
                    text: qsTr("Mezcla")
                    color: themeManager.textSecondary
                }
                Label {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    text: Math.round(mixSlider.value * 100) + "%"
                    color: mixSlider.value < 0.9995 ? themeManager.accent : themeManager.textPrimary
                    font.bold: true
                }
                AppSlider {
                    id: mixSlider
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: mixCaption.bottom
                    anchors.topMargin: 2
                    from: 0
                    to: 1
                    value: 1
                    snapValue: 1
                    snapThreshold: 0.03
                    onMoved: appController.setEffectMix(value)
                }
                Connections {
                    target: appController
                    function onEffectValuesChanged() {
                        if (Math.abs(mixSlider.value - appController.effectMix) > 0.0005)
                            mixSlider.value = appController.effectMix;
                    }
                }
            }

            AppButton {
                visible: appController.effectParams.length > 0
                text: qsTr("Valores iniciales")
                width: parent.width
                onClicked: appController.resetEffectValues()
                AppToolTip { visible: parent.hovered; text: qsTr("Vuelve a los valores iniciales del efecto") }
            }

            }
        }
    }
}
