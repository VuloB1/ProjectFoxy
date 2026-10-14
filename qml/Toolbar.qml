import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import ImageViewerApp

// The actions behind the title bar's buttons: it owns the dialogs they open (Abrir, Configuración, the batch tools,
// the "delete this file" question) so the title bar itself stays a layout. It draws nothing; the buttons live in
// TitleBar.qml and call the functions below.
Item {
    id: root

    function requestOpen() { openDialog.open() }
    function requestDelete() { deleteConfirmDialog.open() }
    function openSettings() { settingsDialog.open() }
    function openBatchExport() { batchExportDialog.open() }
    function openBatchRename() { batchRenameDialog.open() }

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
        nameFilters: [qsTr("Imágenes (%1)").arg(appController.supportedExtensions.join(" "))]
        onAccepted: appController.openFile(selectedFile)
    }
}
