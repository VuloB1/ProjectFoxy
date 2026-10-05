import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import ImageViewerApp

// Top toolbar: open, zoom controls, rotate, edit-mode toggle, theme toggle.
// Kept as a thin, mostly declarative component; actions call into C++
// context properties/models rather than containing logic themselves.
AppPanelBackground {
    id: root
    border.width: 0
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: themeManager.border }

    // Set by Main.qml so toolbar actions can drive the viewport/edit panel
    // without those components needing to know about each other.
    property var canvas: null
    // Edit mode is owned by Main.qml (the edit panel only exists while it is on).
    property bool editMode: false
    // A tool is open in the edit panel: edit mode (and the file) cannot be left meanwhile.
    property bool locked: false
    // "Varias imágenes" is on (owned by Main.qml too).
    property bool multiMode: false
    signal editModeRequested(bool on)
    signal multiModeRequested(bool on)

    function requestOpen() { openDialog.open() }
    function requestDelete() { deleteConfirmDialog.open() }

    Row {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 12
        spacing: 8

        // Navigation/zoom/rotate/slideshow all live in FloatingToolbar now -
        // this side of the top bar just shows what's currently open.
        Label {
            anchors.verticalCenter: parent.verticalCenter
            visible: appController.currentFileName.length > 0
            text: appController.currentFileName
            color: themeManager.textPrimary
            elide: Text.ElideMiddle
        }
    }

    Row {
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 12
        spacing: 8

        AppToolButton {
            text: qsTr("Editar")
            iconName: "edit"
            iconOnly: true
            checkable: true
            // Structural edits/filters bake a single static frame, which
            // doesn't fit a playing animation - see AppController::isAnimated.
            enabled: !appController.isAnimated && !root.locked && !root.multiMode
            checked: root.editMode
            onToggled: root.editModeRequested(checked)
        }
        AppToolButton {
            text: qsTr("Varias imágenes (con zoom vinculado)")
            iconName: "multi"
            iconOnly: true
            checkable: true
            enabled: folderModel.count > 0 && !root.locked
            checked: root.multiMode
            onToggled: root.multiModeRequested(checked)
        }
        AppToolButton {
            text: qsTr("Exportar por lote")
            iconName: "export-batch"
            iconOnly: true
            enabled: folderModel.count > 0 && !root.locked
            onClicked: batchExportDialog.open()
        }
        AppToolButton {
            text: qsTr("Renombrar por lote")
            iconName: "rename-batch"
            iconOnly: true
            enabled: folderModel.count > 0 && !root.locked
            onClicked: batchRenameDialog.open()
        }
        AppToolButton {
            text: qsTr("Información")
            iconName: "info"
            iconOnly: true
            onClicked: infoPopup.open()
        }
        AppToolButton {
            text: qsTr("Configuración")
            iconName: "settings"
            iconOnly: true
            onClicked: settingsDialog.open()
        }
    }

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

    Popup {
        id: infoPopup
        x: root.width - width - 12
        y: root.height + 4
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: AppPanelBackground { color: themeManager.surfaceElevated; radius: themeManager.radiusMedium }

        contentItem: Column {
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

    FileDialog {
        id: openDialog
        title: qsTr("Abrir imagen")
        nameFilters: [qsTr("Imágenes (%1)").arg(appController.supportedExtensions.join(" "))]
        onAccepted: appController.openFile(selectedFile)
    }
}
