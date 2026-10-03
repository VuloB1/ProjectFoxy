import QtQuick
import QtQuick.Controls
import ImageViewerApp

ApplicationWindow {
    id: root
    width: 1280
    height: 800
    minimumWidth: 480
    minimumHeight: 320
    visible: true
    title: "ImageViewer"
    color: themeManager.background
    font.family: themeManager.fontFamily

    // Set once the user has answered the "unsaved changes" question for closing.
    property bool closeApproved: false
    // The window was asked to close while a file was still being written (or the user
    // answered "Guardar" to the close question): it closes once the write has ended OK.
    property bool closeWhenSaved: false

    // Closing the window with edits that exist only in memory asks first.
    onClosing: function (close) {
        if (appController.isSaving) {
            close.accepted = false;
            root.closeWhenSaved = true;
            return;
        }
        if (!root.closeApproved && appController.isDirty) {
            close.accepted = false;
            unsavedDialog.intent = "close";
            unsavedDialog.open();
        }
    }

    // "Guardar": over the original file (after asking, unless switched off), or - for
    // a picture that has no file, or a format that cannot be rewritten - as a copy.
    function requestSave() {
        if (appController.isLoading || appController.isSaving)
            return; // another picture is about to replace this one / a write is running (the backend refuses too)
        if (!appController.canSaveInPlace)
            root.requestSaveAs();
        else if (appSettings.confirmOverwrite)
            overwriteDialog.open();
        else
            appController.saveEdited();
    }

    // Edit mode. The edit panel (sliders, curves, looks, effects...) does not exist
    // until it is switched on, so just viewing pictures never pays for it.
    property bool editMode: false
    readonly property Item canvasItem: canvas
    // A tool (Recortar, Ajustes...) is open in the edit panel: it can only be left through
    // its own Aplicar / Cancelar, so nothing that would leave edit mode, change the picture
    // or move to another one is available meanwhile.
    readonly property bool toolLocked: editLoader.item ? editLoader.item.toolOpen : false
    function setEditMode(on) {
        if (on === root.editMode)
            return;
        if (on && appController.isAnimated)
            return; // edits bake a single still frame
        if (!on && editLoader.item) {
            editLoader.item.forceClose();
            editLoader.item.visible = false; // its hide handler cancels a pending crop / effect preview
        }
        root.editMode = on;
    }
    function requestSaveAs() {
        if (editLoader.item)
            editLoader.item.requestSaveAs();
    }

    // Frameless + custom-drawn titlebar comes later (Fase 3 polish); using
    // the native frame for now keeps window management (snap, move, resize)
    // free and correct while the core viewer is being built.

    Toolbar {
        id: toolbar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 48
        canvas: canvas
        editMode: root.editMode
        locked: root.toolLocked
        onEditModeRequested: function (on) { root.setEditMode(on) }
    }

    ThumbnailStrip {
        id: sidebar
        anchors.top: toolbar.bottom
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        visible: folderModel.count > 1
        enabled: !root.toolLocked
        opacity: enabled ? 1.0 : 0.45
    }

    ImageCanvas {
        id: canvas
        anchors.top: toolbar.bottom
        anchors.left: sidebar.visible ? sidebar.right : parent.left
        anchors.right: root.editMode ? editLoader.left : parent.right
        anchors.bottom: parent.bottom
        toolbar: toolbar
        editMode: root.editMode
        locked: root.toolLocked
        onEditModeRequested: function (on) { root.setEditMode(on) }
    }

    Loader {
        id: editLoader
        active: root.editMode
        anchors.top: toolbar.bottom
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: root.editMode ? 330 : 0
        sourceComponent: EditPanel {
            anchors.fill: parent
            // Not `canvas: canvas`: inside this component that names the panel's own property.
            canvas: root.canvasItem
        }
        // The panel starts hidden (its show handler opens the crop); show it now it is wired up.
        onLoaded: item.visible = true
    }

    FloatingToolbar {
        id: floatingToolbar
        canvas: canvas
        toolbar: toolbar
        editMode: root.editMode
        locked: root.toolLocked
        onSaveAsRequested: root.requestSaveAs()
        onCloseEditRequested: root.setEditMode(false)
        anchors.bottom: canvas.bottom
        anchors.bottomMargin: 16
        anchors.horizontalCenter: canvas.horizontalCenter
        z: 5
        onSaveRequested: root.requestSave()
    }

    UnsavedChangesDialog {
        id: unsavedDialog
        onCloseConfirmed: {
            root.closeApproved = true;
            root.close();
        }
        // "Guardar" was chosen: the file is written first, then the window closes.
        onSaveThenClose: root.closeWhenSaved = true
    }
    OverwriteDialog {
        id: overwriteDialog
        onOverwrite: appController.saveEdited()
        onSaveAsCopy: root.requestSaveAs()
    }

    // The edit panel bakes a single static frame - not meaningful while an
    // animation is playing, so close it rather than leave it open against a
    // frame that's about to keep changing under it.
    Connections {
        target: appController
        function onSaveFinished(ok, path) {
            if (!root.closeWhenSaved)
                return;
            root.closeWhenSaved = false;
            if (ok)
                root.close(); // (asks again if it was edited while it was being saved)
        }
        function onNewImageLoaded() { if (appController.isAnimated) root.setEditMode(false); }
        // Another picture was requested while this one has unsaved edits.
        function onUnsavedChangesBlocked() {
            unsavedDialog.intent = "navigate";
            unsavedDialog.open();
        }
    }

    Shortcut {
        sequences: [StandardKey.Save]
        enabled: root.editMode && !root.toolLocked && (appController.hasEdits || appController.isDirty)
                 && !appController.isLoading && !appController.isSaving
        onActivated: root.requestSave()
    }

    // Drag & drop onto the window opens the file / navigates its folder.
    DropArea {
        anchors.fill: parent
        enabled: !root.toolLocked
        onDropped: function (drop) {
            if (drop.hasUrls && drop.urls.length > 0)
                appController.openFile(drop.urls[0]);
        }
    }

    Shortcut { sequence: StandardKey.MoveToPreviousChar; enabled: !root.toolLocked; onActivated: folderModel.previous() }
    Shortcut { sequence: StandardKey.MoveToNextChar; enabled: !root.toolLocked; onActivated: folderModel.next() }
    Shortcut { sequence: StandardKey.Open; enabled: !root.toolLocked; onActivated: toolbar.requestOpen() }
    // Disabled while editing (so it doesn't replace the image mid-edit) and
    // while a text field has focus (so Ctrl+V pastes text there as normal,
    // e.g. the batch rename/export dialogs) - "cursorPosition" is a cheap
    // duck-typed check for "this is some kind of text input".
    // `sequences` (a list), not `sequence`: StandardKey.Paste stands for several
    // key combinations (Ctrl+V, Shift+Insert, ...) and `sequence` would bind
    // only the first of them.
    Shortcut {
        sequences: [StandardKey.Paste]
        enabled: !root.editMode && !(root.activeFocusItem && "cursorPosition" in root.activeFocusItem)
        onActivated: appController.pasteFromClipboard()
    }
}
