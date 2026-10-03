import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Tamaño: width / height / percent of the picture as it is right now.
Item {
    id: root

    property var canvas: null

    // Modo pixel with "Editar sin suavizar": resizing copies pixels instead of blending them.
    readonly property bool sharp: appSettings.pixelMode && appSettings.pixelSharpEdit

    readonly property bool canApply: Math.round(widthField.value) !== resizeTab.originalWidth
                                     || Math.round(heightField.value) !== resizeTab.originalHeight

    function apply() {
        appController.resizeImage(Math.round(widthField.value), Math.round(heightField.value),
                                  keepAspectCheck.checked, enhanceCheck.checked,
                                  root.sharp ? "nearest" : "lanczos3");
    }
    function cancel() {}

    // --- Tamaño (resize) ------------------------------------------------
    Column {
        id: resizeTab
        width: root.width
        spacing: 10

        // The size the CURRENT (already-baked) image has right now - the
        // baseline that "Escala %" and the aspect ratio are computed
        // against. Refreshed whenever a new image loads, this tab is
        // (re)entered, or an edit is applied here.
        property real originalWidth: 1
        property real originalHeight: 1
        property real aspectRatio: 1
        // Guards against the width/height/percent fields re-triggering
        // each other while one of them is being programmatically synced.
        property bool syncing: false

        function resetFields() {
            const sz = appController.currentImageSize;
            originalWidth = Math.max(1, sz.width);
            originalHeight = Math.max(1, sz.height);
            aspectRatio = originalWidth / originalHeight;
            syncing = true;
            widthField.value = originalWidth;
            heightField.value = originalHeight;
            percentField.value = 100;
            percentSlider.value = 100;
            syncing = false;
        }

        function applyWidth(w) {
            if (syncing) return;
            syncing = true;
            widthField.value = w;
            if (keepAspectCheck.checked)
                heightField.value = Math.max(1, Math.round(w / aspectRatio));
            percentField.value = Math.round(w / originalWidth * 100);
            percentSlider.value = percentField.value;
            syncing = false;
        }

        function applyHeight(h) {
            if (syncing) return;
            syncing = true;
            heightField.value = h;
            if (keepAspectCheck.checked)
                widthField.value = Math.max(1, Math.round(h * aspectRatio));
            percentField.value = Math.round(h / originalHeight * 100);
            percentSlider.value = percentField.value;
            syncing = false;
        }

        function applyPercent(pct) {
            if (syncing) return;
            syncing = true;
            percentField.value = pct;
            percentSlider.value = pct;
            widthField.value = Math.max(1, Math.round(originalWidth * pct / 100));
            heightField.value = Math.max(1, Math.round(originalHeight * pct / 100));
            syncing = false;
        }

        Component.onCompleted: resetFields()

        Row {
            width: parent.width
            Label { text: qsTr("Tamaño original (px)"); color: themeManager.textSecondary; width: 140 }
            Label { text: resizeTab.originalWidth + " x " + resizeTab.originalHeight; color: themeManager.textPrimary }
        }
        Row {
            width: parent.width
            Label { text: qsTr("Tamaño resultante (px)"); color: themeManager.textSecondary; width: 140 }
            Label {
                text: Math.round(widthField.value) + " x " + Math.round(heightField.value)
                color: themeManager.textPrimary
                font.bold: true
            }
        }

        Row {
            width: parent.width
            spacing: 8
            Label { text: qsTr("Ancho"); color: themeManager.textSecondary; anchors.verticalCenter: parent.verticalCenter; width: 42 }
            ScrubNumberField {
                id: widthField
                from: 1; to: 20000; decimals: 0; dragStep: 2
                onValueEdited: resizeTab.applyWidth(newValue)
            }
        }
        Row {
            width: parent.width
            spacing: 8
            Label { text: qsTr("Alto"); color: themeManager.textSecondary; anchors.verticalCenter: parent.verticalCenter; width: 42 }
            ScrubNumberField {
                id: heightField
                from: 1; to: 20000; decimals: 0; dragStep: 2
                onValueEdited: resizeTab.applyHeight(newValue)
            }
        }

        Row {
            width: parent.width
            spacing: 8
            enabled: keepAspectCheck.checked
            opacity: enabled ? 1.0 : 0.5
            Label { text: qsTr("Escala"); color: themeManager.textSecondary; anchors.verticalCenter: parent.verticalCenter; width: 42 }
            AppSlider {
                id: percentSlider
                anchors.verticalCenter: parent.verticalCenter
                // Enlarging much further adds size, not detail: the slider stops at 120 %.
                // Typing in the field still allows up to 250 %. Pixel pictures are enlarged by
                // whole multiples (2x, 4x, 8x...), so that mode lifts both limits.
                from: 1; to: root.sharp ? 800 : 120
                value: 100
                // A detent at 100% (the original size), the way the
                // signed sliders stop at 0.
                snapValue: 100
                snapThreshold: 6
                width: parent.width - 42 - 76 - 16
                onMoved: resizeTab.applyPercent(Math.round(value))
            }
            ScrubNumberField {
                id: percentField
                implicitWidth: 76
                from: 1; to: root.sharp ? 3200 : 250; decimals: 0; dragStep: 1; suffix: "%"
                onValueEdited: resizeTab.applyPercent(newValue)
            }
        }

        AppCheckBox {
            id: keepAspectCheck
            text: qsTr("Mantener proporción")
            checked: true
            // Re-enabling the link snaps height back to the original
            // ratio (based on the current width) instead of waiting for
            // the next edit, so the fields agree with each other right
            // away.
            onToggled: if (checked) resizeTab.applyWidth(widthField.value)
        }

        Label {
            visible: root.sharp
            width: parent.width
            text: qsTr("Modo pixel: se redimensiona sin suavizar (vecino más cercano), así los píxeles no se mezclan.")
            color: themeManager.textSecondary
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        // Only meaningful when the picture is getting bigger: resamples
        // with Lanczos and then restores edge crispness (see
        // core/edit/Resample.h) instead of just stretching the pixels.
        AppCheckBox {
            id: enhanceCheck
            text: qsTr("Mejorar nitidez al ampliar")
            checked: true
            visible: !root.sharp
                     && (widthField.value > resizeTab.originalWidth || heightField.value > resizeTab.originalHeight)
        }
    }
}
