import QtQuick
import QtQuick.Controls
import ImageViewerApp

// The window's own title bar (the native frame is switched off in Main.qml). It is also the toolbar: the program's
// icon and name, the name of the open file, the tools (Editar, GIF animado, Collage, Comparador, Más herramientas,
// Configuración) and the minimise / maximise / close buttons, all drawn in the program's theme.
//
// Dragging the bar moves the window and a double click maximises it; both go through the system, so snapping and
// the like keep working. Whatever is not a button is the drag area, and it is never squeezed below `minDrag`:
//  - the file name takes at most half of the bar, and gives way to the buttons below that;
//  - as the window narrows the optional tools (GIF animado, Collage, Comparador) move, last first, into the
//    "Más herramientas" menu. Editar and Configuración always stay in the bar.
AppPanelBackground {
    id: root
    border.width: 0
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: themeManager.border }

    required property var window
    // Opens the dialogs the buttons ask for (Toolbar.qml, which owns them).
    property var toolbar: null
    // Owned by Main.qml, like the state the old toolbar kept.
    property bool editMode: false
    // A tool is open in the edit panel: the file (and edit mode) cannot be left meanwhile.
    property bool locked: false
    property bool multiMode: false
    // A creation studio ("gif" or "collage") is open: they take over the window, so the rest waits.
    property string studio: ""
    signal editModeRequested(bool on)
    signal multiModeRequested(bool on)
    signal studioRequested(string name)

    readonly property bool maximized: window.visibility === Window.Maximized

    function toggleMaximize() { maximized ? window.showNormal() : window.showMaximized() }

    // ---- layout ----------------------------------------------------------------------------------------------
    readonly property int minDrag: 96      // the bar always keeps this much to grab and drag
    readonly property int btnW: 32         // an icon-only tool button
    readonly property int gap: 8
    readonly property int sidePad: 10
    readonly property int nameMinW: 120    // the file name is not squeezed below this (unless it is shorter)
    readonly property bool showTitle: width >= 760
    readonly property real fixedActionsW: 3 * btnW + 2 * gap // Editar, Más herramientas, Configuración
    readonly property real leftBaseW: sidePad + 16 + gap + (showTitle ? titleLabel.implicitWidth + gap : 0)
    readonly property real nameNatural: appController.currentFileName.length > 0 ? nameTwin.implicitWidth : 0
    // What is left, once the caption buttons, the fixed tools, the left block, a minimum file name and the drag
    // area are taken, decides how many of the optional tools fit.
    readonly property real spareForOptional: width - buttons.width - gap - fixedActionsW - leftBaseW
                                             - Math.min(nameNatural, nameMinW) - gap - minDrag
    readonly property int optionalShown: Math.max(0, Math.min(3, Math.floor(spareForOptional / (btnW + gap))))
    readonly property real actionsW: fixedActionsW + optionalShown * (btnW + gap)
    // The name: its own width, but never more than half the bar, nor what would squeeze the drag area.
    readonly property real nameMaxW: Math.max(0, Math.min(width / 2,
                                     width - buttons.width - gap - actionsW - leftBaseW - gap - minDrag))

    // The drag area is everything but the buttons: the name, the icon and the empty space between.
    Item {
        id: dragArea
        objectName: "titleDrag"
        anchors.fill: parent
        anchors.rightMargin: buttons.width
        DragHandler {
            target: null
            onActiveChanged: if (active) root.window.startSystemMove()
        }
        TapHandler { onDoubleTapped: root.toggleMaximize() }

        Row {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: root.sidePad
            spacing: root.gap
            Image {
                anchors.verticalCenter: parent.verticalCenter
                width: 16; height: 16
                source: "qrc:/qt/qml/ImageViewerApp/resources/icons/icon.png"
                sourceSize: Qt.size(32, 32)
                smooth: true
            }
            Label {
                id: titleLabel
                anchors.verticalCenter: parent.verticalCenter
                visible: root.showTitle
                text: root.window.title
                color: themeManager.textPrimary
            }
            Label {
                id: nameLabel
                anchors.verticalCenter: parent.verticalCenter
                visible: appController.currentFileName.length > 0 && root.nameMaxW > 24
                text: appController.currentFileName
                color: root.showTitle ? themeManager.textSecondary : themeManager.textPrimary
                elide: Text.ElideMiddle
                width: Math.min(root.nameNatural, root.nameMaxW)
            }
        }
        // The name at its natural width, measured apart: an eliding label's own implicitWidth would depend on the
        // width it is given, which depends on this.
        Label { id: nameTwin; visible: false; text: appController.currentFileName }
    }

    // ---- the tools, right-aligned against the caption buttons. They are siblings of the drag area (above it), so
    // a press on a button never starts a window drag.
    Row {
        id: actions
        anchors.right: buttons.left
        anchors.rightMargin: root.gap
        anchors.verticalCenter: parent.verticalCenter
        spacing: root.gap

        component ToolBtn: AppToolButton {
            iconOnly: true
            iconSize: 20
            iconStroke: 1.4
            topPadding: 3
            bottomPadding: 3
        }

        ToolBtn {
            text: qsTr("Editar")
            objectName: "editButton"
            iconName: "edit"
            checkable: true
            // Structural edits/filters bake a single static frame, which
            // doesn't fit a playing animation - see AppController::isAnimated.
            enabled: !appController.isAnimated && !root.locked && !root.multiMode && root.studio === ""
            checked: root.editMode
            onToggled: root.editModeRequested(checked)
        }
        ToolBtn {
            text: qsTr("GIF animado")
            objectName: "gifButton"
            iconName: "film"
            visible: root.optionalShown >= 1
            enabled: !root.locked
            checked: root.studio === "gif"
            onClicked: root.studioRequested(root.studio === "gif" ? "" : "gif")
        }
        ToolBtn {
            text: qsTr("Collage")
            objectName: "collageButton"
            iconName: "collage"
            visible: root.optionalShown >= 2
            enabled: !root.locked
            checked: root.studio === "collage"
            onClicked: root.studioRequested(root.studio === "collage" ? "" : "collage")
        }
        ToolBtn {
            text: qsTr("Comparador (varias imágenes con zoom vinculado)")
            iconName: "multi"
            checkable: true
            visible: root.optionalShown >= 3
            enabled: folderModel.count > 0 && !root.locked && root.studio === ""
            checked: root.multiMode
            onToggled: root.multiModeRequested(checked)
        }
        ToolBtn {
            id: moreButton
            text: qsTr("Más herramientas")
            iconName: "more"
            onClicked: moreMenu.popup(moreButton, moreButton.width - moreMenu.width, moreButton.height + 4)
            AppMenu {
                id: moreMenu
                // The tools that did not fit in the bar: the last one leaves first.
                AppMenuItem {
                    visible: root.optionalShown < 1
                    height: visible ? implicitHeight : 0
                    iconName: "film"
                    text: qsTr("GIF animado")
                    enabled: !root.locked
                    onTriggered: root.studioRequested(root.studio === "gif" ? "" : "gif")
                }
                AppMenuItem {
                    visible: root.optionalShown < 2
                    height: visible ? implicitHeight : 0
                    iconName: "collage"
                    text: qsTr("Collage")
                    enabled: !root.locked
                    onTriggered: root.studioRequested(root.studio === "collage" ? "" : "collage")
                }
                AppMenuItem {
                    visible: root.optionalShown < 3
                    height: visible ? implicitHeight : 0
                    iconName: "multi"
                    text: qsTr("Comparar")
                    enabled: folderModel.count > 0 && !root.locked && root.studio === ""
                    onTriggered: root.multiModeRequested(!root.multiMode)
                }
                AppMenuSeparator {
                    visible: root.optionalShown < 3
                    height: visible ? implicitHeight : 0
                }
                AppMenuItem {
                    iconName: "export-batch"
                    text: qsTr("Exportar por lote…")
                    enabled: folderModel.count > 0 && !root.locked
                    onTriggered: if (root.toolbar) root.toolbar.openBatchExport()
                }
                AppMenuItem {
                    iconName: "rename-batch"
                    text: qsTr("Renombrar por lote…")
                    enabled: folderModel.count > 0 && !root.locked
                    onTriggered: if (root.toolbar) root.toolbar.openBatchRename()
                }
            }
        }
        ToolBtn {
            text: qsTr("Configuración")
            objectName: "settingsButton"
            iconName: "settings"
            onClicked: if (root.toolbar) root.toolbar.openSettings()
        }
    }

    Row {
        id: buttons
        anchors.right: parent.right
        height: parent.height

        component CaptionButton: Rectangle {
            id: btn
            property bool danger: false
            property bool nativeHover: false // Windows owns the maximise button's mouse (WindowsFrame.cpp)
            property alias glyph: glyphItem.sourceComponent
            property string label: ""
            signal clicked
            width: 46
            height: parent.height
            readonly property bool over: hover.hovered || nativeHover
            color: over ? (danger ? "#e81123" : Qt.rgba(themeManager.textPrimary.r, themeManager.textPrimary.g, themeManager.textPrimary.b, 0.14)) : "transparent"
            readonly property color ink: danger && over ? "white" : themeManager.textPrimary
            Accessible.role: Accessible.Button
            Accessible.name: label
            Loader { id: glyphItem; anchors.centerIn: parent }
            HoverHandler { id: hover }
            TapHandler { onTapped: btn.clicked() }
            AppToolTip { visible: btn.over; text: btn.label }
        }

        CaptionButton {
            objectName: "minimizeButton"
            label: qsTr("Minimizar")
            onClicked: root.window.showMinimized()
            glyph: Component { Rectangle { width: 10; height: 1; color: parent.parent.ink } }
        }
        CaptionButton {
            objectName: "maximizeButton"
            label: root.maximized ? qsTr("Restaurar") : qsTr("Maximizar")
            onClicked: root.toggleMaximize()
            glyph: Component {
                Item {
                    width: 10; height: 10
                    Rectangle { // restored: a second square behind
                        visible: root.maximized
                        x: 2; y: 0; width: 8; height: 8; color: "transparent"; border.color: parent.parent.parent.ink
                    }
                    Rectangle {
                        x: 0; y: root.maximized ? 2 : 0; width: root.maximized ? 8 : 10; height: root.maximized ? 8 : 10
                        color: root.maximized ? themeManager.surface : "transparent"
                        border.color: parent.parent.parent.ink
                    }
                }
            }
        }
        CaptionButton {
            objectName: "closeButton"
            danger: true
            label: qsTr("Cerrar")
            onClicked: root.window.close()
            glyph: Component {
                Item {
                    width: 10; height: 10
                    Rectangle { anchors.centerIn: parent; width: 14; height: 1; rotation: 45; color: parent.parent.parent.ink }
                    Rectangle { anchors.centerIn: parent; width: 14; height: 1; rotation: -45; color: parent.parent.parent.ink }
                }
            }
        }
    }
}
