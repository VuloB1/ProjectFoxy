import QtQuick
import QtQuick.Controls
import ImageViewerApp

// The notice of a newer version (UpdateManager, `updater`): it offers the update, shows the download, and asks to
// restart when the installer is ready. Sits at the top right of the window, below the title bar.
Rectangle {
    id: root
    objectName: "updateBanner"

    readonly property string state: updater.state
    // Words for the codes UpdateManager::error gives.
    function errorText(code) {
        switch (code) {
        case "network": return qsTr("No se pudo descargar. Revisa tu conexión e inténtalo de nuevo.")
        case "nosums": return qsTr("La versión no trae su suma de verificación, así que no se instala desde aquí.")
        case "corrupt": return qsTr("La descarga llegó dañada y se ha descartado.")
        case "disk": return qsTr("No se pudo guardar el instalador en el disco.")
        case "launch": return qsTr("No se pudo iniciar el instalador.")
        default: return qsTr("No se pudo comprobar la versión.")
        }
    }

    visible: updater.bannerVisible
    width: Math.min(440, (parent ? parent.width : 440) - 32)
    height: content.implicitHeight + 28
    radius: themeManager.radiusMedium === 0 ? 0 : 12
    color: themeManager.surfaceElevated
    border.color: themeManager.border
    border.width: 1

    Accessible.role: Accessible.AlertMessage
    Accessible.name: title.text

    Column {
        id: content
        x: 16; y: 14
        width: parent.width - 32
        spacing: 10

        Row {
            width: parent.width
            spacing: 12
            Image {
                width: 32; height: 32
                source: "qrc:/qt/qml/ImageViewerApp/resources/icons/icon.png"
                sourceSize: Qt.size(64, 64)
                smooth: true
            }
            Column {
                width: parent.width - 32 - 12 - closeButton.width - 12
                spacing: 2
                Label {
                    id: title
                    width: parent.width
                    wrapMode: Text.WordWrap
                    color: themeManager.textPrimary
                    font.bold: true
                    text: {
                        switch (root.state) {
                        case "downloading": return qsTr("Descargando la versión %1…").arg(updater.latestVersion)
                        case "ready": return qsTr("La versión %1 está lista para instalar").arg(updater.latestVersion)
                        case "installing": return qsTr("Instalando… el programa se reiniciará solo.")
                        case "failed": return qsTr("No se pudo actualizar")
                        default: return qsTr("Hay una versión nueva: %1").arg(updater.latestVersion)
                        }
                    }
                }
                Label {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    color: themeManager.textSecondary
                    font.pixelSize: 12
                    text: {
                        if (root.state === "failed")
                            return root.errorText(updater.error)
                        if (root.state === "ready" && appController.isDirty)
                            return qsTr("Guarda o descarta los cambios de la imagen para poder actualizar.")
                        if (root.state === "available")
                            return qsTr("Estás usando la %1.").arg(updater.currentVersion)
                        return ""
                    }
                    visible: text.length > 0
                }
            }
            AppToolButton {
                id: closeButton
                visible: root.state === "available" || root.state === "failed" || root.state === "ready"
                iconName: "close"
                iconOnly: true
                text: qsTr("Cerrar")
                onClicked: updater.dismiss()
            }
        }

        AppProgressBar {
            visible: root.state === "downloading"
            width: parent.width
            from: 0; to: 1
            value: updater.progress
        }

        Flow {
            width: parent.width
            spacing: 8
            visible: root.state === "available" || root.state === "failed" || root.state === "ready"
            AppButton {
                primary: true
                text: root.state === "ready" ? qsTr("Reiniciar e instalar")
                      : root.state === "failed" ? qsTr("Reintentar")
                      : updater.canInstall ? qsTr("Actualizar ahora") : qsTr("Descargar")
                enabled: root.state !== "ready" || !appController.isDirty
                onClicked: root.state === "ready" ? updater.installAndRestart() : updater.download()
            }
            AppButton {
                text: qsTr("Novedades")
                onClicked: updater.openReleasePage()
            }
            AppButton {
                visible: root.state === "available"
                text: qsTr("Omitir esta versión")
                onClicked: updater.skipVersion()
            }
        }
    }
}
