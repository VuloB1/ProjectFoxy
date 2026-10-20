import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import ImageViewerApp

// The actions behind the title bar's buttons: it owns the dialogs they open (Abrir, Configuración, the batch tools,
// the "delete this file" question) so the title bar itself stays a layout. It draws nothing; the buttons live in
// TitleBar.qml and call the functions below.
Item {
    id: root

    // The file chosen in "Leer un cómic o manga"; Main.qml opens the reader on it.
    signal readChosen(string file)

    // One Open for pictures, comics and videos (a video goes to the animation studio: Main.qml). The video extensions are
    // asked when the dialog opens, not at start: that loads the multimedia plugin.
    function requestOpen() {
        const openDialog = show(openLoader);
        openDialog.nameFilters = [qsTr("Imágenes, vídeos y cómics (%1)").arg(appController.supportedExtensions
            .concat(["*.cbz", "*.zip", "*.pdf"]).concat(videoSupport.extensions.map(function (e) { return "*." + e; })).join(" "))];
        openDialog.open();
    }
    function requestDelete() { show(deleteLoader).open() }
    function openSettings() { show(settingsLoader).open() }
    function openBatchExport() { show(batchExportLoader).open() }
    function openBatchRename() { show(batchRenameLoader).open() }
    function requestReadComic() { show(readLoader).open() }

    // The dialogs are built the first time they are asked for, not when the window opens (together they took
    // about 50 ms of the start): `show` makes the dialog of a loader if it is not there yet and gives it back.
    function show(loader) {
        if (!loader.active)
            loader.active = true;
        return loader.item;
    }

    Loader { id: settingsLoader; active: false; sourceComponent: SettingsDialog {} }

    Loader {
        id: deleteLoader
        active: false
        sourceComponent: ConfirmDialog {
            symbol: "trash"
            danger: true
            heading: qsTr("¿Eliminar este archivo?")
            message: qsTr("«%1» se enviará a la Papelera de reciclaje.").arg(appController.currentFileName)
            detail: qsTr("Podrás recuperarlo desde la Papelera si cambias de idea.")
            confirmText: qsTr("Eliminar")
            onConfirmed: appController.deleteCurrentFile()
        }
    }

    Loader { id: batchRenameLoader; active: false; sourceComponent: BatchRenameDialog {} }
    Loader { id: batchExportLoader; active: false; sourceComponent: BatchExportDialog {} }

    Loader {
        id: openLoader
        active: false
        sourceComponent: FileDialog {
            title: qsTr("Abrir imagen o vídeo")
            onAccepted: appController.openFile(selectedFile)
        }
    }

    // "Leer": a comic file (.cbz) or any picture of a folder to read that folder as a book.
    Loader {
        id: readLoader
        active: false
        sourceComponent: FileDialog {
            title: qsTr("Leer un cómic o manga")
            nameFilters: [qsTr("Cómics (%1)").arg("*.cbz *.zip *.pdf"), qsTr("Imágenes (%1)").arg(appController.supportedExtensions.join(" "))]
            onAccepted: root.readChosen(selectedFile)
        }
    }
}
