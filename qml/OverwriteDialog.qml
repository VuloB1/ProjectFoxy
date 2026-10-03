import QtQuick
import QtQuick.Controls
import ImageViewerApp

// "Guardar" writes straight over the original file. Asked before doing it (the
// choice can be switched off in Configuración, or here with "No volver a
// preguntar").
AppDialog {
    id: root

    // The user chose to overwrite / to save a copy instead.
    signal overwrite()
    signal saveAsCopy()

    readonly property bool recompresses: {
        const ext = appController.currentFileName.split(".").pop().toLowerCase();
        return ext === "jpg" || ext === "jpeg" || ext === "webp";
    }

    title: qsTr("Sobrescribir el original")
    modal: true
    standardButtons: Dialog.NoButton
    anchors.centerIn: Overlay.overlay
    onAboutToShow: dontAskAgain.checked = false

    Column {
        spacing: 12
        width: 360

        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            color: themeManager.textPrimary
            text: qsTr("«%1» se va a reemplazar por la imagen editada.").arg(appController.currentFileName)
        }
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            color: themeManager.textSecondary
            text: root.recompresses
                ? qsTr("Al ser JPEG/WebP se vuelve a comprimir (con calidad alta). Se conservan los datos de la cámara, la fecha, el GPS y el perfil de color. Si quieres quedarte también con la foto original, guarda una copia.")
                : qsTr("Se conservan los datos de la cámara, la fecha, el GPS y el perfil de color. Si quieres quedarte también con la foto original, guarda una copia.")
        }
        AppCheckBox { id: dontAskAgain; text: qsTr("No volver a preguntar") }
        Row {
            spacing: 8
            AppButton {
                text: qsTr("Sobrescribir")
                onClicked: {
                    if (dontAskAgain.checked)
                        appSettings.confirmOverwrite = false;
                    root.close();
                    root.overwrite();
                }
            }
            AppButton { text: qsTr("Guardar copia…"); onClicked: { root.close(); root.saveAsCopy(); } }
            AppButton { text: qsTr("Cancelar"); onClicked: root.close() }
        }
    }
}
