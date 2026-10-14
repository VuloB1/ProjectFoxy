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
    title: "Project Foxy"
    // The native frame is off: TitleBar.qml is the title bar and the edge handles below resize it.
    flags: Qt.Window | Qt.FramelessWindowHint
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
    // The reading button: the folder of the picture that is open (from that picture), or a file to choose.
    function readCurrentFolder() {
        if (mangaReader.active) {
            mangaReader.close();
            return;
        }
        const paths = folderModel.count > 0 ? folderModel.filePaths() : [];
        if (paths.length > 0 && folderModel.currentIndex >= 0)
            root.readBook(paths[folderModel.currentIndex]);
        else
            toolbar.requestReadComic();
    }
    function requestSaveAs() {
        if (editLoader.item)
            editLoader.item.requestSaveAs();
    }

    // "Varias imágenes": several pictures side by side with linked zoom (MultiView.qml). It takes the
    // canvas's place; editing and browsing wait until it is closed.
    property bool multiMode: false
    // A creation studio ("gif") covers the window below the title bar.
    property string studio: ""
    function setStudio(name) {
        if (name === root.studio)
            return;
        if (name !== "") {
            if (root.toolLocked || appController.isLoading)
                return;
            root.setEditMode(false);
            root.setMultiMode(false);
        }
        root.studio = name;
    }
    // Leaving a studio (or going to the other one) with something made and not saved asks first.
    property string pendingStudio: ""
    function requestStudio(name) {
        if (root.studio !== "" && name !== root.studio && studioLoader.item && studioLoader.item.hasWork) {
            root.pendingStudio = name;
            leaveStudioDialog.open();
            return;
        }
        root.setStudio(name);
    }
    function setMultiMode(on) {
        if (on === root.multiMode)
            return;
        if (on) {
            if (root.toolLocked || appController.isLoading)
                return;
            root.setEditMode(false);
        }
        root.multiMode = on;
    }


    // The title bar is also the toolbar (TitleBar.qml); Toolbar.qml only owns the dialogs its buttons open.
    Toolbar { id: toolbar; onReadChosen: function (file) { root.readBook(file) } }

    TitleBar {
        id: titleBar
        objectName: "titleBar"
        window: root
        toolbar: toolbar
        editMode: root.editMode
        locked: root.toolLocked
        multiMode: root.multiMode
        studio: root.studio
        reading: mangaReader.active
        onReadRequested: root.readCurrentFolder()
        onEditModeRequested: function (on) { root.setEditMode(on) }
        onMultiModeRequested: function (on) { root.setMultiMode(on) }
        onStudioRequested: function (name) { root.requestStudio(name) }
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 36
        z: 10
    }

    // Grips along the edges and corners (not while maximised).
    Repeater {
        model: [
            { e: Qt.LeftEdge, c: Qt.SizeHorCursor }, { e: Qt.RightEdge, c: Qt.SizeHorCursor },
            { e: Qt.TopEdge, c: Qt.SizeVerCursor }, { e: Qt.BottomEdge, c: Qt.SizeVerCursor },
            { e: Qt.LeftEdge | Qt.TopEdge, c: Qt.SizeFDiagCursor }, { e: Qt.RightEdge | Qt.BottomEdge, c: Qt.SizeFDiagCursor },
            { e: Qt.RightEdge | Qt.TopEdge, c: Qt.SizeBDiagCursor }, { e: Qt.LeftEdge | Qt.BottomEdge, c: Qt.SizeBDiagCursor }
        ]
        delegate: MouseArea {
            required property var modelData
            readonly property bool corner: (modelData.e & (Qt.LeftEdge | Qt.RightEdge)) && (modelData.e & (Qt.TopEdge | Qt.BottomEdge))
            enabled: root.visibility !== Window.Maximized
            z: 20
            cursorShape: modelData.c
            width: (modelData.e & Qt.LeftEdge) || (modelData.e & Qt.RightEdge) ? 6 : root.width - 12
            height: (modelData.e & Qt.TopEdge) || (modelData.e & Qt.BottomEdge) ? 6 : root.height - 12
            x: (modelData.e & Qt.RightEdge) ? root.width - width : (modelData.e & Qt.LeftEdge) ? 0 : 6
            y: (modelData.e & Qt.BottomEdge) ? root.height - height : (modelData.e & Qt.TopEdge) ? 0 : 6
            onPressed: root.startSystemResize(modelData.e)
        }
    }

    ThumbnailStrip {
        id: sidebar
        anchors.top: titleBar.bottom
        anchors.left: parent.left
        anchors.bottom: statusBar.top
        visible: folderModel.count > 1
        busy: root.editMode
        enabled: !root.toolLocked && !root.multiMode && root.studio === ""
        opacity: enabled ? 1.0 : 0.45
    }

    ImageCanvas {
        id: canvas
        anchors.top: titleBar.bottom
        anchors.left: sidebar.visible ? sidebar.right : parent.left
        anchors.right: root.editMode ? editLoader.left : parent.right
        anchors.bottom: statusBar.top
        // The picture opens (and stays centred) above the dock, but zoomed it may run on behind the sides of it.
        bottomInset: statusBar.dockVisible ? statusBar.rise : 0
        toolbar: toolbar
        editMode: root.editMode
        locked: root.toolLocked
        visible: !root.multiMode && root.studio === ""
        onEditModeRequested: function (on) { root.setEditMode(on) }
    }

    // Several pictures at once; it only exists while it is shown.
    Loader {
        id: multiLoader
        active: root.multiMode
        anchors.top: canvas.top
        anchors.bottom: canvas.bottom
        anchors.bottomMargin: canvas.bottomInset // the comparer keeps clear of the dock, as it always did
        anchors.left: canvas.left
        anchors.right: canvas.right
        sourceComponent: MultiView {
            onCloseRequested: root.setMultiMode(false)
        }
        onLoaded: item.begin()
    }

    // The animation studio; like the multi view it only exists while it is open, and takes the whole window.
    Component { id: gifStudioComponent; GifStudio { onCloseRequested: root.setStudio("") } }
    Component { id: collageComponent; CollageMaker { onCloseRequested: root.setStudio("") } }
    Loader {
        id: studioLoader
        active: root.studio !== ""
        anchors.top: titleBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: statusBar.top
        z: 6
        sourceComponent: root.studio === "gif" ? gifStudioComponent : collageComponent
    }

    // The comic / manga reader (ReaderView.qml) takes the whole window below the title bar while a book is open.
    Loader {
        id: readerLoader
        active: mangaReader.active
        anchors.top: titleBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: statusBar.top
        z: 3 // under the bottom bar, whose dock rises over the page
        sourceComponent: ReaderView {
            bottomInset: statusBar.rise
            onCloseRequested: mangaReader.close()
        }
        onLoaded: item.forceActiveFocus()
    }
    // A .cbz opened like any other file (Abrir, drag and drop, "Open with") is read, not shown.
    Connections {
        target: appController
        function onComicRequested(path) { root.readBook(path) }
    }
    function readBook(path) {
        if (root.toolLocked)
            return;
        root.setEditMode(false);
        root.setMultiMode(false);
        root.setStudio("");
        if (!mangaReader.open(path))
            readerErrorDialog.open();
    }
    ConfirmDialog {
        id: readerErrorDialog
        symbol: "info"
        danger: true
        heading: qsTr("No se pudo abrir para leer")
        message: mangaReader.error
        cancelText: qsTr("Cerrar")
        confirmText: qsTr("Aceptar")
    }

    Loader {
        id: editLoader
        active: root.editMode
        anchors.top: titleBar.bottom
        anchors.right: parent.right
        anchors.bottom: statusBar.top
        width: root.editMode ? 330 : 0
        sourceComponent: EditPanel {
            anchors.fill: parent
            // Not `canvas: canvas`: inside this component that names the panel's own property.
            canvas: root.canvasItem
        }
        // The panel starts hidden (its show handler opens the crop); show it now it is wired up.
        onLoaded: item.visible = true
    }

    StatusBar {
        id: statusBar
        canvas: canvas
        toolbar: toolbar
        editMode: root.editMode
        locked: root.toolLocked
        onSaveAsRequested: root.requestSaveAs()
        onCloseEditRequested: root.setEditMode(false)
        dockVisible: root.studio === ""
        customDock: readerLoader.item ? readerLoader.item.dockContent
                                      : (multiLoader.item ? multiLoader.item.dockContent : null)
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 30
        z: 5
        onSaveRequested: root.requestSave()
    }

    ConfirmDialog {
        id: leaveStudioDialog
        symbol: "save"
        danger: true
        heading: qsTr("¿Salir sin guardar?")
        message: qsTr("Lo que armaste en este estudio se va a descartar.")
        detail: qsTr("Si quieres conservarlo, vuelve y usa «Guardar como…».")
        cancelText: qsTr("Seguir aquí")
        confirmText: qsTr("Salir sin guardar")
        onConfirmed: {
            if (studioLoader.item)
                studioLoader.item.discard();
            root.setStudio(root.pendingStudio);
        }
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
        enabled: !root.toolLocked && !root.multiMode && root.studio === ""
        onDropped: function (drop) {
            if (drop.hasUrls && drop.urls.length > 0)
                appController.openFile(drop.urls[0]);
        }
    }

    Shortcut { sequence: StandardKey.MoveToPreviousChar; enabled: !root.toolLocked && !root.multiMode && root.studio === "" && !mangaReader.active; onActivated: folderModel.previous() }
    Shortcut { sequence: StandardKey.MoveToNextChar; enabled: !root.toolLocked && !root.multiMode && root.studio === "" && !mangaReader.active; onActivated: folderModel.next() }
    Shortcut { sequences: [StandardKey.Open]; enabled: !root.toolLocked && !root.multiMode && root.studio === "" && !mangaReader.active; onActivated: toolbar.requestOpen() }
    // Disabled while editing (so it doesn't replace the image mid-edit) and
    // while a text field has focus (so Ctrl+V pastes text there as normal,
    // e.g. the batch rename/export dialogs) - "cursorPosition" is a cheap
    // duck-typed check for "this is some kind of text input".
    // `sequences` (a list), not `sequence`: StandardKey.Paste stands for several
    // key combinations (Ctrl+V, Shift+Insert, ...) and `sequence` would bind
    // only the first of them.
    Shortcut {
        sequences: [StandardKey.Paste]
        enabled: !root.editMode && !mangaReader.active && !(root.activeFocusItem && "cursorPosition" in root.activeFocusItem)
        onActivated: appController.pasteFromClipboard()
    }
}
