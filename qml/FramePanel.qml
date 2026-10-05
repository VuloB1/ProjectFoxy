import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Marco: the page of the Recortar tool that cuts the picture to a shape (a rectangle with round, soft, cut
// or hollow corners, an ellipse, a hexagon, a heart...) and puts an outline, a margin, a shadow and a
// background around it. It drives the hidden "frame" effect (core/edit/EffectsFrame.cpp) - this page only
// shows its controls - so the result previews on the picture as it is set, and Aplicar bakes it in.
Item {
    id: root

    // The order of the frame effect's parameters (the same as in EffectsFrame.cpp).
    readonly property int pShape: 0
    readonly property int pStyle: 1
    readonly property int pRadius: 2
    readonly property int pCorners: 3

    readonly property bool ready: appController.effectId === "frame" && appController.effectParams.length > 0
    readonly property int shape: ready ? Math.round(appController.effectValues[pShape]) : 0
    // The shapes whose corners can be treated (all but the smooth ones, ellipse and heart).
    readonly property bool cornered: shape !== 1 && shape !== 7

    readonly property var shapes: [
        { icon: "frame-rect", name: qsTr("Rectángulo") },
        { icon: "frame-ellipse", name: qsTr("Elipse o círculo") },
        { icon: "frame-hexagon", name: qsTr("Hexágono") },
        { icon: "frame-octagon", name: qsTr("Octágono") },
        { icon: "frame-diamond", name: qsTr("Rombo") },
        { icon: "frame-triangle", name: qsTr("Triángulo") },
        { icon: "frame-star", name: qsTr("Estrella") },
        { icon: "frame-heart", name: qsTr("Corazón") }
    ]
    readonly property var styles: [
        { icon: "corner-round", name: qsTr("Esquinas redondas") },
        { icon: "corner-soft", name: qsTr("Esquinas suaves (curva más amplia)") },
        { icon: "corner-cut", name: qsTr("Esquinas cortadas en recto") },
        { icon: "corner-hollow", name: qsTr("Esquinas cóncavas, como una entrada") }
    ]

    // A run of the effect's own controls, one row each.
    component ParamRows: Column {
        property var indices: []
        property bool active: false
        spacing: 10
        Repeater {
            model: parent.active ? parent.indices : []
            delegate: EffectParamRow {
                required property int modelData
                width: parent.width
                paramIndex: modelData
                spec: appController.effectParams[modelData]
            }
        }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight + 8
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: AppScrollBar { id: bar }
        AppWheelScroll { view: flick }

        Column {
            id: column
            width: flick.width - bar.implicitWidth - 4
            spacing: 10

            Label {
                width: parent.width
                text: qsTr("Cortá la foto con la forma que quieras y rodeala de contorno, sombra y fondo. La imagen crece para que todo entre; si querés mantener el tamaño, activalo abajo.")
                color: themeManager.textSecondary
                wrapMode: Text.WordWrap
                font.pixelSize: 11
            }

            // Starting points.
            Flow {
                width: parent.width
                spacing: 4
                visible: root.ready && appController.effectPresets.length > 0
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

            Label { text: qsTr("Forma"); color: themeManager.textSecondary }
            Flow {
                width: parent.width
                spacing: 6
                readonly property real cell: (width - spacing * 3) / 4
                Repeater {
                    model: root.shapes
                    delegate: AppButton {
                        required property int index
                        required property var modelData
                        text: modelData.name
                        iconName: modelData.icon
                        iconOnly: true
                        width: parent.cell
                        checked: root.shape === index
                        onClicked: appController.setEffectValue(root.pShape, index)
                    }
                }
            }
            Label {
                width: parent.width
                visible: root.shape !== 0
                text: qsTr("La forma llena toda la foto: si querés un círculo o un hexágono parejo, recortala antes en 1:1.")
                color: themeManager.textSecondary
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }

            // The corners of the shape.
            Column {
                width: parent.width
                spacing: 10
                visible: root.cornered

                Label { text: qsTr("Corte de las esquinas"); color: themeManager.textSecondary }
                Row {
                    spacing: 6
                    width: parent.width
                    readonly property real cell: (width - spacing * 3) / 4
                    Repeater {
                        model: root.styles
                        delegate: AppButton {
                            required property int index
                            required property var modelData
                            text: modelData.name
                            iconName: modelData.icon
                            iconOnly: true
                            width: parent.cell
                            checked: root.ready && Math.round(appController.effectValues[root.pStyle]) === index
                            onClicked: appController.setEffectValue(root.pStyle, index)
                        }
                    }
                }
                ParamRows { width: parent.width; active: root.ready; indices: [root.pRadius] }

                Row {
                    spacing: 12
                    visible: root.shape === 0
                    FrameCornerPicker {
                        anchors.verticalCenter: parent.verticalCenter
                        mask: root.ready ? Math.round(appController.effectValues[root.pCorners]) : 15
                        onChanged: function (m) { appController.setEffectValue(root.pCorners, m); }
                    }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        width: column.width - 78 - 12
                        text: qsTr("Tocá cada esquina para redondearla o dejarla recta.")
                        color: themeManager.textSecondary
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }
            }

            AdjustSection {
                width: parent.width
                title: qsTr("Contorno")
                ParamRows { width: parent.width; active: root.ready; indices: [4, 5, 6, 7] }
            }
            AdjustSection {
                width: parent.width
                title: qsTr("Margen")
                ParamRows { width: parent.width; active: root.ready; indices: [8, 9] }
            }
            AdjustSection {
                width: parent.width
                title: qsTr("Sombra")
                ParamRows { width: parent.width; active: root.ready; indices: [10, 11, 12, 13, 14] }
            }
            AdjustSection {
                width: parent.width
                title: qsTr("Fondo")
                ParamRows { width: parent.width; active: root.ready; indices: [15, 16, 17, 18, 19, 20] }
            }

            AppButton {
                width: parent.width
                text: qsTr("Valores iniciales")
                onClicked: appController.resetEffectValues()
            }
        }
    }
}
