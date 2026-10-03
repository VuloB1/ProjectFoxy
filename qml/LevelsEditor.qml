import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Levels: drag the black, midtone and white triangles under the histogram to
// set which brightnesses become pure black / pure white and how the middle is
// bent (moving the middle triangle left brightens the midtones). Works on the
// combined RGB channel or on R, G and B separately - stretching one channel on
// its own is how a color cast gets removed. All values live in the C++
// adjustment state (appController.adjust.levels / outBlack / outWhite).
Column {
    id: root

    property int channel: 0
    readonly property var levels: appController.adjust.levels[channel]
    readonly property real gammaPos: Math.pow(0.5, levels.gamma)

    spacing: 8

    function commit(black, gamma, white) {
        appController.setLevels(root.channel, black, gamma, white);
    }

    ChannelSelector {
        width: root.width
        channel: root.channel
        onPicked: function (c) { root.channel = c; }
    }

    // Histogram with the three input handles hanging below it. Inset by half a
    // handle so a handle sitting at 0 or 1 is not cut off by the panel edge.
    Item {
        id: strip
        width: root.width
        height: 96

        Item {
            id: plot
            x: 7
            width: parent.width - 14
            height: parent.height

            HistogramView {
                id: hist
                width: parent.width
                height: 68
                channels: appController.histogramChannels
                mode: ["luma", "r", "g", "b"][root.channel]
            }
            Rectangle {
                y: hist.height + 3
                width: parent.width
                height: 8
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "#000000" }
                    GradientStop { position: 1.0; color: "#ffffff" }
                }
                border.color: themeManager.border
                border.width: 1
            }

            component Handle: Canvas {
                property color fillColor: "#808080"
                property color edgeColor: "#dddddd"
                width: 14
                height: 12
                y: 82
                onFillColorChanged: requestPaint()
                onEdgeColorChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d");
                    ctx.reset();
                    ctx.beginPath();
                    ctx.moveTo(7, 0.5);
                    ctx.lineTo(13.5, 11.5);
                    ctx.lineTo(0.5, 11.5);
                    ctx.closePath();
                    ctx.fillStyle = fillColor;
                    ctx.fill();
                    ctx.strokeStyle = edgeColor;
                    ctx.lineWidth = 1;
                    ctx.stroke();
                }
                Component.onCompleted: requestPaint()
            }

            Handle {
                x: root.levels.inBlack * plot.width - width / 2
                fillColor: "#000000"
                edgeColor: "#dddddd"
            }
            Handle {
                x: (root.levels.inBlack + (root.levels.inWhite - root.levels.inBlack) * root.gammaPos) * plot.width - width / 2
                fillColor: "#808080"
                edgeColor: "#dddddd"
            }
            Handle {
                x: root.levels.inWhite * plot.width - width / 2
                fillColor: "#ffffff"
                edgeColor: "#333333"
            }

            MouseArea {
                id: drag
                // Dragging a triangle must never turn into scrolling the panel
                // this sits in (the Flickable would steal the drag as soon as
                // the pointer moved a few pixels up or down).
                preventStealing: true
                x: -7
                y: 70
                width: parent.width + 14
                height: parent.height - 70
                // 0 = black, 1 = midtone, 2 = white; -1 = nothing grabbed
                property int active: -1

                function handleX(i) {
                    const lv = root.levels;
                    if (i === 0) return lv.inBlack * plot.width;
                    if (i === 2) return lv.inWhite * plot.width;
                    return (lv.inBlack + (lv.inWhite - lv.inBlack) * root.gammaPos) * plot.width;
                }

                // Mouse x relative to the plot itself (this area sticks out 7px
                // on each side of it).
                function plotX(mouse) { return mouse.x - 7; }

                function grabbed(mouse) {
                    let best = -1;
                    let bestDist = 14;
                    // The midtone is tested first so that when it sits on top
                    // of an end handle it is the one that gets picked.
                    for (const i of [1, 0, 2]) {
                        const d = Math.abs(plotX(mouse) - handleX(i));
                        if (d < bestDist) { best = i; bestDist = d; }
                    }
                    return best;
                }

                onPressed: function (mouse) { active = grabbed(mouse); }
                onPositionChanged: function (mouse) {
                    if (active < 0)
                        return;
                    const lv = root.levels;
                    const t = Math.min(1, Math.max(0, plotX(mouse) / plot.width));
                    if (active === 0) {
                        root.commit(Math.min(t, lv.inWhite - 0.02), lv.gamma, lv.inWhite);
                    } else if (active === 2) {
                        root.commit(lv.inBlack, lv.gamma, Math.max(t, lv.inBlack + 0.02));
                    } else {
                        // The middle handle sits at 0.5^gamma between the two
                        // ends; solve that back for gamma.
                        const span = lv.inWhite - lv.inBlack;
                        const frac = Math.min(0.98, Math.max(0.02, (t - lv.inBlack) / span));
                        const gamma = Math.log(frac) / Math.log(0.5);
                        root.commit(lv.inBlack, Math.min(9.99, Math.max(0.1, gamma)), lv.inWhite);
                    }
                }
                onReleased: active = -1
                onDoubleClicked: function (mouse) {
                    // Double-click a triangle to put just that one back.
                    const lv = root.levels;
                    const best = grabbed(mouse);
                    if (best === 0) root.commit(0, lv.gamma, lv.inWhite);
                    else if (best === 2) root.commit(lv.inBlack, lv.gamma, 1);
                    else if (best === 1) root.commit(lv.inBlack, 1, lv.inWhite);
                }
            }
        }
    }

    // Exact values: click a field to type, or drag sideways over it to scrub.
    Row {
        spacing: 6
        width: root.width

        Column {
            width: (root.width - 12) / 3
            spacing: 2
            Label { text: qsTr("Negro"); color: themeManager.textSecondary; font.pixelSize: 11 }
            ScrubNumberField {
                id: blackField
                width: parent.width
                height: 28
                from: 0; to: 253; decimals: 0; dragStep: 1
                onValueEdited: function (v) { root.commit(v / 255, root.levels.gamma, root.levels.inWhite); }
            }
        }
        Column {
            width: (root.width - 12) / 3
            spacing: 2
            Label { text: qsTr("Gamma"); color: themeManager.textSecondary; font.pixelSize: 11 }
            ScrubNumberField {
                id: gammaField
                width: parent.width
                height: 28
                from: 0.1; to: 9.99; decimals: 2; dragStep: 0.01
                onValueEdited: function (v) { root.commit(root.levels.inBlack, v, root.levels.inWhite); }
            }
        }
        Column {
            width: (root.width - 12) / 3
            spacing: 2
            Label { text: qsTr("Blanco"); color: themeManager.textSecondary; font.pixelSize: 11 }
            ScrubNumberField {
                id: whiteField
                width: parent.width
                height: 28
                from: 2; to: 255; decimals: 0; dragStep: 1
                onValueEdited: function (v) { root.commit(root.levels.inBlack, root.levels.gamma, v / 255); }
            }
        }
    }

    Row {
        spacing: 6
        width: root.width

        Column {
            width: (root.width - 6) / 2
            spacing: 2
            Label { text: qsTr("Salida: negro"); color: themeManager.textSecondary; font.pixelSize: 11 }
            ScrubNumberField {
                id: outBlackField
                width: parent.width
                height: 28
                from: 0; to: 253; decimals: 0; dragStep: 1
                onValueEdited: function (v) { appController.setOutputLevels(v / 255, appController.adjust.outWhite); }
            }
        }
        Column {
            width: (root.width - 6) / 2
            spacing: 2
            Label { text: qsTr("Salida: blanco"); color: themeManager.textSecondary; font.pixelSize: 11 }
            ScrubNumberField {
                id: outWhiteField
                width: parent.width
                height: 28
                from: 2; to: 255; decimals: 0; dragStep: 1
                onValueEdited: function (v) { appController.setOutputLevels(appController.adjust.outBlack, v / 255); }
            }
        }
    }

    AppButton {
        text: qsTr("Restablecer niveles")
        width: root.width
        onClicked: appController.resetLevels()
    }

    // ScrubNumberField assigns its own `value` when the user edits it, which
    // would detach a plain binding - so keep the fields in step with the model
    // (Auto, reset, dragging a triangle) through Binding objects instead.
    Binding { target: blackField; property: "value"; value: root.levels.inBlack * 255; restoreMode: Binding.RestoreNone }
    Binding { target: gammaField; property: "value"; value: root.levels.gamma; restoreMode: Binding.RestoreNone }
    Binding { target: whiteField; property: "value"; value: root.levels.inWhite * 255; restoreMode: Binding.RestoreNone }
    Binding { target: outBlackField; property: "value"; value: appController.adjust.outBlack * 255; restoreMode: Binding.RestoreNone }
    Binding { target: outWhiteField; property: "value"; value: appController.adjust.outWhite * 255; restoreMode: Binding.RestoreNone }
}
