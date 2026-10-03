import QtQuick
import QtQuick.Controls
import ImageViewerApp

// "Renombrar por lote": pick which pictures of the folder to rename (left) and how
// (right): a base name, the number to start at and how many digits. Underneath, the new
// names are shown as they will come out, before anything is touched; if the rename would be
// refused (a name already taken by a file that was not picked...) it says why.
AppDialog {
    id: root

    // What FolderModel::checkRename says about the current picks and fields.
    property var plan: ({ ok: false, names: [], error: "" })
    property string failure: ""

    modal: true
    standardButtons: Dialog.NoButton
    anchors.centerIn: Overlay.overlay
    padding: 0
    width: Math.min(900, (Overlay.overlay ? Overlay.overlay.width : 900) - 40)
    height: Math.min(600, (Overlay.overlay ? Overlay.overlay.height : 600) - 40)

    onAboutToShow: {
        failure = "";
        files.reset(false); // nothing picked by default: renaming is not something to do by accident
        refresh();
    }

    function refresh() {
        const picked = files.selectedPaths();
        plan = picked.length === 0
            ? { ok: false, names: [], error: "" }
            : folderModel.checkRename(picked, baseField.text, Math.round(startField.value), digits.value);
    }

    contentItem: Item {
        implicitWidth: root.width
        implicitHeight: root.height

        Label {
            x: 28
            y: 22
            text: qsTr("Renombrar por lote")
            color: themeManager.textPrimary
            font.bold: true
            font.pixelSize: 18
        }
        Label {
            x: 28
            y: 50
            text: qsTr("Elige las imágenes que quieres renombrar; se numeran en el orden de la lista.")
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
            onSelectionChanged: root.refresh()
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

        // ---- how to name them -----------------------------------------------
        Column {
            id: options
            anchors.top: files.top
            anchors.left: divider.right
            anchors.leftMargin: 20
            anchors.right: parent.right
            anchors.rightMargin: 28
            spacing: 14

            Column {
                width: parent.width
                spacing: 6
                Label { text: qsTr("Nombre base"); color: themeManager.textSecondary }
                AppTextField {
                    id: baseField
                    width: parent.width
                    text: qsTr("imagen_")
                    onTextChanged: root.refresh()
                }
            }
            Row {
                spacing: 16
                width: parent.width
                Column {
                    width: (parent.width - 16) / 2
                    spacing: 6
                    Label { text: qsTr("Empezar en"); color: themeManager.textSecondary }
                    ScrubNumberField {
                        id: startField
                        width: parent.width
                        from: 0; to: 999999; decimals: 0; dragStep: 1; value: 1
                        onValueEdited: root.refresh()
                        onValueChanged: root.refresh()
                    }
                }
                Column {
                    width: (parent.width - 16) / 2
                    spacing: 6
                    Label { text: qsTr("Dígitos"); color: themeManager.textSecondary }
                    ScrubNumberField {
                        id: digits
                        width: parent.width
                        from: 1; to: 6; decimals: 0; dragStep: 1
                        value: appSettings.batchRenamePadding
                        onValueEdited: { appSettings.batchRenamePadding = newValue; root.refresh(); }
                        onValueChanged: root.refresh()
                    }
                }
            }

            // what the names will look like
            Label {
                text: qsTr("Así quedarán")
                color: themeManager.textSecondary
                topPadding: 4
            }
            Rectangle {
                width: parent.width
                height: 190
                radius: themeManager.radiusMedium === 0 ? 0 : 8
                color: themeManager.surface
                border.color: themeManager.border
                border.width: 1
                clip: true

                ListView {
                    id: preview
                    anchors.fill: parent
                    anchors.margins: 8
                    model: root.plan.ok ? files.selectedPaths().length : 0
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: AppScrollBar {}
                    AppWheelScroll { view: preview; step: 80 }
                    readonly property var paths: files.selectedPaths()
                    delegate: Item {
                        required property int index
                        width: preview.width
                        height: 24
                        readonly property string oldName: {
                            const p = preview.paths[index] || "";
                            return p.substring(p.lastIndexOf("/") + 1);
                        }
                        Label {
                            anchors.left: parent.left
                            anchors.right: arrow.left
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: parent.oldName
                            color: themeManager.textSecondary
                            elide: Text.ElideMiddle
                            horizontalAlignment: Text.AlignRight
                        }
                        Label {
                            id: arrow
                            x: parent.width / 2 - width / 2 + 6
                            anchors.verticalCenter: parent.verticalCenter
                            text: "→"
                            color: themeManager.textSecondary
                        }
                        Label {
                            anchors.left: arrow.right
                            anchors.leftMargin: 8
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.plan.names[index] || ""
                            color: themeManager.textPrimary
                            font.bold: true
                            elide: Text.ElideMiddle
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        width: parent.width - 20
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        visible: !root.plan.ok
                        text: files.selectedCount === 0 ? qsTr("Elige al menos una imagen.")
                              : (root.plan.error || "")
                        color: files.selectedCount === 0 ? themeManager.textSecondary : "#e5484d"
                    }
                }
            }
        }

        // ---- bottom right -----------------------------------------------------------
        Column {
            anchors.left: divider.right
            anchors.leftMargin: 20
            anchors.right: parent.right
            anchors.rightMargin: 28
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24
            spacing: 12

            Label {
                width: parent.width
                visible: root.failure.length > 0
                text: root.failure
                color: "#e5484d"
                font.bold: true
                wrapMode: Text.WordWrap
            }
            Row {
                spacing: 10
                anchors.right: parent.right
                AppButton { width: 120; text: qsTr("Cancelar"); onClicked: root.close() }
                AppButton {
                    width: 190
                    primary: true
                    enabled: root.plan.ok
                    text: files.selectedCount > 0 ? qsTr("Renombrar %1").arg(files.selectedCount) : qsTr("Renombrar")
                    onClicked: {
                        const result = folderModel.renameFiles(files.selectedPaths(), baseField.text,
                                                               Math.round(startField.value), digits.value);
                        if (result.ok)
                            root.close();
                        else
                            root.failure = result.error;
                    }
                }
            }
        }
    }
}
