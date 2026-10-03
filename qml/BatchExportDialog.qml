import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import ImageViewerApp

// "Exportar por lote": pick which pictures of the folder to convert (left), then the format,
// the destination and the largest side (right). While it runs the dialog cannot be dismissed
// by a stray click; it ends with a line saying how many went well.
AppDialog {
    id: root

    property url destFolder
    // "" until a run has ended, then what to tell the user.
    property string resultText: ""
    property bool resultBad: false

    modal: true
    standardButtons: Dialog.NoButton
    anchors.centerIn: Overlay.overlay
    padding: 0
    width: Math.min(900, (Overlay.overlay ? Overlay.overlay.width : 900) - 40)
    height: Math.min(600, (Overlay.overlay ? Overlay.overlay.height : 600) - 40)
    closePolicy: batchExporter.running ? Popup.NoAutoClose : (Popup.CloseOnEscape | Popup.CloseOnPressOutside)

    onAboutToShow: {
        files.reset(true);
        resultText = "";
    }

    Connections {
        target: batchExporter
        function onFinished(succeeded, failed) {
            root.resultBad = failed > 0;
            root.resultText = failed === 0
                ? qsTr("Listo: %1 imágenes exportadas.").arg(succeeded)
                : qsTr("Terminado: %1 exportadas y %2 con error.").arg(succeeded).arg(failed);
        }
    }

    contentItem: Item {
        implicitWidth: root.width
        implicitHeight: root.height

        Label {
            id: heading
            x: 28
            y: 22
            text: qsTr("Exportar por lote")
            color: themeManager.textPrimary
            font.bold: true
            font.pixelSize: 18
        }
        Label {
            x: 28
            y: 50
            text: qsTr("Elige las imágenes de esta carpeta que quieres convertir.")
            color: themeManager.textSecondary
            font.pixelSize: 12
        }

        BatchFileList {
            id: files
            anchors.top: parent.top
            anchors.topMargin: 88
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24
            anchors.left: parent.left
            anchors.leftMargin: 28
            width: Math.round(parent.width * 0.5) - 40
            enabled: !batchExporter.running
        }

        Rectangle {
            id: divider
            anchors.top: files.top
            anchors.bottom: files.bottom
            anchors.left: files.right
            anchors.leftMargin: 20
            width: 1
            color: themeManager.border
        }

        // ---- options ---------------------------------------------------------
        Column {
            id: options
            anchors.top: files.top
            anchors.left: divider.right
            anchors.leftMargin: 20
            anchors.right: parent.right
            anchors.rightMargin: 28
            spacing: 16

            Column {
                width: parent.width
                spacing: 6
                Label { text: qsTr("Formato"); color: themeManager.textSecondary }
                AppComboBox { id: formatCombo; width: parent.width; model: ["png", "jpg", "bmp", "tif", "webp"]; enabled: !batchExporter.running }
            }
            Column {
                width: parent.width
                spacing: 6
                Label { text: qsTr("Carpeta de destino"); color: themeManager.textSecondary }
                AppButton {
                    width: parent.width
                    enabled: !batchExporter.running
                    text: root.destFolder.toString().length > 0
                          ? decodeURIComponent(root.destFolder.toString().replace("file:///", ""))
                          : qsTr("Elegir carpeta…")
                    onClicked: destDialog.open()
                }
            }
            Column {
                width: parent.width
                spacing: 6
                Label { text: qsTr("Lado mayor máximo (px)"); color: themeManager.textSecondary }
                ScrubNumberField {
                    id: maxSide
                    width: parent.width
                    from: 0; to: 20000; decimals: 0; dragStep: 10; value: 0
                    enabled: !batchExporter.running
                }
                Label { text: qsTr("0 = conservar el tamaño original"); color: themeManager.textSecondary; font.pixelSize: 11 }
            }
            Label {
                width: parent.width
                text: qsTr("Calidad JPG y WebP: %1 % (se cambia en Configuración).").arg(appSettings.batchExportQuality)
                color: themeManager.textSecondary
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }

        // ---- bottom right: progress / result and the buttons -------------------------
        Column {
            anchors.left: divider.right
            anchors.leftMargin: 20
            anchors.right: parent.right
            anchors.rightMargin: 28
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24
            spacing: 12

            AppProgressBar {
                width: parent.width
                visible: batchExporter.running
                from: 0
                to: Math.max(1, batchExporter.total)
                value: batchExporter.progress
            }
            Label {
                width: parent.width
                visible: batchExporter.running
                color: themeManager.textSecondary
                text: qsTr("Exportando %1 de %2…").arg(batchExporter.progress).arg(batchExporter.total)
            }
            Label {
                width: parent.width
                visible: !batchExporter.running && root.resultText.length > 0
                text: root.resultText
                color: root.resultBad ? "#e5484d" : themeManager.accent
                font.bold: true
                wrapMode: Text.WordWrap
            }

            Row {
                spacing: 10
                anchors.right: parent.right

                AppButton {
                    width: 120
                    text: batchExporter.running ? qsTr("Detener") : qsTr("Cerrar")
                    onClicked: batchExporter.running ? batchExporter.cancel() : root.close()
                }
                AppButton {
                    width: 170
                    primary: true
                    visible: !batchExporter.running
                    enabled: files.selectedCount > 0 && root.destFolder.toString().length > 0
                    text: files.selectedCount > 0 ? qsTr("Exportar %1").arg(files.selectedCount) : qsTr("Exportar")
                    onClicked: {
                        root.resultText = "";
                        batchExporter.start(files.selectedPaths(), root.destFolder, formatCombo.currentText,
                                            Math.round(maxSide.value), appSettings.batchExportQuality);
                    }
                }
            }
        }
    }

    FolderDialog {
        id: destDialog
        title: qsTr("Elegir carpeta de destino")
        onAccepted: root.destFolder = selectedFolder
    }
}
