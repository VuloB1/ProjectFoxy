import QtQuick
import QtQuick.Controls
import ImageViewerApp

// "Configuración" (the gear in the top bar): a two-column page - the categories on
// the left, the chosen one on the right. Everything applies the moment it is
// changed and is persisted by appSettings / themeManager, so there is no
// Guardar/Cancelar, only a way out.
AppDialog {
    id: root
    modal: true
    standardButtons: Dialog.NoButton
    anchors.centerIn: Overlay.overlay
    padding: 0
    width: Math.min(830, (Overlay.overlay ? Overlay.overlay.width : 830) - 40)
    height: Math.min(460, (Overlay.overlay ? Overlay.overlay.height : 460) - 40)

    property int page: 0
    readonly property var pages: [
        { icon: "theme", name: qsTr("Apariencia"), hint: qsTr("Cómo se ve el programa.") },
        { icon: "slideshow", name: qsTr("Presentación"), hint: qsTr("Cómo avanza la presentación de diapositivas.") },
        { icon: "save", name: qsTr("Guardado"), hint: qsTr("Qué pasa al guardar una imagen editada.") },
        { icon: "folder-batch", name: qsTr("Procesos por lote"), hint: qsTr("Valores para exportar y renombrar carpetas enteras.") }
    ]

    // One setting: its name (and a line explaining it) on the left, the control on the right.
    component SettingRow: Item {
        id: row
        property string label
        property string hint
        default property alias control: slot.data
        width: parent ? parent.width : 0
        implicitHeight: Math.max(44, textColumn.implicitHeight + 14)

        Column {
            id: textColumn
            anchors.left: parent.left
            anchors.right: slot.left
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Label { width: parent.width; text: row.label; color: themeManager.textPrimary; elide: Text.ElideRight }
            Label {
                width: parent.width
                visible: row.hint.length > 0
                text: row.hint
                color: themeManager.textSecondary
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }
        Item {
            id: slot
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: childrenRect.width
            height: childrenRect.height
        }
    }

    component Divider: Rectangle { width: parent ? parent.width : 0; height: 1; color: themeManager.border; opacity: 0.6 }

    // A rounded panel that groups the settings of one topic.
    component Card: Rectangle {
        id: card
        default property alias content: inner.data
        property string heading: ""
        width: parent ? parent.width : 0
        implicitHeight: inner.implicitHeight + (heading.length > 0 ? 62 : 24)
        radius: themeManager.radiusMedium === 0 ? 0 : 12
        color: themeManager.surface
        border.color: themeManager.border
        border.width: 1

        Label {
            visible: card.heading.length > 0
            x: 18
            y: 14
            text: card.heading
            color: themeManager.textSecondary
            font.bold: true
            font.pixelSize: 11
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 0.8
        }
        Column {
            id: inner
            x: 18
            y: card.heading.length > 0 ? 40 : 12
            width: card.width - 36
            spacing: 0
        }
    }

    contentItem: Item {
        implicitWidth: root.width
        implicitHeight: root.height

        // ---- left: categories ------------------------------------------------
        Rectangle {
            id: sidebar
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            width: 196
            color: themeManager.surface
            radius: root.cornerRadius
            // square off the corners that touch the content pane
            Rectangle { anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; width: parent.radius; color: parent.color; visible: parent.radius > 0 }

            Label {
                x: 20; y: 20
                text: qsTr("Configuración")
                color: themeManager.textPrimary
                font.bold: true
                font.pixelSize: 16
            }

            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: 64
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 4

                Repeater {
                    model: root.pages
                    delegate: Rectangle {
                        id: item
                        required property int index
                        required property var modelData
                        readonly property bool current: root.page === index
                        width: parent.width
                        height: 38
                        radius: themeManager.radiusSmall
                        color: current ? themeManager.accent : (hover.hovered ? themeManager.controlFace : "transparent")
                        opacity: current ? 1.0 : (hover.hovered ? 0.85 : 1.0)

                        Row {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            spacing: 10
                            AppIcon {
                                anchors.verticalCenter: parent.verticalCenter
                                name: item.modelData.icon
                                size: 18
                                color: item.current ? themeManager.accentText : themeManager.textSecondary
                            }
                            Label {
                                anchors.verticalCenter: parent.verticalCenter
                                text: item.modelData.name
                                color: item.current ? themeManager.accentText : themeManager.textPrimary
                                font.bold: item.current
                            }
                        }
                        HoverHandler { id: hover }
                        TapHandler { onTapped: root.page = item.index }
                    }
                }
            }
        }

        // ---- right: the chosen page ------------------------------------------
        Item {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: sidebar.right
            anchors.right: parent.right

            Label {
                id: pageTitle
                anchors.left: parent.left
                anchors.leftMargin: 28
                y: 20
                text: root.pages[root.page].name
                color: themeManager.textPrimary
                font.bold: true
                font.pixelSize: 18
            }
            Label {
                anchors.left: parent.left
                anchors.leftMargin: 28
                y: 46
                text: root.pages[root.page].hint
                color: themeManager.textSecondary
                font.pixelSize: 12
            }

            AppToolButton {
                anchors.right: parent.right
                anchors.rightMargin: 12
                y: 12
                text: qsTr("Cerrar")
                iconName: "close"
                iconOnly: true
                onClicked: root.close()
            }

            Flickable {
                id: scroller
                anchors.top: parent.top
                anchors.topMargin: 84
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 16
                anchors.left: parent.left
                anchors.leftMargin: 28
                anchors.right: parent.right
                anchors.rightMargin: 20
                contentWidth: width
                contentHeight: pageColumn.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: AppScrollBar {}

                Item {
                    id: pageColumn
                    width: scroller.width - 12
                    implicitHeight: Math.max(appearance.implicitHeight + pixelCard.implicitHeight + stripCard.implicitHeight + 32, slideshow.implicitHeight,
                                             saving.implicitHeight, batch.implicitHeight)

                    // -- Apariencia: one preview card per theme --------------------
                    Card {
                        id: appearance
                        width: parent.width
                        visible: root.page === 0
                        heading: qsTr("Tema")
                        Flow {
                            width: parent.width
                            spacing: 14
                            bottomPadding: 6
                            Repeater {
                                model: themeManager.availableThemes
                                delegate: Item {
                                    id: card
                                    required property var modelData
                                    readonly property bool selected: modelData.id === themeManager.themeId
                                    readonly property bool classic: modelData.skin === "win98"
                                    width: 118
                                    height: 112

                                    // The preview itself. Its border is a separate frame drawn ON TOP of
                                    // everything inside (the parts are inset from it), so the chosen
                                    // theme's accent frame is never covered by the mock-up's own bars.
                                    Rectangle {
                                        id: preview
                                        width: parent.width
                                        height: 88
                                        radius: card.classic ? 0 : 9
                                        color: card.modelData.background

                                        // top bar
                                        Rectangle {
                                            x: 2; y: 2; width: parent.width - 4; height: 16
                                            topLeftRadius: card.classic ? 0 : 7
                                            topRightRadius: card.classic ? 0 : 7
                                            color: card.modelData.surfaceElevated
                                            Rectangle { x: 8; y: 5; width: 28; height: 6; radius: card.classic ? 0 : 3; color: card.modelData.textSecondary; opacity: 0.8 }
                                            Rectangle { anchors.right: parent.right; anchors.rightMargin: 10; y: 4; width: 8; height: 8; radius: card.classic ? 0 : 4; color: card.modelData.accent }
                                        }
                                        // side strip
                                        Rectangle {
                                            x: 2; y: 18; width: 22; height: parent.height - 20
                                            bottomLeftRadius: card.classic ? 0 : 7
                                            color: card.modelData.surface
                                        }
                                        // the "photo"
                                        Rectangle {
                                            x: 32; y: 26; width: parent.width - 44; height: 38
                                            radius: card.classic ? 0 : 4
                                            gradient: Gradient {
                                                orientation: Gradient.Horizontal
                                                GradientStop { position: 0; color: card.modelData.accent }
                                                GradientStop { position: 1; color: card.modelData.surface }
                                            }
                                            border.width: 1
                                            border.color: card.modelData.border
                                        }
                                        // the floating bar
                                        Rectangle {
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            anchors.horizontalCenterOffset: 10
                                            y: parent.height - 18
                                            width: 56; height: 9
                                            radius: card.classic ? 0 : 4
                                            color: card.modelData.surfaceElevated
                                            border.width: 1
                                            border.color: card.modelData.border
                                        }

                                        // the frame: accent and thicker when chosen
                                        Rectangle {
                                            anchors.fill: parent
                                            radius: parent.radius
                                            color: "transparent"
                                            border.width: card.selected ? 2 : 1
                                            border.color: card.selected ? themeManager.accent
                                                          : cardHover.hovered ? themeManager.textSecondary : themeManager.border
                                        }
                                    }

                                    // tick on the chosen one
                                    Rectangle {
                                        visible: card.selected
                                        anchors.right: preview.right
                                        anchors.top: preview.top
                                        anchors.margins: 6
                                        width: 18; height: 18; radius: 9
                                        color: themeManager.accent
                                        AppIcon { anchors.centerIn: parent; name: "check"; size: 12; color: themeManager.accentText; strokeWidth: 2.4 }
                                    }

                                    Label {
                                        anchors.bottom: parent.bottom
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: card.modelData.name
                                        color: themeManager.textPrimary
                                        font.bold: card.selected
                                    }

                                    HoverHandler { id: cardHover; cursorShape: Qt.PointingHandCursor }
                                    TapHandler { onTapped: themeManager.setTheme(card.modelData.id) }
                                    opacity: cardHover.hovered || card.selected ? 1.0 : 0.88
                                }
                            }
                        }
                    }

                    // -- Apariencia, 2nd card: Modo pixel -------------------------------------
                    // The switch itself keeps pictures sharp when zoomed in; the extras below it
                    // are separate switches that only count while the mode is on.
                    Card {
                        id: pixelCard
                        width: parent.width
                        visible: root.page === 0
                        y: appearance.height + 16
                        heading: qsTr("Modo pixel")
                        SettingRow {
                            label: qsTr("Modo pixel")
                            hint: qsTr("Para sprites, íconos y dibujos hechos de píxeles: se ven nítidos al ampliar, sin borrosidad, y el zoom llega a 64×.")
                            AppSwitch {
                                checked: appSettings.pixelMode
                                onToggled: appSettings.pixelMode = checked
                            }
                        }
                        Divider {}
                        Label {
                            text: qsTr("Extras del modo pixel")
                            topPadding: 10
                            bottomPadding: 2
                            color: themeManager.textSecondary
                            font.pixelSize: 11
                            font.bold: true
                        }
                        // the extras: dimmed and untouchable while the mode is off
                        Column {
                            id: pixelExtras
                            width: parent.width
                            enabled: appSettings.pixelMode
                            opacity: enabled ? 1.0 : 0.5
                            Behavior on opacity { NumberAnimation { duration: themeManager.animFast } }

                            SettingRow {
                                label: qsTr("Zoom en números enteros")
                                hint: qsTr("1×, 2×, 3×, 4×… Las imágenes pequeñas se abren ampliadas.")
                                AppSwitch {
                                    checked: appSettings.pixelIntegerZoom
                                    onToggled: appSettings.pixelIntegerZoom = checked
                                }
                            }
                            Divider {}
                            SettingRow {
                                label: qsTr("Fondo de ajedrez")
                                hint: qsTr("Deja ver las zonas transparentes.")
                                AppSwitch {
                                    checked: appSettings.pixelCheckerboard
                                    onToggled: appSettings.pixelCheckerboard = checked
                                }
                            }
                            Divider {}
                            SettingRow {
                                label: qsTr("Cuadrícula entre píxeles")
                                hint: qsTr("Aparece al ampliar desde 8×.")
                                AppSwitch {
                                    checked: appSettings.pixelGrid
                                    onToggled: appSettings.pixelGrid = checked
                                }
                            }
                            Divider {}
                            SettingRow {
                                label: qsTr("Lectura del píxel")
                                hint: qsTr("Abajo a la izquierda: posición y color del píxel bajo el cursor.")
                                AppSwitch {
                                    checked: appSettings.pixelReadout
                                    onToggled: appSettings.pixelReadout = checked
                                }
                            }
                            Divider {}
                            SettingRow {
                                label: qsTr("Editar sin suavizar")
                                hint: qsTr("«Tamaño» y «Enderezar» copian píxeles (vecino más cercano), así no aparecen colores nuevos.")
                                AppSwitch {
                                    checked: appSettings.pixelSharpEdit
                                    onToggled: appSettings.pixelSharpEdit = checked
                                }
                            }
                        }
                    }

                    // -- Apariencia, 3rd card: the thumbnail bar on the left -------------------
                    Card {
                        id: stripCard
                        width: parent.width
                        visible: root.page === 0
                        y: appearance.height + pixelCard.height + 32
                        heading: qsTr("Barra de miniaturas")
                        SettingRow {
                            label: qsTr("Cuándo se muestra")
                            hint: qsTr("«Con pestaña» la abres y cierras tú. «Automática» se cierra al editar, en la presentación y en ventanas estrechas.")
                            AppComboBox {
                                id: stripModeCombo
                                width: 170
                                readonly property var modes: ["manual", "open", "closed", "auto"]
                                model: [qsTr("Con pestaña"), qsTr("Siempre abierta"), qsTr("Siempre cerrada"), qsTr("Automática")]
                                currentIndex: Math.max(0, modes.indexOf(appSettings.stripMode))
                                onActivated: function (i) { appSettings.stripMode = modes[i]; }
                            }
                        }
                        Divider {}
                        SettingRow {
                            label: qsTr("Tamaño de las miniaturas")
                            hint: qsTr("También puedes arrastrar el borde de la barra con el mouse.")
                            ScrubNumberField {
                                from: 48; to: 200; decimals: 0; dragStep: 1; suffix: qsTr(" px")
                                value: appSettings.stripThumbSize
                                onValueEdited: appSettings.stripThumbSize = Math.round(newValue)
                            }
                        }
                        Divider {}
                        SettingRow {
                            label: qsTr("Columnas")
                            hint: qsTr("Útil con muchas imágenes en la carpeta.")
                            ScrubNumberField {
                                from: 1; to: 4; decimals: 0; dragStep: 1
                                value: appSettings.stripColumns
                                onValueEdited: appSettings.stripColumns = Math.round(newValue)
                            }
                        }
                    }

                    // -- Presentación -----------------------------------------------
                    Card {
                        id: slideshow
                        width: parent.width
                        visible: root.page === 1
                        SettingRow {
                            label: qsTr("Duración por foto")
                            hint: qsTr("Cuánto se muestra cada imagen antes de pasar a la siguiente.")
                            ScrubNumberField {
                                from: 0.5; to: 30; decimals: 1; dragStep: 0.1; suffix: qsTr(" s")
                                value: appSettings.slideshowIntervalMs / 1000.0
                                onValueEdited: appSettings.slideshowIntervalMs = Math.round(newValue * 1000)
                            }
                        }
                        Divider {}
                        SettingRow {
                            label: qsTr("Orden aleatorio")
                            hint: qsTr("Mezcla las imágenes de la carpeta.")
                            AppSwitch {
                                checked: appSettings.slideshowRandom
                                onToggled: appSettings.slideshowRandom = checked
                            }
                        }
                        Divider {}
                        SettingRow {
                            label: qsTr("Repetir en bucle")
                            hint: qsTr("Al llegar a la última vuelve a empezar.")
                            AppSwitch {
                                checked: appSettings.slideshowLoop
                                onToggled: appSettings.slideshowLoop = checked
                            }
                        }
                    }

                    // -- Guardado ---------------------------------------------------
                    Card {
                        id: saving
                        width: parent.width
                        visible: root.page === 2
                        SettingRow {
                            label: qsTr("Preguntar antes de sobrescribir")
                            hint: qsTr("Pide confirmación al guardar encima del archivo original.")
                            AppSwitch {
                                checked: appSettings.confirmOverwrite
                                onToggled: appSettings.confirmOverwrite = checked
                            }
                        }
                    }

                    // -- Procesos por lote ------------------------------------------
                    Card {
                        id: batch
                        width: parent.width
                        visible: root.page === 3
                        SettingRow {
                            label: qsTr("Calidad de exportación")
                            hint: qsTr("Para JPG y WebP al exportar por lote.")
                            ScrubNumberField {
                                from: 1; to: 100; decimals: 0; dragStep: 1; suffix: "%"
                                value: appSettings.batchExportQuality
                                onValueEdited: appSettings.batchExportQuality = newValue
                            }
                        }
                        Divider {}
                        SettingRow {
                            label: qsTr("Dígitos de numeración")
                            hint: qsTr("Al renombrar por lote: 3 da imagen_001, imagen_002…")
                            ScrubNumberField {
                                from: 1; to: 6; decimals: 0; dragStep: 1
                                value: appSettings.batchRenamePadding
                                onValueEdited: appSettings.batchRenamePadding = newValue
                            }
                        }
                    }
                }
            }
        }
    }
}
