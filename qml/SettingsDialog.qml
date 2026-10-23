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
    width: Math.min(860, (Overlay.overlay ? Overlay.overlay.width : 860) - 40)
    height: Math.min(620, (Overlay.overlay ? Overlay.overlay.height : 620) - 40)

    property int page: 0
    // Each page starts at its top: the previous page may have been scrolled further than this one is tall.
    onPageChanged: scroller.contentY = 0
    readonly property var pages: [
        { icon: "theme", name: qsTr("Apariencia"), hint: qsTr("Cómo se ve el programa.") },
        { icon: "save", name: qsTr("Guardado"), hint: qsTr("Qué pasa al guardar una imagen editada.") },
        { icon: "folder-batch", name: qsTr("Procesos por lote"), hint: qsTr("Valores para exportar y renombrar carpetas enteras.") },
        { icon: "info", name: qsTr("Acerca de"), hint: qsTr("El programa, su autor y las licencias.") }
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
                        readonly property bool classic: themeManager.skin === "win98"
                        width: parent.width
                        height: 40
                        radius: themeManager.radiusSmall
                        // the chosen page: a soft accent tint with a bar on its edge (the classic theme keeps its solid block)
                        color: current ? (classic ? themeManager.accent : Qt.rgba(themeManager.accent.r, themeManager.accent.g, themeManager.accent.b, 0.16))
                                       : (hover.hovered ? themeManager.controlFace : "transparent")

                        Rectangle {
                            visible: item.current && !item.classic
                            x: 0; width: 3; height: 18; radius: 1.5
                            anchors.verticalCenter: parent.verticalCenter
                            color: themeManager.accent
                        }

                        Row {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            spacing: 10
                            AppIcon {
                                anchors.verticalCenter: parent.verticalCenter
                                name: item.modelData.icon
                                size: 18
                                color: item.current ? (item.classic ? themeManager.accentText : themeManager.accent) : themeManager.textSecondary
                            }
                            Label {
                                anchors.verticalCenter: parent.verticalCenter
                                text: item.modelData.name
                                color: item.current && item.classic ? themeManager.accentText : themeManager.textPrimary
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

            Rectangle { // under the page header
                x: 28; y: 78; width: parent.width - 28 - 20; height: 1
                color: themeManager.border; opacity: 0.6
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
                anchors.topMargin: 92
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
                AppWheelScroll { view: scroller; step: 100 } // the same speed as the thumbnail bar

                Item {
                    id: pageColumn
                    width: scroller.width - 12
                    implicitHeight: root.page === 0 ? appearance.implicitHeight + languageCard.implicitHeight + zoomCard.implicitHeight + pixelCard.implicitHeight + stripCard.implicitHeight + windowCard.implicitHeight + festiveCard.implicitHeight + 96
                                       : root.page === 1 ? saving.implicitHeight
                                       : root.page === 2 ? batch.implicitHeight
                                       : (about.item ? about.item.implicitHeight : 0)

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
                                        text: qsTr(card.modelData.name)
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

                    // -- Apariencia, 2nd card: Idioma -------------------------------------------
                    Card {
                        id: languageCard
                        width: parent.width
                        visible: root.page === 0
                        y: appearance.height + 16
                        heading: qsTr("Idioma")
                        SettingRow {
                            label: qsTr("Idioma de la interfaz")
                            hint: qsTr("Se aplica al instante, sin reiniciar. «Según el sistema» usa el idioma del sistema.")
                            AppComboBox {
                                id: languageBox
                                width: 190
                                model: translations.languages.map(function (l) { return l.code === "system" ? qsTr("Según el sistema") : l.name; })
                                function sync() {
                                    currentIndex = Math.max(0, translations.languages.findIndex(function (l) { return l.code === appSettings.language; }));
                                }
                                Component.onCompleted: sync()
                                onActivated: function (i) { appSettings.language = translations.languages[i].code; }
                                Connections { target: appSettings; function onLanguageChanged() { languageBox.sync(); } }
                            }
                        }
                    }

                    // -- Apariencia, 2nd card: Modo pixel -------------------------------------
                    // The switch itself keeps pictures sharp when zoomed in; the extras below it
                    // are separate switches that only count while the mode is on.
                    Card {
                        id: zoomCard
                        width: parent.width
                        visible: root.page === 0
                        y: appearance.height + languageCard.height + 32
                        heading: qsTr("Zoom")
                        SettingRow {
                            label: qsTr("Tipo de zoom con la rueda")
                            hint: qsTr("«Escalonado» salta de a un paso por vez. «Suavizado» se desliza hacia el nuevo tamaño, con una sensación más fluida.")
                            AppComboBox {
                                width: 170
                                model: [qsTr("Escalonado"), qsTr("Suavizado")]
                                currentIndex: appSettings.smoothZoom ? 1 : 0
                                onActivated: function (i) { appSettings.smoothZoom = (i === 1); }
                            }
                        }
                    }

                    Card {
                        id: pixelCard
                        width: parent.width
                        visible: root.page === 0
                        y: appearance.height + languageCard.height + zoomCard.height + 48
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
                        y: appearance.height + languageCard.height + zoomCard.height + pixelCard.height + 64
                        heading: qsTr("Barra de miniaturas")
                        SettingRow {
                            label: qsTr("Cuándo se muestra")
                            hint: qsTr("«Manual» la abres y cierras tú. «Automática» se cierra al editar y en ventanas estrechas. La pestaña del borde siempre puede invertir la decisión.")
                            AppComboBox {
                                id: stripModeCombo
                                width: 170
                                readonly property var modes: ["manual", "open", "closed", "auto"]
                                model: [qsTr("Manual"), qsTr("Siempre abierta"), qsTr("Siempre cerrada"), qsTr("Automática")]
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

                    // -- Apariencia, 4th card: one window or several -----------------------------
                    Card {
                        id: windowCard
                        objectName: "windowCard"
                        width: parent.width
                        visible: root.page === 0
                        y: appearance.height + languageCard.height + zoomCard.height + pixelCard.height + stripCard.height + 80
                        heading: qsTr("Ventanas")
                        SettingRow {
                            label: qsTr("Una sola ventana")
                            hint: qsTr("Al abrir una imagen desde el Explorador se usa la ventana que ya está abierta, en vez de abrir otra. Se aplica la próxima vez que abras el programa.")
                            AppSwitch {
                                checked: appSettings.singleInstance
                                onToggled: appSettings.singleInstance = checked
                            }
                        }
                        Divider {}
                        SettingRow {
                            visible: trayIcon.available
                            label: qsTr("Seguir abierto en la bandeja")
                            hint: qsTr("Al cerrar la ventana el programa se queda cargado junto al reloj, sin ventana, y se abre al instante. Ocupa algo de memoria mientras tanto. Para salir del todo, usa «Salir» en el menú de su icono.")
                            AppSwitch {
                                checked: appSettings.keepInTray
                                onToggled: appSettings.keepInTray = checked
                            }
                        }
                        Divider { visible: trayIcon.available }
                        SettingRow {
                            visible: trayIcon.available
                            label: qsTr("Iniciar con Windows")
                            hint: qsTr("Abre Project Foxy al iniciar sesión.")
                            AppSwitch {
                                checked: appSettings.startWithWindows
                                onToggled: appSettings.startWithWindows = checked
                            }
                        }
                        Divider { visible: trayIcon.available && appSettings.startWithWindows }
                        SettingRow {
                            visible: trayIcon.available && appSettings.startWithWindows
                            label: qsTr("Iniciar minimizado en la bandeja")
                            hint: qsTr("Arranca sin ventana, solo con el icono junto al reloj, y se abre al instante cuando lo necesites. Si está apagado, al iniciar sesión se abre la ventana. Necesita «Seguir abierto en la bandeja».")
                            AppSwitch {
                                checked: appSettings.startMinimized
                                onToggled: {
                                    appSettings.startMinimized = checked
                                    if (checked)
                                        appSettings.keepInTray = true
                                }
                            }
                        }
                        Divider { visible: trayIcon.available }
                        SettingRow {
                            label: qsTr("Sincronizar con la pantalla (V-Sync)")
                            hint: qsTr("Evita cortes en la imagen. Si la ventana se traba al cambiarle el tamaño (pasa con algunas tarjetas AMD), prueba a apagarla. Se aplica la próxima vez que abras el programa.")
                            AppSwitch {
                                checked: appSettings.verticalSync
                                onToggled: appSettings.verticalSync = checked
                            }
                        }
                    }

                    // -- Apariencia, 5th card: the holiday decorations ---------------------------
                    Card {
                        id: festiveCard
                        objectName: "festiveCard"
                        width: parent.width
                        visible: root.page === 0
                        y: appearance.height + languageCard.height + zoomCard.height + pixelCard.height + stripCard.height + windowCard.height + 96
                        heading: qsTr("Adornos festivos")
                        SettingRow {
                            label: qsTr("Adornos en fechas señaladas")
                            hint: qsTr("Pone adornos discretos en la ventana en Halloween, Día de los Muertos, Navidad, Año Nuevo, Noche de Reyes, San Valentín, San Patricio y Pascua. Solo en los temas modernos.")
                            AppSwitch {
                                objectName: "festiveSwitch"
                                checked: appSettings.festive
                                onToggled: appSettings.festive = checked
                            }
                        }
                        Divider {}
                        SettingRow {
                            label: qsTr("Pieza en la barra de miniaturas")
                            hint: qsTr("Muestra el dibujo grande de la esquina inferior de la barra de miniaturas. Apágalo si te estorba; la guirnalda se queda.")
                            AppSwitch {
                                checked: appSettings.festiveCorner
                                onToggled: appSettings.festiveCorner = checked
                            }
                        }
                        Divider {}
                        SettingRow {
                            label: qsTr("Fiestas con adornos")
                            hint: qsTr("Elige para cuáles quieres ver adornos.")
                        }
                        Repeater {
                            model: [
                                { key: "halloween", name: qsTr("Halloween") }, { key: "muertos", name: qsTr("Día de los Muertos") },
                                { key: "navidad", name: qsTr("Navidad") }, { key: "nuevoano", name: qsTr("Año Nuevo") },
                                { key: "reyes", name: qsTr("Noche de Reyes") }, { key: "sanvalentin", name: qsTr("San Valentín") },
                                { key: "sanpatricio", name: qsTr("San Patricio") }, { key: "pascua", name: qsTr("Pascua") }
                            ]
                            delegate: SettingRow {
                                required property var modelData
                                label: modelData.name
                                enabled: appSettings.festive
                                AppSwitch {
                                    checked: appSettings.festiveOff.indexOf(modelData.key) < 0
                                    onToggled: appSettings.setFestiveOn(modelData.key, checked)
                                }
                            }
                        }
                        Divider {}
                        SettingRow {
                            label: qsTr("Ver los adornos de")
                            hint: qsTr("Para ver cómo quedan cualquier día del año. Al reiniciar vuelve a «Según la fecha».")
                            AppComboBox {
                                id: festivePreviewBox
                                width: 190
                                readonly property var keys: ["", "halloween", "muertos", "navidad", "nuevoano", "reyes", "sanvalentin", "sanpatricio", "pascua"]
                                model: [qsTr("Según la fecha"), qsTr("Halloween"), qsTr("Día de los Muertos"), qsTr("Navidad"), qsTr("Año Nuevo"),
                                        qsTr("Noche de Reyes"), qsTr("San Valentín"), qsTr("San Patricio"), qsTr("Pascua")]
                                function sync() { currentIndex = Math.max(0, keys.indexOf(festive.preview)); }
                                Component.onCompleted: sync()
                                onActivated: function (i) { festive.preview = keys[i]; }
                                Connections { target: festive; function onFestivalChanged() { festivePreviewBox.sync(); } }
                            }
                        }
                    }

                    // -- Guardado ---------------------------------------------------
                    Card {
                        id: saving
                        width: parent.width
                        visible: root.page === 1
                        SettingRow {
                            label: qsTr("Preguntar antes de sobrescribir")
                            hint: qsTr("Pide confirmación al guardar encima del archivo original.")
                            AppSwitch {
                                checked: appSettings.confirmOverwrite
                                onToggled: appSettings.confirmOverwrite = checked
                            }
                        }
                    }

                    // -- Acerca de --------------------------------------------------
                    Loader {
                        id: about
                        width: parent.width
                        active: root.page === 3
                        source: "AboutPage.qml"
                    }

                    // -- Procesos por lote ------------------------------------------
                    Card {
                        id: batch
                        width: parent.width
                        visible: root.page === 2
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
