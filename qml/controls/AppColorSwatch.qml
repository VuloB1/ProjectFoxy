import QtQuick
import QtQuick.Controls
import ImageViewerApp

// A colour well: a swatch that opens a small picker (a palette, hue / saturation / brightness
// sliders and a hex field). `value` is the colour shown; `picked` fires with every change the
// user makes, so what depends on it can update live while a slider is dragged.
Item {
    id: root

    property color value: "#ffffff"
    signal picked(color chosen)

    implicitWidth: 64
    implicitHeight: 28

    function hex(c) {
        function two(x) { const s = Math.round(x * 255).toString(16); return s.length < 2 ? "0" + s : s; }
        return "#" + two(c.r) + two(c.g) + two(c.b);
    }

    Rectangle {
        id: well
        anchors.fill: parent
        radius: themeManager.radiusSmall
        color: root.value
        border.width: 1
        border.color: wellHover.hovered || popup.visible ? themeManager.accent : themeManager.border
        HoverHandler { id: wellHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: popup.visible ? popup.close() : popup.open() }
    }

    Popup {
        id: popup
        y: root.height + 4
        x: Math.min(0, root.parent ? root.parent.width - root.x - width : 0)
        width: 236
        padding: 10
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        // The colour is being edited in HSV so that dragging brightness does not lose the hue of a dark colour.
        property real h: 0
        property real s: 0
        property real v: 1
        property bool syncing: false

        function load(c) {
            syncing = true;
            // Qt reports -1 as the hue of greys: keep the last hue then
            if (c.hsvHue >= 0)
                h = c.hsvHue;
            s = c.hsvSaturation;
            v = c.hsvValue;
            hueSlider.value = h;
            satSlider.value = s;
            valSlider.value = v;
            hexField.text = root.hex(c);
            syncing = false;
        }
        function push() {
            if (syncing)
                return;
            const c = Qt.hsva(h, s, v, 1);
            hexField.text = root.hex(c);
            root.picked(c);
        }
        onAboutToShow: load(root.value)

        background: Rectangle {
            radius: themeManager.radiusMedium
            color: themeManager.surfaceElevated
            border.color: themeManager.border
            border.width: 1
        }

        contentItem: Column {
            spacing: 8

            Grid {
                id: palette
                columns: 8
                spacing: 4
                readonly property var colors: [
                    "#ffffff", "#d9d9d9", "#a6a6a6", "#737373", "#404040", "#000000", "#7a4a2b", "#c89f7a",
                    "#ff3b30", "#ff9500", "#ffcc00", "#a4de02", "#34c759", "#00c7be", "#32ade6", "#007aff",
                    "#5856d6", "#af52de", "#ff2d92", "#ff6b81", "#8b0000", "#006400", "#00008b", "#4b0082",
                    "#ffd1dc", "#ffe4b5", "#fff8b0", "#d4f5c9", "#c4f0ee", "#c7e0ff", "#e0d4ff", "#f5d0ee"
                ]
                Repeater {
                    model: palette.colors
                    delegate: Rectangle {
                        required property string modelData
                        width: 22
                        height: 22
                        radius: 4
                        color: modelData
                        border.width: 1
                        border.color: themeManager.border
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            onTapped: {
                                popup.load(Qt.color(modelData));
                                root.picked(Qt.color(modelData));
                            }
                        }
                    }
                }
            }

            Row {
                width: parent.width
                spacing: 8
                Label { text: qsTr("Tono"); color: themeManager.textSecondary; width: 64; anchors.verticalCenter: parent.verticalCenter }
                AppSlider {
                    id: hueSlider
                    width: parent.width - 72
                    from: 0; to: 1
                    onMoved: { popup.h = value; popup.push(); }
                }
            }
            Row {
                width: parent.width
                spacing: 8
                Label { text: qsTr("Saturación"); color: themeManager.textSecondary; width: 64; anchors.verticalCenter: parent.verticalCenter }
                AppSlider {
                    id: satSlider
                    width: parent.width - 72
                    from: 0; to: 1
                    onMoved: { popup.s = value; popup.push(); }
                }
            }
            Row {
                width: parent.width
                spacing: 8
                Label { text: qsTr("Brillo"); color: themeManager.textSecondary; width: 64; anchors.verticalCenter: parent.verticalCenter }
                AppSlider {
                    id: valSlider
                    width: parent.width - 72
                    from: 0; to: 1
                    onMoved: { popup.v = value; popup.push(); }
                }
            }

            Row {
                width: parent.width
                spacing: 8
                Rectangle {
                    width: 28
                    height: 28
                    radius: 4
                    color: Qt.hsva(popup.h, popup.s, popup.v, 1)
                    border.width: 1
                    border.color: themeManager.border
                }
                AppTextField {
                    id: hexField
                    width: parent.width - 36
                    height: 28
                    font.family: "Consolas"
                    maximumLength: 7
                    inputMethodHints: Qt.ImhNoPredictiveText
                    onEditingFinished: {
                        const c = Qt.color(text);
                        // Qt.color() gives black for text it cannot read: only accept a real #rrggbb
                        if (/^#[0-9a-fA-F]{6}$/.test(text)) {
                            popup.load(c);
                            root.picked(c);
                        } else {
                            text = root.hex(Qt.hsva(popup.h, popup.s, popup.v, 1));
                        }
                    }
                }
            }
        }
    }
}
