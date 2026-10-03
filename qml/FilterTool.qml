import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Filtros: a grid of looks (each a preview of this very photo) and, pinned underneath,
// the strength slider. Live like Ajustes; Aplicar keeps, Cancelar undoes.
Item {
    id: root

    property var canvas: null
    readonly property bool canApply: appController.toolSessionDirty
    function apply() {}
    function cancel() {}

    // --- Filtros ----------------------------------------------------------
    // Looks are recipes (see core/edit/Looks.h); every cell is a preview of
    // the current photo with that look on. The look itself is drawn live by
    // the first Grade shader in ImageCanvas.qml.
    Flickable {
        id: filterFlick
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: amountBox.top
        anchors.bottomMargin: 10
        contentWidth: width
        contentHeight: filterColumn.implicitHeight + 8
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: AppScrollBar { id: filterBar }
        AppWheelScroll { view: filterFlick }

        // "" = every look, otherwise a group id from appController.lookGroups
        property string group: ""
        readonly property var shownLooks: appController.lookList.filter(
            function (look) { return filterFlick.group === "" || look.group === filterFlick.group; })

        // The previews are only rendered while this tab is on screen, and
        // again whenever the photo underneath changes (crop, rotate, next
        // image...). Cheap when they are already current.
        function refreshPreviews() {
            if (visible)
                appController.ensureLookThumbnails();
        }
        onVisibleChanged: refreshPreviews()
        Component.onCompleted: refreshPreviews()
        Connections {
            target: appController
            function onStructuralImageChanged() { filterFlick.refreshPreviews(); }
        }

        Column {
            id: filterColumn
            width: filterFlick.width - filterBar.implicitWidth - 4 // clear of the scroll bar
            spacing: 10

            Flow {
                width: parent.width
                spacing: 4
                Repeater {
                    model: [{ id: "", name: qsTr("Todos") }].concat(appController.lookGroups)
                    delegate: AppToolButton {
                        required property var modelData
                        text: modelData.name
                        checked: filterFlick.group === modelData.id
                        onClicked: filterFlick.group = modelData.id
                    }
                }
            }

            Grid {
                columns: 3
                spacing: 6
                width: parent.width
                readonly property real cell: (width - spacing * 2) / 3

                LookCell {
                    width: parent.cell
                    lookId: ""
                    label: qsTr("Ninguno")
                    selected: appController.liveFilterId === ""
                    onPicked: appController.applyFilterPreset("", amountSlider.value)
                }
                Repeater {
                    model: filterFlick.shownLooks
                    delegate: LookCell {
                        required property var modelData
                        width: parent.cell
                        lookId: modelData.id
                        label: modelData.name
                        selected: appController.liveFilterId === modelData.id
                        onPicked: appController.applyFilterPreset(modelData.id, amountSlider.value)
                    }
                }
            }
        }
    }

    // How strongly the picked look is applied: pinned above the buttons, always in reach.
    Item {
        id: amountBox
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 42
        enabled: appController.liveFilterId !== ""
        opacity: enabled ? 1.0 : 0.5

        Label {
            id: amountCaption
            anchors.left: parent.left
            anchors.top: parent.top
            text: qsTr("Cantidad")
            color: themeManager.textSecondary
        }
        Label {
            anchors.right: parent.right
            anchors.top: parent.top
            text: Math.round(amountSlider.value * 100)
            color: amountSlider.value < 0.9995 ? themeManager.accent : themeManager.textPrimary
            font.bold: true
        }
        AppSlider {
            id: amountSlider
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: amountCaption.bottom
            anchors.topMargin: 2
            from: 0
            to: 1
            value: 1
            onMoved: appController.applyFilterPreset(appController.liveFilterId, value)
        }
        // Back to 100% when the look is cleared (Restablecer, a new image).
        Connections {
            target: appController
            function onLiveFilterChanged() {
                if (Math.abs(amountSlider.value - appController.liveFilterIntensity) > 0.0005)
                    amountSlider.value = appController.liveFilterIntensity;
            }
        }
    }
}
