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
    // A creation studio ("gif" or "collage") is open: they take over the window, so nothing else is offered meanwhile.
    property string studio: ""
    signal editModeRequested(bool on)
    signal multiModeRequested(bool on)
    signal studioRequested(string name)

    function requestOpen() { openDialog.open() }
    function requestDelete() { deleteConfirmDialog.open() }

    Row {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 12
        spacing: 8

        // Navigation/zoom/rotate all live in StatusBar now -
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
            objectName: "editButton"
            iconName: "edit"
            iconOnly: true
            iconSize: 20
            iconStroke: 1.4
            topPadding: 3
            bottomPadding: 3
            checkable: true
            // Structural edits/filters bake a single static frame, which
            // doesn't fit a playing animation - see AppController::isAnimated.
            enabled: !appController.isAnimated && !root.locked && !root.multiMode && root.studio === ""
            checked: root.editMode
            onToggled: root.editModeRequested(checked)
        }
        AppToolButton {
            text: qsTr("GIF animado")
            objectName: "gifButton"
            iconName: "film"
            iconOnly: true
            iconSize: 20
            iconStroke: 1.4
            topPadding: 3
            bottomPadding: 3
            enabled: !root.locked
            checked: root.studio === "gif"
            onClicked: root.studioRequested(root.studio === "gif" ? "" : "gif")
        }
        AppToolButton {
            text: qsTr("Collage")
            objectName: "collageButton"
            iconName: "collage"
            iconOnly: true
            iconSize: 20
            iconStroke: 1.4
            topPadding: 3
            bottomPadding: 3
            enabled: !root.locked
            checked: root.studio === "collage"
            onClicked: root.studioRequested(root.studio === "collage" ? "" : "collage")
        }
        AppToolButton {
            text: qsTr("Comparador (varias imágenes con zoom vinculado)")
            iconName: "multi"
            iconOnly: true
            iconSize: 20
            iconStroke: 1.4
            topPadding: 3
            bottomPadding: 3
            checkable: true
            enabled: folderModel.count > 0 && !root.locked && root.studio === ""
            checked: root.multiMode
            onToggled: root.multiModeRequested(checked)
        }
        AppToolButton {
            id: moreButton
            text: qsTr("Más herramientas")
            iconName: "more"
            iconOnly: true
            iconSize: 20
            iconStroke: 1.4
            topPadding: 3
            bottomPadding: 3
            onClicked: moreMenu.popup(moreButton, moreButton.width - moreMenu.width, moreButton.height + 4)
            AppMenu {
                id: moreMenu
                AppMenuItem {
                    iconName: "export-batch"
                    text: qsTr("Exportar por lote…")
                    enabled: folderModel.count > 0 && !root.locked
                    onTriggered: batchExportDialog.open()
                }
                AppMenuItem {
                    iconName: "rename-batch"
                    text: qsTr("Renombrar por lote…")
                    enabled: folderModel.count > 0 && !root.locked
                    onTriggered: batchRenameDialog.open()
                }
            }
        }
        AppToolButton {
            text: qsTr("Configuración")
            objectName: "settingsButton"
            iconName: "settings"
            iconOnly: true
            iconSize: 20
            iconStroke: 1.4
            topPadding: 3
            bottomPadding: 3
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

    FileDialog {
        id: openDialog
        title: qsTr("Abrir imagen")
        nameFilters: [qsTr("Imágenes (%1)").arg(appController.supportedExtensions.join(" "))]
        onAccepted: appController.openFile(selectedFile)
    }
}
