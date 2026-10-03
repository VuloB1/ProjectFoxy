import QtQuick
import QtQuick.Controls
import ImageViewerApp

// "Hay cambios sin guardar": asked before the window closes, or before another
// picture replaces the one being edited. The user can save them (when the open
// file can be overwritten), discard them, or cancel and stay.
AppDialog {
    id: root

    // "navigate": another picture / the clipboard was requested (AppController is
    // holding that request until this dialog answers it through resolveUnsaved()).
    // "close": the window is closing.
    property string intent: "navigate"
    // Emitted when intent is "close" and the user chose to go ahead (saved, or
    // discarded): the window may close now.
    signal closeConfirmed()
    // Emitted when intent is "close" and the user chose "Guardar": the save has STARTED
    // (it runs in the background); the window must wait for appController.saveFinished.
    signal saveThenClose()

    // Closing by Escape or a click outside is "cancel".
    property bool answered: false

    title: qsTr("Cambios sin guardar")
    modal: true
    standardButtons: Dialog.NoButton
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape

    onAboutToShow: answered = false
    onClosed: {
        if (!answered && intent === "navigate")
            appController.resolveUnsaved("cancel");
    }

    function answer(choice) {
        answered = true;
        if (intent === "navigate") {
            appController.resolveUnsaved(choice);
        } else if (choice === "discard") {
            closeConfirmed();
        } else if (choice === "save" && appController.saveEdited()) {
            saveThenClose();
        }
        close();
    }

    Column {
        spacing: 12
        width: 360

        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            color: themeManager.textPrimary
            text: qsTr("«%1» tiene cambios que no se han guardado.").arg(appController.currentFileName)
        }
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            color: themeManager.textSecondary
            text: root.intent === "close"
                ? qsTr("¿Quieres guardarlos antes de cerrar?")
                : qsTr("¿Quieres guardarlos antes de abrir otra imagen?")
        }
        Label {
            visible: !appController.canSaveInPlace
            width: parent.width
            wrapMode: Text.WordWrap
            color: themeManager.textSecondary
            text: qsTr("Este archivo no se puede sobrescribir. Cancela y usa «Guardar como…» si quieres conservarlos.")
        }
        Row {
            spacing: 8
            AppButton {
                text: qsTr("Guardar")
                visible: appController.canSaveInPlace
                onClicked: root.answer("save")
            }
            AppButton { text: qsTr("Descartar"); onClicked: root.answer("discard") }
            AppButton { text: qsTr("Cancelar"); onClicked: root.answer("cancel") }
        }
    }
}
