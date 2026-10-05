import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import ImageViewerApp

// The edit mode's side panel. It shows the list of tools (Recortar, Tamaño, Ajustes,
// Filtros, Efectos); picking one slides that tool's page in over the
// list. A tool is a transaction: the page has no way out except its own Cancelar
// (undoes everything done in it) or Aplicar (keeps it), so nobody leaves a tool with a
// half-made change. Each tool lives in its own file and is only created while it is
// open (see CropTool / ResizeTool / AdjustTool / FilterTool / EffectsTool); they all
// offer the same small interface: canApply, apply(), cancel().
//
// What is done inside a tool goes through appController's history, between
// beginToolSession() and endToolSession(): that is what makes Cancelar exact.
AppPanelBackground {
    id: root
    visible: false

    property var canvas: null

    // The tool that is open ("" = the list is showing).
    property string tool: ""
    // The last tool opened: the page keeps showing its title while it slides away.
    property string shownTool: ""
    readonly property bool toolOpen: tool !== ""

    readonly property var tools: [
        { id: "crop", icon: "crop", name: qsTr("Recortar"), hint: qsTr("Recorte, giro, volteo y enderezado") },
        { id: "resize", icon: "resize", name: qsTr("Tamaño"), hint: qsTr("Ancho, alto y escala") },
        { id: "adjust", icon: "sliders", name: qsTr("Ajustes"), hint: qsTr("Luz, color, detalle, niveles y curvas") },
        { id: "filters", icon: "filters", name: qsTr("Filtros"), hint: qsTr("Estilos listos para aplicar") },
        { id: "effects", icon: "effects", name: qsTr("Efectos"), hint: qsTr("Desenfoque, color, luz, dibujos, distorsión y más") },
        { id: "lens", icon: "lens", name: qsTr("Lente"), hint: qsTr("Distorsión, franjas de color y esquinas oscuras") }
    ]

    function toolInfo(id) {
        for (const t of tools)
            if (t.id === id)
                return t;
        return { id: "", icon: "", name: "", hint: "" };
    }

    function openTool(id) {
        if (toolOpen || appController.isLoading || !canvas)
            return;
        appController.beginToolSession();
        toolLoader.sourceComponent = id === "crop" ? cropComponent
            : id === "resize" ? resizeComponent
            : id === "adjust" ? adjustComponent
            : id === "filters" ? filterComponent
            : id === "lens" ? lensComponent
            : effectsComponent;
        shownTool = id;
        tool = id;
    }

    // apply = true keeps what the tool did, false undoes it. Either way the page slides out.
    function closeTool(apply) {
        const page = toolLoader.item;
        if (!toolOpen || !page)
            return;
        if (apply) {
            page.apply();
            appController.endToolSession(true);
        } else {
            page.cancel();
            appController.endToolSession(false);
        }
        tool = "";
    }

    // Edit mode is being switched off (or the picture was replaced) with a tool still
    // open: nothing is kept, and there is no animation to wait for.
    function forceClose() {
        if (!toolOpen)
            return;
        closeTool(false);
        toolLoader.sourceComponent = null;
    }

    onVisibleChanged: {
        if (!visible)
            appController.cancelEffect(); // an effect that was only being previewed
    }

    // The picture was replaced under an open tool (undo of a load, a paste...): its
    // session is gone with the old document, so just drop the page.
    Connections {
        target: appController
        function onNewImageLoaded() {
            if (root.toolOpen) {
                root.tool = "";
                toolLoader.sourceComponent = null;
            }
        }
    }

    // ---- the list of tools ---------------------------------------------------
    Column {
        id: home
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8
        enabled: !root.toolOpen

        Label {
            text: qsTr("Editar")
            color: themeManager.textPrimary
            font.bold: true
            font.pixelSize: 15
            width: parent.width
            bottomPadding: 6
        }

        Repeater {
            model: root.tools
            delegate: Rectangle {
                id: entry
                required property var modelData
                width: home.width
                height: 62
                radius: themeManager.radiusSmall
                color: hover.hovered
                       ? Qt.tint(themeManager.surfaceElevated, Qt.rgba(themeManager.accent.r, themeManager.accent.g, themeManager.accent.b, 0.22))
                       : themeManager.surfaceElevated
                border.width: 1
                border.color: hover.hovered ? themeManager.accent : themeManager.border
                Behavior on color { ColorAnimation { duration: themeManager.animFast } }

                AppIcon {
                    id: entryIcon
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    name: entry.modelData.icon
                    size: 26
                    color: hover.hovered ? themeManager.accent : themeManager.textPrimary
                }
                Column {
                    anchors.left: entryIcon.right
                    anchors.leftMargin: 14
                    anchors.right: chevron.left
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Label { width: parent.width; text: entry.modelData.name; color: themeManager.textPrimary; font.bold: true; elide: Text.ElideRight }
                    Label { width: parent.width; text: entry.modelData.hint; color: themeManager.textSecondary; font.pixelSize: 11; elide: Text.ElideRight }
                }
                AppIcon {
                    id: chevron
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    name: "next"
                    size: 16
                    color: themeManager.textSecondary
                }

                HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: root.openTool(entry.modelData.id) }
            }
        }
    }

    // ---- the open tool's page, sliding in -------------------------
    Item {
        id: toolPage
        width: root.width
        height: root.height
        // Opening and closing is just where the page sits: it comes in from the right edge, as
        // a page pushed on top of the list does, and leaves the same way.
        //
        // `shown` (0 = away, 1 = in place) is what animates, not x itself: the panel is created
        // while its width is still 0 and then given its real width, and animating x through that
        // made the page flash past every time edit mode was opened.
        property real shown: root.toolOpen ? 1 : 0
        x: (1 - shown) * root.width
        visible: shown > 0
        clip: true
        z: 2

        Behavior on shown {
            NumberAnimation {
                id: slide
                duration: themeManager.animMedium
                easing.type: themeManager.easingCurve
                // Once it is out of sight, the tool itself is thrown away.
                onRunningChanged: if (!running && !root.toolOpen) toolLoader.sourceComponent = null
            }
        }

        // The page paints over the list - and must also shut it out: without this the list's
        // entries underneath still saw the mouse through the empty parts of the page (showing
        // a pointing hand, even answering clicks) where nothing on screen was clickable.
        Rectangle { anchors.fill: parent; color: themeManager.surface }
        MouseArea { anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.AllButtons; onWheel: function (wheel) { wheel.accepted = true } }

        Column {
            id: pageHeader
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 12
            spacing: 2

            Row {
                spacing: 10
                AppIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: root.toolInfo(root.shownTool).icon
                    size: 22
                    color: themeManager.accent
                }
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.toolInfo(root.shownTool).name
                    color: themeManager.textPrimary
                    font.bold: true
                    font.pixelSize: 15
                }
            }
            Label {
                width: parent.width
                text: qsTr("Aplicá o cancelá para volver")
                color: themeManager.textSecondary
                font.pixelSize: 11
            }
        }

        Loader {
            id: toolLoader
            anchors.top: pageHeader.bottom
            anchors.topMargin: 12
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: pageFooter.top
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.bottomMargin: 10
        }

        Column {
            id: pageFooter
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 12
            spacing: 10

            Rectangle { width: parent.width; height: 1; color: themeManager.border }

            Row {
                spacing: 8
                width: parent.width
                enabled: !(root.canvas && root.canvas.transforming)
                readonly property real cell: (width - spacing) / 2
                AppButton {
                    text: qsTr("Cancelar")
                    iconName: "cancel"
                    width: parent.cell
                    onClicked: root.closeTool(false)
                }
                AppButton {
                    text: qsTr("Aplicar")
                    iconName: "check"
                    width: parent.cell
                    enabled: toolLoader.item ? toolLoader.item.canApply : false
                    onClicked: root.closeTool(true)
                }
            }
        }
    }

    Component { id: cropComponent; CropTool { canvas: root.canvas } }
    Component { id: resizeComponent; ResizeTool { canvas: root.canvas } }
    Component { id: adjustComponent; AdjustTool { canvas: root.canvas } }
    Component { id: filterComponent; FilterTool { canvas: root.canvas } }
    Component { id: effectsComponent; EffectsTool { canvas: root.canvas } }
    Component { id: lensComponent; LensTool { canvas: root.canvas } }

    // Exposed so FloatingToolbar's edit-mode button row can trigger the same
    // save-as flow without duplicating the dialog.
    function requestSaveAs() {
        if (appController.isSaving)
            return; // one write at a time (the backend refuses a second one too)
        saveAsDialog.currentFile = appController.suggestedSaveUrl;
        saveAsDialog.open();
    }

    FileDialog {
        id: saveAsDialog
        title: qsTr("Guardar como")
        fileMode: FileDialog.SaveFile
        nameFilters: [
            qsTr("PNG (*.png)"),
            qsTr("JPEG (*.jpg *.jpeg)"),
            qsTr("BMP (*.bmp)"),
            qsTr("TIFF (*.tif *.tiff)"),
            qsTr("WebP (*.webp)")
        ]
        onAccepted: appController.saveEditedAs(selectedFile)
    }
}
