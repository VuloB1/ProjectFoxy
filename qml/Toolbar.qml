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

    function requestOpen() { openDialog.open() }
    function requestDelete() { deleteConfirmDialog.open() }
    function openSettings() { settingsDialog.open() }
    function openBatchExport() { batchExportDialog.open() }
    function openBatchRename() { batchRenameDialog.open() }
    function requestReadComic() { readDialog.open() }

    SettingsDialog {
        id: settingsDialog
    }

    ConfirmDialog {
        id: deleteConfirmDialog
        symbol: "trash"
        danger: true
        heading: qsTr("¿Eliminar este archivo?")
        message: qsTr("«%1» se enviará a la Papelera de reciclaje.").arg(appController.currentFileName)
        detail: qsTr("Podrás recuperarlo desde la Papelera si cambias de idea.")
        confirmText: qsTr("Eliminar")
        onConfirmed: appController.deleteCurrentFile()
    }

    BatchRenameDialog { id: batchRenameDialog }
    BatchExportDialog { id: batchExportDialog }

    FileDialog {
        id: openDialog
        title: qsTr("Abrir imagen")
        nameFilters: [qsTr("Imágenes y cómics (%1)").arg(appController.supportedExtensions.concat(["*.cbz", "*.zip", "*.pdf"]).join(" "))]
        onAccepted: appController.openFile(selectedFile)
    }

    // "Leer": a comic file (.cbz) or any picture of a folder to read that folder as a book.
    FileDialog {
        id: readDialog
        title: qsTr("Leer un cómic o manga")
        nameFilters: [qsTr("Cómics (%1)").arg("*.cbz *.zip *.pdf"), qsTr("Imágenes (%1)").arg(appController.supportedExtensions.join(" "))]
        onAccepted: root.readChosen(selectedFile)
    }
}
