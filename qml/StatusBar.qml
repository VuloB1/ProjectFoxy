import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import ImageViewerApp

// The bottom bar: on the left what is under the mouse (pixel position and colour), on the right the size and zoom
// of the picture, and in the middle a raised, rounded dock with every browse / edit action (icons only; the
// name shows as a tooltip). Holds what used to be the floating pill over the canvas.
Item {
    id: root

    // Set by Main.qml, same property-passing convention Toolbar.qml already
    // uses for `canvas`/`editPanel`.
    property var canvas: null
    property var toolbar: null
    // Edit mode is owned by Main.qml (the edit panel only exists while it is on).
    property bool editMode: false
    // A tool is open in the edit panel: only the actions that belong to editing the picture
    // (Comparar, Deshacer, Rehacer) stay; leaving, saving and resetting wait for Aplicar / Cancelar.
    property bool locked: false
    signal saveAsRequested()
    signal closeEditRequested()

    // "Guardar" was pressed: Main.qml decides whether to ask first (overwriting the
    // original) or to fall back to "Guardar como…".
    signal saveRequested()

    // While editing, this bar sits right over where the crop area (and its
    // resize handles) live - the browse/view buttons below are useless there,
    // so this row swaps to edit actions instead.

    // (The crop's own Cancelar / Aplicar live in the edit panel's tool page now.)

    readonly property bool round: themeManager.radiusMedium !== 0
    // The strip is thin; the dock rises above it.
    implicitHeight: 30
    // The dock's buttons are shown unless the window is taken over by the multi view or a studio.
    property bool dockVisible: true
    // Controls that replace the usual buttons in the dock (the comparer's), as a Component; null = the usual ones.
    property Component customDock: null
    // How far the dock stands above the strip (Main.qml keeps the picture out of that band).
    readonly property int rise: 22

    Rectangle {
        id: strip
        anchors.fill: parent
        color: themeManager.surface
        Rectangle { width: parent.width; height: 1; color: themeManager.border }
        // solid to the mouse, so a click on it does not reach the picture
        MouseArea { anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.AllButtons }
    }

    // ---- left: the pixel under the mouse
    readonly property var px: root.canvas && root.canvas.pixelHovering ? root.canvas.pixelInfo : ({ valid: false })
    Row {
        visible: root.px.valid === true
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 14; height: 14; radius: 3
            border.color: themeManager.border
            color: root.px.valid === true ? Qt.rgba(root.px.r / 255, root.px.g / 255, root.px.b / 255, root.px.a / 255) : "transparent"
        }
        Label {
            anchors.verticalCenter: parent.verticalCenter
            color: themeManager.textPrimary
            font.pixelSize: 12
            text: root.px.valid === true
                  ? qsTr("X %1  Y %2").arg(root.canvas.pixelCell.x).arg(root.canvas.pixelCell.y)
                    + "   " + colorPicker.hex(root.px.r, root.px.g, root.px.b, root.px.a).toUpperCase()
                    + "   R " + root.px.r + "  G " + root.px.g + "  B " + root.px.b
                    + (root.px.a < 255 ? "  A " + root.px.a : "")
                  : ""
        }
    }

    // ---- right: size and zoom, then the information button in the corner
    AppToolButton {
        id: infoButton
        objectName: "infoButton"
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        text: qsTr("Información")
        iconName: "info"
        iconOnly: true
        iconSize: 18
        iconStroke: 1.4
        topPadding: 2
        bottomPadding: 2
        onClicked: infoPopup.open()
    }
    Label {
        anchors.right: infoButton.left
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        color: themeManager.textSecondary
        font.pixelSize: 12
        visible: appController.currentImageSize.width > 0 && root.canvas !== null
        text: root.canvas ? appController.currentImageSize.width + " × " + appController.currentImageSize.height
                            + "   " + root.canvas.zoomPercent + "%" : ""
    }

    // ---- middle: the dock. One outline: rounded top corners, and where it meets the strip the line curves outward
    // (a concave fillet), so the two read as a single piece. `fillet` is the extra room each side that curve takes.
    Canvas {
        id: dock
        visible: root.dockVisible
        z: 1
        readonly property real fillet: root.round ? 8 : 0
        readonly property real corner: root.round ? 10 : 0
        readonly property real pad: 9 // inside the body, left of the first button / right of the last
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: (root.customDock !== null ? customRow.implicitWidth : row.implicitWidth) + 2 * pad + 2 * fillet
        height: root.implicitHeight + root.rise
        canvasSize: Qt.size(width, height) // otherwise the drawing is stretched when the size changes (theme, edit mode...)
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset(); // a fresh state each time: the translate below would otherwise pile up
            const f = fillet, r = corner, w = width, h = height, base = root.rise; // `base`: where the strip's own line is
            function outline() {
                ctx.moveTo(0, base);
                if (f > 0) ctx.arc(0, base - f, f, Math.PI / 2, 0, true); else ctx.lineTo(f, base);
                ctx.lineTo(f, r);
                if (r > 0) ctx.arc(f + r, r, r, Math.PI, 1.5 * Math.PI); else ctx.lineTo(f, 0);
                ctx.lineTo(w - f - r, 0);
                if (r > 0) ctx.arc(w - f - r, r, r, 1.5 * Math.PI, 2 * Math.PI); else ctx.lineTo(w - f, 0);
                ctx.lineTo(w - f, base - f);
                if (f > 0) ctx.arc(w, base - f, f, Math.PI, Math.PI / 2, true); else ctx.lineTo(w, base);
            }
            ctx.fillStyle = themeManager.surface;
            ctx.beginPath();
            outline();
            ctx.lineTo(w, h); ctx.lineTo(0, h);
            ctx.closePath();
            ctx.fill();
            ctx.strokeStyle = themeManager.border;
            ctx.lineWidth = 1;
            ctx.translate(0, 0.5);
            ctx.beginPath();
            outline();
            ctx.stroke();
        }
        Connections {
            target: themeManager
            function onThemeChanged() { dock.requestPaint(); }
        }
        // only the body takes the mouse, not the curve's corners
        MouseArea { x: dock.fillet; width: parent.width - 2 * dock.fillet; height: parent.height; hoverEnabled: true; acceptedButtons: Qt.AllButtons }
    }

    Loader {
        id: customRow
        active: root.customDock !== null
        sourceComponent: root.customDock
        visible: root.dockVisible
        z: 2
        anchors.left: dock.left
        anchors.leftMargin: dock.fillet + dock.pad
        anchors.verticalCenter: dock.verticalCenter
    }

    Row {
        id: row
        visible: root.dockVisible && root.customDock === null
        z: 2
        anchors.left: dock.left
        anchors.leftMargin: dock.fillet + dock.pad
        anchors.verticalCenter: dock.verticalCenter
        spacing: 4

        AppToolButton {
            hPad: 10
            iconOnly: true
            iconSize: 22
            text: qsTr("Abrir")
            iconName: "open"
            visible: !root.editMode
            onClicked: if (root.toolbar) root.toolbar.requestOpen()
        }
        AppToolButton {
            hPad: 10
            iconOnly: true
            iconSize: 22
            text: qsTr("Anterior")
            objectName: "previousButton"
            iconName: "prev"
            visible: !root.editMode
            enabled: folderModel.count > 1
            onClicked: folderModel.previous()
        }
        AppToolButton {
            hPad: 10
            iconOnly: true
            iconSize: 22
            text: qsTr("Siguiente")
            objectName: "nextButton"
            iconName: "next"
            visible: !root.editMode
            enabled: folderModel.count > 1
            onClicked: folderModel.next()
        }
        AppToolButton {
            hPad: 10
            iconOnly: true
            iconSize: 22
            text: qsTr("Zoom")
            iconName: "zoom-out"
            objectName: "zoomOutButton"
            visible: !root.editMode
            onClicked: if (root.canvas) root.canvas.zoomOut()
        }
        AppToolButton {
            hPad: 10
            iconOnly: true
            iconSize: 22
            text: qsTr("Ajustar")
            objectName: "fitButton"
            iconName: "fit"
            visible: !root.editMode
            onClicked: if (root.canvas) root.canvas.fitToWindow()
        }
        // --- Edit-mode row: same slot, swapped to the actions that are
        // actually usable while the edit panel is open. Comparar/Deshacer/
        // Rehacer apply regardless of which tab is active, so they're always
        // shown while editing; the rest of the row is contextual.
        AppToolButton {
            hPad: 10
            iconOnly: true
            iconSize: 22
            text: qsTr("Zoom")
            iconName: "zoom-in"
            objectName: "zoomInButton"
            visible: !root.editMode
            onClicked: if (root.canvas) root.canvas.zoomIn()
        }
        AppToolButton {
            hPad: 10
            iconOnly: true
            iconSize: 22
            text: qsTr("Girar izq.")
            iconName: "rotate-left"
            visible: !root.editMode
            onClicked: if (root.canvas) root.canvas.rotateCounterClockwise()
        }
        AppToolButton {
            hPad: 10
            iconOnly: true
            iconSize: 22
            text: qsTr("Girar der.")
            iconName: "rotate-right"
            visible: !root.editMode
            onClicked: if (root.canvas) root.canvas.rotateClockwise()
        }
        AppToolButton {
            hPad: 10
            iconOnly: true
            iconSize: 22
            text: qsTr("Comparar")
            iconName: "compare"
            visible: root.editMode
            enabled: appController.hasEdits
            // Held down (not toggled) - shows the as-opened original for as
            // long as the mouse is pressed, like most photo editors.
            onPressedChanged: if (root.canvas) root.canvas.compareOriginal = pressed
        }
        AppToolButton {
            hPad: 10
            iconOnly: true
            iconSize: 22
            text: qsTr("Deshacer")
            objectName: "undoButton"
            iconName: "undo"
            visible: root.editMode
            enabled: appController.canUndoEdit
            onClicked: appController.undoEdit()
        }
        AppToolButton {
            hPad: 10
            iconOnly: true
            iconSize: 22
            text: qsTr("Rehacer")
            iconName: "redo"
            visible: root.editMode
            enabled: appController.canRedoEdit
            onClicked: appController.redoEdit()
        }

        AppToolButton {
            hPad: 10
            iconOnly: true
            iconSize: 22
            text: qsTr("Restablecer")
            iconName: "reset"
            visible: root.editMode && !root.locked
            enabled: appController.hasEdits
            onClicked: appController.resetEdits()
        }
        AppToolButton {
            hPad: 10
            iconOnly: true
            iconSize: 22
            text: qsTr("Guardar")
            objectName: "saveButton"
            iconName: "save"
            visible: root.editMode && !root.locked
            // Also when the screen differs from the file without any active edit
            // (Guardar, then Deshacer): that is something to save too.
            enabled: (appController.hasEdits || appController.isDirty) && !appController.isLoading
                     && !appController.isSaving
            onClicked: root.saveRequested()
        }
        AppToolButton {
            hPad: 10
            iconOnly: true
            iconSize: 22
            text: qsTr("Guardar como…")
            iconName: "save-as"
            visible: root.editMode && !root.locked
            enabled: !appController.isSaving
            onClicked: root.saveAsRequested()
        }
        AppToolButton {
            hPad: 10
            iconOnly: true
            iconSize: 22
            text: qsTr("Cerrar edición")
            iconName: "close"
            visible: root.editMode && !root.locked
            onClicked: root.closeEditRequested()
        }
    }

    Popup {
        id: infoPopup
        x: root.width - width - 12
        y: -height - 6
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: AppPanelBackground { color: themeManager.surfaceElevated; radius: themeManager.radiusMedium }

        contentItem: Column {
            objectName: "infoPopupContent"
            spacing: 6
            Label {
                color: themeManager.textPrimary
                font.bold: true
                text: folderModel.count > 0
                      ? qsTr("Imagen %1 de %2").arg(folderModel.currentIndex + 1).arg(folderModel.count)
                      : qsTr("Sin imagen")
            }
            Label {
                color: themeManager.textSecondary
                text: qsTr("Dimensiones: %1 x %2")
                      .arg(appController.currentImageSize.width)
                      .arg(appController.currentImageSize.height)
            }
            Label {
                color: themeManager.textSecondary
                visible: root.canvas !== null
                text: root.canvas ? qsTr("Zoom: %1%").arg(root.canvas.zoomPercent) : ""
            }

            Rectangle {
                width: parent.width
                height: 1
                color: themeManager.border
                visible: Object.keys(appController.metadata).length > 0
            }

            Repeater {
                model: Object.keys(appController.metadata)
                delegate: Label {
                    color: themeManager.textSecondary
                    text: modelData + ": " + appController.metadata[modelData]
                }
            }

            Label {
                color: themeManager.textSecondary
                visible: Object.keys(appController.metadata).length === 0
                text: qsTr("Sin datos EXIF")
            }

            Rectangle { width: parent.width; height: 1; color: themeManager.border }
            Label {
                color: themeManager.textSecondary
                font.pixelSize: 11
                text: qsTr("Project Foxy %1").arg(Qt.application.version)
            }

            Rectangle {
                width: parent.width
                height: 1
                color: themeManager.border
                visible: appController.histogram.length > 0
            }

            Label {
                color: themeManager.textSecondary
                visible: appController.histogram.length > 0
                text: qsTr("Histograma")
            }

            Canvas {
                id: histogramCanvas
                width: 256
                height: 70
                visible: appController.histogram.length > 0
                onPaint: {
                    const ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    ctx.fillStyle = themeManager.surface;
                    ctx.fillRect(0, 0, width, height);
                    const data = appController.histogram;
                    if (data.length === 0)
                        return;
                    ctx.fillStyle = themeManager.textSecondary;
                    for (let i = 0; i < data.length; i++) {
                        const barHeight = data[i] * height;
                        ctx.fillRect(i, height - barHeight, 1, barHeight);
                    }
                }

                Connections {
                    target: appController
                    function onHistogramChanged() { histogramCanvas.requestPaint(); }
                }
                Connections {
                    target: themeManager
                    function onThemeChanged() { histogramCanvas.requestPaint(); }
                }
                Component.onCompleted: requestPaint()
            }
        }
    }
}
