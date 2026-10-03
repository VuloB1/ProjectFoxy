import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Ajustes: every slider writes the live overlay the GPU draws; the panel's Aplicar keeps
// what was done here and Cancelar undoes it (the history decides, see
// AppController::endToolSession).
Item {
    id: root

    property var canvas: null
    readonly property bool canApply: appController.toolSessionDirty
    function apply() {}
    function cancel() {}

    // --- Ajustes ---------------------------------------------------------
    // A long list, so it scrolls inside whatever room is left under the
    // tab bar. Every control reads and writes appController's adjustment
    // state, which the GPU shaders in ImageCanvas.qml render live.
    Flickable {
        id: adjustFlick
        anchors.fill: parent
        contentWidth: width
        contentHeight: adjustColumn.implicitHeight + 8
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: AppScrollBar { id: adjustBar }
        AppWheelScroll { view: adjustFlick }

        Column {
            id: adjustColumn
            width: adjustFlick.width - adjustBar.implicitWidth - 4 // clear of the scroll bar
            spacing: 10

            // What the picture looks like once every adjustment below is
            // applied. Click to switch between the three color channels
            // and plain brightness.
            Column {
                width: parent.width
                spacing: 4

                Row {
                    width: parent.width
                    Label {
                        text: qsTr("Histograma")
                        color: themeManager.textSecondary
                        width: parent.width - modeLabel.width
                    }
                    Label {
                        id: modeLabel
                        text: histView.mode === "rgb" ? qsTr("RGB") : qsTr("Luminosidad")
                        color: themeManager.textPrimary
                        font.bold: true
                    }
                }
                HistogramView {
                    id: histView
                    width: parent.width
                    height: 64
                    channels: appController.adjustedHistogram

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: histView.mode = histView.mode === "rgb" ? "luma" : "rgb"
                    }
                }
            }

            Column {
                width: parent.width
                spacing: 4

                Label { text: qsTr("Automático"); color: themeManager.textSecondary }
                Row {
                    spacing: 6
                    width: parent.width
                    readonly property real cell: (width - spacing * 2) / 3
                    AppButton {
                        text: qsTr("Color")
                        width: parent.cell
                        onClicked: appController.autoAdjust("color")
                        AppToolTip { visible: parent.hovered; text: qsTr("Neutraliza una dominante de color") }
                    }
                    AppButton {
                        text: qsTr("Niveles")
                        width: parent.cell
                        onClicked: appController.autoAdjust("levels")
                        AppToolTip { visible: parent.hovered; text: qsTr("Estira cada canal de color por separado") }
                    }
                    AppButton {
                        text: qsTr("Contraste")
                        width: parent.cell
                        onClicked: appController.autoAdjust("contrast")
                        AppToolTip { visible: parent.hovered; text: qsTr("Estira el brillo al rango completo") }
                    }
                }
            }

            AdjustSection {
                width: parent.width
                title: qsTr("Luz")
                AdjustRow { width: parent.width; param: "exposure"; label: qsTr("Exposición") }
                AdjustRow { width: parent.width; param: "brightness"; label: qsTr("Brillo") }
                AdjustRow { width: parent.width; param: "contrast"; label: qsTr("Contraste") }
                AdjustRow { width: parent.width; param: "highlights"; label: qsTr("Luces") }
                AdjustRow { width: parent.width; param: "shadows"; label: qsTr("Sombras") }
                AdjustRow { width: parent.width; param: "whites"; label: qsTr("Blancos") }
                AdjustRow { width: parent.width; param: "blacks"; label: qsTr("Negros") }
                AdjustRow { width: parent.width; param: "gamma"; label: qsTr("Gamma") }
            }

            AdjustSection {
                width: parent.width
                title: qsTr("Color")
                // Temperature as the colour temperature of the light being added, in kelvin: low (orange)
                // warms the picture, high (blue) cools it, 6500 K (daylight) leaves it alone. The groove is
                // painted with those colours, so the slider shows what it will do.
                AdjustRow {
                    width: parent.width
                    param: "temperature"
                    label: qsTr("Temperatura")
                    inverted: true
                    trackColors: ["#ff8a1f", "#dcdcdc", "#3f8cff"]
                    formatter: function (v) {
                        const k = v >= 0 ? 6500 - v * 4500 : 6500 - v * 5500;
                        return Math.round(k / 10) * 10 + " K";
                    }
                }
                // Tint: magenta on the left, green on the right.
                AdjustRow { width: parent.width; param: "tint"; label: qsTr("Matiz"); trackColors: ["#e040c0", "#dcdcdc", "#3fc850"] }
                AdjustRow { width: parent.width; param: "saturation"; label: qsTr("Saturación") }
                AdjustRow { width: parent.width; param: "vibrance"; label: qsTr("Intensidad") }
                AdjustRow { width: parent.width; param: "hue"; label: qsTr("Tono"); displayScale: 180; displaySuffix: "°" }
            }

            AdjustSection {
                width: parent.width
                title: qsTr("Detalle")
                AdjustRow { width: parent.width; param: "clarity"; label: qsTr("Claridad") }
                AdjustRow { width: parent.width; param: "sharpness"; label: qsTr("Nitidez"); from: 0 }
            }

            AdjustSection {
                width: parent.width
                title: qsTr("Niveles")
                expanded: false
                LevelsEditor { width: parent.width }
            }

            AdjustSection {
                width: parent.width
                title: qsTr("Curvas")
                expanded: false
                CurvesEditor { width: parent.width }
            }

            AppButton {
                text: qsTr("Restablecer ajustes")
                width: parent.width
                onClicked: appController.resetAdjust()
            }
        }
    }

}
