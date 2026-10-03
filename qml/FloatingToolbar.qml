import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import ImageViewerApp

// Floating pill-shaped navigation bar, centered over the bottom of the
// canvas (Telegram-style bottom bar, per the user's reference screenshot).
// Holds every "browse/view" action; Toolbar.qml (now repurposed as a slim
// status bar) keeps Editar/Info/Más/Tema/Animación instead - see the plan
// this was built from for the exact split.
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
    // True while the slideshow is playing (the thumbnail bar closes itself in "auto" mode).
    readonly property bool slideshowRunning: slideshowButton.checked
    signal saveAsRequested()
    signal closeEditRequested()

    // "Guardar" was pressed: Main.qml decides whether to ask first (overwriting the
    // original) or to fall back to "Guardar como…".
    signal saveRequested()

    // While editing, this bar sits right over where the crop area (and its
    // resize handles) live - the browse/view buttons below are useless there,
    // so this row swaps to edit actions instead.

    // (The crop's own Cancelar / Aplicar live in the edit panel's tool page now.)

    // The modern themes' end buttons are drawn right up to the capsule's ends, so they
    // only need a thin margin; the flat theme keeps its wider one.
    implicitWidth: row.implicitWidth + (themeManager.radiusMedium === 0 ? 28 : 8)
    implicitHeight: 60

    Rectangle {
        id: pill
        anchors.fill: parent
        // Only the fully-rounded skins (Modern) get the Telegram-style
        // capsule; a flat theme (radiusMedium: 0, e.g. Win98, where nothing
        // else in the UI is rounded either) gets a plain square bar instead,
        // so this floating piece doesn't stick out as the one round shape in
        // an otherwise flat theme.
        radius: themeManager.radiusMedium === 0 ? 0 : height / 2
        color: themeManager.surfaceElevated
        border.color: themeManager.border
        border.width: 1

        // The bar is solid to the mouse, spaces between its buttons included: otherwise a click
        // there fell through to the picture underneath.
        MouseArea { anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.AllButtons }
    }

    MultiEffect {
        source: pill
        anchors.fill: pill
        shadowEnabled: true
        shadowColor: "#40000000"
        shadowBlur: 0.6
        shadowVerticalOffset: 3
        shadowHorizontalOffset: 0
    }

    // The first and last visible buttons get the capsule's curvature on their outer
    // side, so their hover/press shape runs concentric with the bar's border.
    function updateEnds() {
        // Work out each button's place first, then touch only the ones that changed: an edge
        // button is wider (extra outer padding), so assigning the same values again would
        // re-lay the row out forever.
        let first = null, last = null;
        const buttons = [];
        for (let i = 0; i < row.children.length; ++i) {
            const c = row.children[i];
            if (!("edge" in c))
                continue;
            buttons.push(c);
            if (!c.visible)
                continue;
            if (!first)
                first = c;
            last = c;
        }
        const loneButton = first === last; // a lone button keeps its own shape
        for (const c of buttons) {
            const want = loneButton || !c.visible ? 0 : c === first ? -1 : c === last ? 1 : 0;
            if (c.edge !== want)
                c.edge = want;
        }
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 2
        onPositioningComplete: root.updateEnds()
        Component.onCompleted: root.updateEnds()

        AppToolButton {
            hPad: 12
            text: qsTr("Abrir")
            iconName: "open"
            visible: !root.editMode
            onClicked: if (root.toolbar) root.toolbar.requestOpen()
        }
        AppToolButton {
            hPad: 12
            text: qsTr("Anterior")
            iconName: "prev"
            visible: !root.editMode
            enabled: folderModel.count > 1
            onClicked: folderModel.previous()
        }
        AppToolButton {
            hPad: 12
            text: qsTr("Siguiente")
            iconName: "next"
            visible: !root.editMode
            enabled: folderModel.count > 1
            onClicked: folderModel.next()
        }
        AppToolButton {
            hPad: 12
            text: qsTr("Zoom")
            iconName: "zoom-out"
            visible: !root.editMode
            onClicked: if (root.canvas) root.canvas.zoomOut()
        }
        AppToolButton {
            hPad: 12
            text: qsTr("Zoom")
            iconName: "zoom-in"
            visible: !root.editMode
            onClicked: if (root.canvas) root.canvas.zoomIn()
        }
        AppToolButton {
            hPad: 12
            text: qsTr("Girar izq.")
            iconName: "rotate-left"
            visible: !root.editMode
            onClicked: if (root.canvas) root.canvas.rotateCounterClockwise()
        }
        AppToolButton {
            hPad: 12
            text: qsTr("Girar der.")
            iconName: "rotate-right"
            visible: !root.editMode
            onClicked: if (root.canvas) root.canvas.rotateClockwise()
        }
        AppToolButton {
            hPad: 12
            text: qsTr("Ajustar")
            iconName: "fit"
            visible: !root.editMode
            onClicked: if (root.canvas) root.canvas.fitToWindow()
        }
        AppToolButton {
            id: slideshowButton
            hPad: 12
            text: qsTr("Presentación")
            iconName: checked ? "pause" : "play"
            visible: !root.editMode
            checkable: true
            enabled: folderModel.count > 1
            onToggled: if (!checked) slideshowTimer.stop()
        }

        // --- Edit-mode row: same slot, swapped to the actions that are
        // actually usable while the edit panel is open. Comparar/Deshacer/
        // Rehacer apply regardless of which tab is active, so they're always
        // shown while editing; the rest of the row is contextual.
        AppToolButton {
            hPad: 12
            text: qsTr("Comparar")
            iconName: "compare"
            visible: root.editMode
            enabled: appController.hasEdits
            // Held down (not toggled) - shows the as-opened original for as
            // long as the mouse is pressed, like most photo editors.
            onPressedChanged: if (root.canvas) root.canvas.compareOriginal = pressed
        }
        AppToolButton {
            hPad: 12
            text: qsTr("Deshacer")
            iconName: "undo"
            visible: root.editMode
            enabled: appController.canUndoEdit
            onClicked: appController.undoEdit()
        }
        AppToolButton {
            hPad: 12
            text: qsTr("Rehacer")
            iconName: "redo"
            visible: root.editMode
            enabled: appController.canRedoEdit
            onClicked: appController.redoEdit()
        }

        AppToolButton {
            hPad: 12
            text: qsTr("Restablecer")
            iconName: "reset"
            visible: root.editMode && !root.locked
            enabled: appController.hasEdits
            onClicked: appController.resetEdits()
        }
        AppToolButton {
            hPad: 12
            text: qsTr("Guardar")
            iconName: "save"
            visible: root.editMode && !root.locked
            // Also when the screen differs from the file without any active edit
            // (Guardar, then Deshacer): that is something to save too.
            enabled: (appController.hasEdits || appController.isDirty) && !appController.isLoading
                     && !appController.isSaving
            onClicked: root.saveRequested()
        }
        AppToolButton {
            hPad: 12
            text: qsTr("Guardar como…")
            iconName: "save-as"
            visible: root.editMode && !root.locked
            enabled: !appController.isSaving
            onClicked: root.saveAsRequested()
        }
        AppToolButton {
            hPad: 12
            text: qsTr("Cerrar edición")
            iconName: "close"
            visible: root.editMode && !root.locked
            onClicked: root.closeEditRequested()
        }

        Timer {
            id: slideshowTimer
            interval: appSettings.slideshowIntervalMs
            repeat: true
            running: slideshowButton.checked
            onTriggered: {
                if (!folderModel.advanceSlideshow(appSettings.slideshowRandom, appSettings.slideshowLoop))
                    slideshowButton.checked = false;
            }
        }
    }

    // A slideshow running into an edit session would keep swapping/cancelling
    // the image being edited out from under the user - stop it the moment
    // edit mode opens.
    onEditModeChanged: if (editMode) slideshowButton.checked = false
}
