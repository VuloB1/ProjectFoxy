import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import ImageViewerApp

// Crear > GIF animado: the pictures along the bottom, the animation playing above them, the options on
// the right. Everything it does goes through `animStudio` (src/app/AnimStudio.h); this page only shows it.
Item {
    id: root

    signal closeRequested()

    readonly property var opts: animStudio.options
    readonly property int count: animStudio.count
    property int currentIndex: -1
    // The animation being shown: which frame of the finished animation, and whether it moves.
    property int playIndex: 0
    property bool playing: true
    property bool resultOk: true
    property string resultText: ""
    property string resultPath: ""

    readonly property var formatNames: ["GIF", "APNG", "WebP"]
    readonly property var formatSuffixes: ["gif", "png", "webp"]

    function frameUrl(i) { return "image://animprev/" + i + "?r=" + animStudio.revision; }
    function seconds(ms) { return (ms / 1000).toFixed(ms < 10000 ? 1 : 0) + " s"; }
    function sizeText(bytes) {
        if (bytes >= 1048576) return (bytes / 1048576).toFixed(1) + " MB";
        return Math.max(1, Math.round(bytes / 1024)) + " KB";
    }

    Rectangle { anchors.fill: parent; color: themeManager.background }

    // ---------------------------------------------------------------- the preview
    function showFrame(i) {
        const n = animStudio.planCount;
        if (n === 0) {
            frontImage.source = "";
            backImage.source = "";
            return;
        }
        playIndex = ((i % n) + n) % n;
        frontImage.source = frameUrl(playIndex);
        backImage.source = frameUrl((playIndex + 1) % n);
        tick.interval = animStudio.planDelay(playIndex);
        tick.restart();
    }

    Connections {
        target: animStudio
        function onPlanChanged() { root.showFrame(root.playIndex); }
        function onExportFinished(ok, path, bytes, error) {
            root.resultOk = ok;
            root.resultPath = ok ? path : "";
            root.resultText = ok ? qsTr("Guardado: %1 (%2)").arg(path.replace(/^.*[\\/]/, "")).arg(root.sizeText(bytes))
                                 : (error !== "" ? error : qsTr("No se pudo guardar."));
        }
    }
    onCountChanged: if (currentIndex >= count) currentIndex = count - 1
    Connections {
        target: animStudio
        function onRowsInserted() { root.currentIndex = Math.min(root.currentIndex < 0 ? 0 : root.currentIndex, animStudio.rowCount() - 1); }
    }
    Component.onCompleted: showFrame(0)

    Timer {
        id: tick
        repeat: false
        running: false
        onTriggered: {
            if (!root.playing || animStudio.planCount < 2) {
                return;
            }
            if (backImage.status !== Image.Ready) { // the next frame is still being drawn: wait for it
                tick.interval = 25;
                tick.restart();
                return;
            }
            const n = animStudio.planCount;
            const next = (root.playIndex + 1) % n;
            // the frame that was loaded behind comes to the front, and the one after it starts loading
            const loaded = backImage.source;
            frontImage.source = loaded;
            root.playIndex = next;
            backImage.source = root.frameUrl((next + 1) % n);
            tick.interval = animStudio.planDelay(next);
            tick.restart();
        }
    }
    onPlayingChanged: { if (playing) tick.restart(); }

    // ---------------------------------------------------------------- the left side: preview and strip
    Item {
        id: left
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: side.left

        // the top bar
        Row {
            id: topBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.topMargin: 10
            anchors.leftMargin: 12
            spacing: 6
            height: 34
            AppIcon { anchors.verticalCenter: parent.verticalCenter; name: "film"; size: 22; color: themeManager.accent }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Crear GIF animado")
                color: themeManager.textPrimary
                font.bold: true
                font.pixelSize: 15
                rightPadding: 12
            }
            AppButton { text: qsTr("Agregar imágenes…"); onClicked: addDialog.open() }
            AppButton {
                text: qsTr("Toda la carpeta")
                enabled: folderModel.count > 0
                onClicked: animStudio.addPaths(folderModel.filePaths())
                AppToolTip { visible: parent.hovered; text: qsTr("Agrega todas las imágenes de la carpeta que está abierta") }
            }
            AppButton { text: qsTr("Invertir orden"); enabled: root.count > 1; onClicked: animStudio.reverseOrder() }
            AppButton { text: qsTr("Quitar todas"); enabled: root.count > 0; onClicked: { animStudio.clear(); root.currentIndex = -1; } }
        }
        AppToolButton {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: 10
            anchors.rightMargin: 12
            text: qsTr("Cerrar el estudio")
            iconName: "close"
            iconOnly: true
            onClicked: root.closeRequested()
        }

        // the preview
        Item {
            id: stage
            anchors.top: topBar.bottom
            anchors.topMargin: 10
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: controls.top
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            clip: true

            // the picture's own rectangle, so the checkerboard (for transparency) is only behind it
            readonly property real aspect: root.opts.width / Math.max(1, root.opts.height)
            readonly property real boxW: Math.min(width, height * aspect)
            readonly property real boxH: boxW / aspect

            Item {
                id: box
                anchors.centerIn: parent
                width: stage.boxW
                height: stage.boxH
                visible: root.count > 0

                Canvas {
                    anchors.fill: parent
                    visible: root.opts.transparent
                    onPaint: {
                        const ctx = getContext("2d");
                        const s = 12;
                        for (let y = 0; y < height; y += s)
                            for (let x = 0; x < width; x += s) {
                                ctx.fillStyle = ((x / s + y / s) % 2 === 0) ? "#cfcfcf" : "#9a9a9a";
                                ctx.fillRect(x, y, s, s);
                            }
                    }
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                    onVisibleChanged: requestPaint()
                }
                Rectangle { anchors.fill: parent; color: "transparent"; border.width: 1; border.color: themeManager.border; z: 3 }
                Image {
                    id: frontImage
                    anchors.fill: parent
                    asynchronous: true
                    cache: true
                    smooth: true
                    fillMode: Image.Stretch
                }
                // loads the frame that comes next, hidden, so it is ready when it is its turn
                Image {
                    id: backImage
                    anchors.fill: parent
                    asynchronous: true
                    cache: true
                    visible: false
                }
            }

            // nothing yet
            Column {
                anchors.centerIn: parent
                spacing: 12
                visible: root.count === 0
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Agregá las imágenes con las que armar la animación")
                    color: themeManager.textPrimary
                    font.pixelSize: 15
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Podés soltarlas acá, elegirlas o usar todas las de la carpeta. Un GIF, APNG o WebP animado que agregues se desarma en sus fotogramas.")
                    color: themeManager.textSecondary
                    width: 460
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 8
                    AppButton { text: qsTr("Agregar imágenes…"); primary: true; onClicked: addDialog.open() }
                    AppButton { text: qsTr("Toda la carpeta"); enabled: folderModel.count > 0; onClicked: animStudio.addPaths(folderModel.filePaths()) }
                }
            }
        }

        // play / pause and the position
        Row {
            id: controls
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: strip.top
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            anchors.bottomMargin: 6
            height: 34
            spacing: 8
            visible: root.count > 0
            AppToolButton {
                text: root.playing ? qsTr("Pausa") : qsTr("Reproducir")
                iconName: root.playing ? "pause" : "play"
                iconOnly: true
                enabled: animStudio.planCount > 1
                onClicked: root.playing = !root.playing
            }
            AppSlider {
                id: scrub
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 260
                from: 0
                to: Math.max(1, animStudio.planCount - 1)
                stepSize: 1
                snapMode: Slider.SnapAlways
                value: root.playIndex
                enabled: animStudio.planCount > 1
                onMoved: { root.playing = false; root.showFrame(Math.round(value)); }
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                width: 200
                color: themeManager.textSecondary
                elide: Text.ElideRight
                text: qsTr("%1 fotogramas · %2 · %3×%4").arg(animStudio.planCount).arg(root.seconds(animStudio.totalMs))
                          .arg(root.opts.width).arg(root.opts.height)
            }
        }

        // the pictures
        Rectangle {
            id: strip
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 140
            color: themeManager.surface
            border.width: 0
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: themeManager.border }

            ListView {
                id: list
                anchors.fill: parent
                anchors.margins: 8
                orientation: ListView.Horizontal
                spacing: 8
                clip: true
                model: animStudio
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.horizontal: AppScrollBar {}
                header: Item {
                    width: 96
                    height: list.height
                    Rectangle {
                        width: 88
                        height: 100
                        anchors.verticalCenter: parent.verticalCenter
                        radius: themeManager.radiusSmall
                        color: addHover.hovered ? Qt.rgba(themeManager.accent.r, themeManager.accent.g, themeManager.accent.b, 0.18) : "transparent"
                        border.width: 1
                        border.color: addHover.hovered ? themeManager.accent : themeManager.border
                        AppIcon { anchors.centerIn: parent; name: "plus"; size: 28; color: addHover.hovered ? themeManager.accent : themeManager.textSecondary }
                        HoverHandler { id: addHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: addDialog.open() }
                        AppToolTip { visible: addHover.hovered; text: qsTr("Agregar imágenes") }
                    }
                }
                delegate: Item {
                    id: cell
                    required property int index
                    required property int entryId
                    required property string name
                    required property int holdMs
                    width: 128
                    height: list.height
                    readonly property bool selected: root.currentIndex === index

                    Rectangle {
                        id: card
                        width: parent.width
                        height: 100
                        anchors.top: parent.top
                        anchors.topMargin: 4
                        radius: themeManager.radiusSmall
                        color: themeManager.surfaceElevated
                        border.width: cell.selected ? 2 : 1
                        border.color: cell.selected ? themeManager.accent : themeManager.border
                        clip: true

                        Image {
                            anchors.fill: parent
                            anchors.margins: 2
                            source: "image://animsrc/" + cell.entryId + "?r=0"
                            sourceSize: Qt.size(256, 256)
                            asynchronous: true
                            fillMode: Image.PreserveAspectCrop
                        }
                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.margins: 4
                            width: numberLabel.implicitWidth + 10
                            height: 18
                            radius: 9
                            color: Qt.rgba(0, 0, 0, 0.6)
                            Label { id: numberLabel; anchors.centerIn: parent; text: cell.index + 1; color: "#fff"; font.pixelSize: 11 }
                        }
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 20
                            color: Qt.rgba(0, 0, 0, 0.6)
                            Label { anchors.centerIn: parent; text: root.seconds(cell.holdMs); color: "#fff"; font.pixelSize: 11 }
                        }
                        // the buttons that show up under the mouse
                        Row {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 3
                            spacing: 2
                            visible: cardHover.hovered
                            Repeater {
                                model: [
                                    { icon: "prev", tip: qsTr("Mover a la izquierda"), act: 0, ok: cell.index > 0 },
                                    { icon: "next", tip: qsTr("Mover a la derecha"), act: 1, ok: cell.index < root.count - 1 },
                                    { icon: "copy", tip: qsTr("Duplicar"), act: 2, ok: true },
                                    { icon: "close", tip: qsTr("Quitar"), act: 3, ok: true }
                                ]
                                delegate: Rectangle {
                                    required property var modelData
                                    visible: modelData.ok
                                    width: 20
                                    height: 20
                                    radius: 4
                                    color: miniHover.hovered ? (modelData.act === 3 ? "#d94848" : themeManager.accent) : Qt.rgba(0, 0, 0, 0.65)
                                    AppIcon { anchors.centerIn: parent; name: modelData.icon; size: 12; color: "#fff" }
                                    HoverHandler { id: miniHover; cursorShape: Qt.PointingHandCursor }
                                    TapHandler {
                                        onTapped: {
                                            if (modelData.act === 0) { animStudio.move(cell.index, cell.index - 1); root.currentIndex = cell.index - 1; }
                                            else if (modelData.act === 1) { animStudio.move(cell.index, cell.index + 1); root.currentIndex = cell.index + 1; }
                                            else if (modelData.act === 2) animStudio.duplicateAt(cell.index);
                                            else animStudio.removeAt(cell.index);
                                        }
                                    }
                                    AppToolTip { visible: miniHover.hovered; text: modelData.tip }
                                }
                            }
                        }
                        HoverHandler { id: cardHover }
                        TapHandler {
                            onTapped: {
                                root.currentIndex = cell.index;
                                const at = animStudio.planIndexOf(cell.index);
                                if (at >= 0) {
                                    root.playing = false;
                                    root.showFrame(at);
                                }
                            }
                        }
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: card.bottom
                        anchors.topMargin: 1
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideMiddle
                        text: cell.name
                        color: themeManager.textSecondary
                        font.pixelSize: 10
                    }
                }
            }
        }

        DropArea {
            anchors.fill: parent
            onDropped: function (drop) { if (drop.hasUrls) animStudio.addUrls(drop.urls); }
        }
    }

    // ---------------------------------------------------------------- the right side: options
    Rectangle {
        id: side
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 330
        color: themeManager.surface
        Rectangle { anchors.left: parent.left; width: 1; height: parent.height; color: themeManager.border }

        // a slider with its caption and value
        component OptSlider: Item {
            property string key: ""
            property string label: ""
            property real from: 0
            property real to: 100
            property real step: 1
            property string suffix: ""
            property int decimals: 0
            property real shown: animStudio.options[key]
            width: parent ? parent.width : 0
            implicitHeight: 40
            Label { text: parent.label; color: themeManager.textSecondary }
            Label {
                anchors.right: parent.right
                text: parent.shown.toFixed(parent.decimals) + parent.suffix
                color: themeManager.textPrimary
                font.bold: true
            }
            AppSlider {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                from: parent.from
                to: parent.to
                stepSize: parent.step
                value: parent.shown
                onMoved: animStudio.setOption(parent.key, value)
            }
        }
        component OptCheck: AppCheckBox {
            property string key: ""
            width: parent ? parent.width : 0
            checked: animStudio.options[key] === true
            onToggled: animStudio.setOption(key, checked)
        }

        Flickable {
            id: flick
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: footer.top
            anchors.leftMargin: 14
            anchors.rightMargin: 6
            anchors.topMargin: 10
            contentWidth: width
            contentHeight: col.implicitHeight + 10
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: AppScrollBar { id: bar }
            AppWheelScroll { view: flick }

            Column {
                id: col
                width: flick.width - bar.implicitWidth - 6
                spacing: 8

                // ---- the picture picked in the strip
                AdjustSection {
                    width: parent.width
                    title: qsTr("Duración de los fotogramas")
                    Label {
                        width: parent.width
                        text: root.currentIndex >= 0 ? qsTr("Fotograma %1").arg(root.currentIndex + 1) : qsTr("Elegí un fotograma de la tira para cambiarle la duración.")
                        color: themeManager.textSecondary
                        wrapMode: Text.WordWrap
                        font.pixelSize: 11
                    }
                    Item {
                        width: parent.width
                        implicitHeight: 40
                        visible: root.currentIndex >= 0
                        readonly property int hold: root.count > 0 ? animStudio.holdAt(root.currentIndex) : 1000
                        Label { text: qsTr("Este fotograma"); color: themeManager.textSecondary }
                        Label { anchors.right: parent.right; text: parent.hold + " ms"; color: themeManager.textPrimary; font.bold: true }
                        AppSlider {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            from: 20; to: 5000; stepSize: 10
                            value: parent.hold
                            onMoved: animStudio.setHold(root.currentIndex, Math.round(value))
                        }
                    }
                    Row {
                        width: parent.width
                        spacing: 6
                        AppButton {
                            width: (parent.width - 6) / 2
                            text: qsTr("A todos los fotogramas")
                            enabled: root.currentIndex >= 0
                            onClicked: animStudio.setAllHold(animStudio.holdAt(root.currentIndex))
                        }
                        Item {
                            width: (parent.width - 6) / 2
                            height: 56
                            Label { text: qsTr("Los que agregues"); color: themeManager.textSecondary; font.pixelSize: 11 }
                            AppSpinBox {
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                width: parent.width
                                height: 26
                                from: 20; to: 60000; stepSize: 100
                                editable: true
                                value: root.opts.defaultHold
                                onValueModified: animStudio.setOption("defaultHold", value)
                            }
                        }
                    }
                }

                // ---- size, fit, background
                AdjustSection {
                    width: parent.width
                    title: qsTr("Tamaño")
                    Row {
                        width: parent.width
                        spacing: 6
                        AppSpinBox {
                            width: (parent.width - 44) / 2
                            from: 16; to: 4096; stepSize: 10
                            editable: true
                            value: root.opts.width
                            onValueModified: animStudio.setOption("width", value)
                        }
                        AppToolButton {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 32
                            text: root.opts.lockAspect ? qsTr("Mantener las proporciones: activado") : qsTr("Mantener las proporciones: desactivado")
                            iconName: root.opts.lockAspect ? "link" : "link-off"
                            iconOnly: true
                            onClicked: animStudio.setOption("lockAspect", !root.opts.lockAspect)
                        }
                        AppSpinBox {
                            width: (parent.width - 44) / 2
                            from: 16; to: 4096; stepSize: 10
                            editable: true
                            value: root.opts.height
                            onValueModified: animStudio.setOption("height", value)
                        }
                    }
                    Flow {
                        width: parent.width
                        spacing: 4
                        Repeater {
                            model: [240, 320, 480, 640, 800, 1080]
                            delegate: AppToolButton {
                                required property int modelData
                                text: modelData + " px"
                                enabled: root.count > 0
                                onClicked: animStudio.sizeFromFirst(modelData)
                            }
                        }
                    }
                    Label { text: qsTr("Cómo entra cada imagen"); color: themeManager.textSecondary }
                    Flow {
                        width: parent.width
                        spacing: 4
                        Repeater {
                            model: [qsTr("Entera, con fondo"), qsTr("Llenar y recortar"), qsTr("Estirar")]
                            delegate: AppToolButton {
                                required property int index
                                required property string modelData
                                text: modelData
                                checked: root.opts.fit === index
                                onClicked: animStudio.setOption("fit", index)
                            }
                        }
                    }
                    Row {
                        width: parent.width
                        spacing: 10
                        Label { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Fondo"); color: themeManager.textSecondary }
                        AppColorSwatch {
                            anchors.verticalCenter: parent.verticalCenter
                            enabled: !root.opts.transparent
                            opacity: enabled ? 1 : 0.4
                            value: Qt.rgba(((root.opts.background >> 16) & 255) / 255, ((root.opts.background >> 8) & 255) / 255, (root.opts.background & 255) / 255, 1)
                            onPicked: function (c) {
                                animStudio.setOption("background", (Math.round(c.r * 255) << 16) | (Math.round(c.g * 255) << 8) | Math.round(c.b * 255));
                            }
                        }
                        OptCheck { key: "transparent"; text: qsTr("Transparente"); width: parent.width - 120 }
                    }
                }

                // ---- how it plays
                AdjustSection {
                    width: parent.width
                    title: qsTr("Animación")
                    Label { text: qsTr("Transición entre imágenes"); color: themeManager.textSecondary }
                    AppComboBox {
                        width: parent.width
                        model: [qsTr("Ninguna (corte directo)"), qsTr("Fundido"), qsTr("Deslizar a la izquierda"), qsTr("Deslizar a la derecha"),
                                qsTr("Deslizar hacia arriba"), qsTr("Deslizar hacia abajo"), qsTr("Zoom")]
                        currentIndex: root.opts.transition
                        onActivated: function (i) { animStudio.setOption("transition", i); }
                    }
                    Column {
                        width: parent.width
                        spacing: 6
                        visible: root.opts.transition !== 0
                        OptSlider { key: "transitionMs"; label: qsTr("Duración de la transición"); from: 100; to: 2000; step: 50; suffix: " ms" }
                        OptSlider { key: "transitionSteps"; label: qsTr("Fotogramas por transición"); from: 2; to: 16; step: 1 }
                        OptCheck { key: "transitionOnLoop"; text: qsTr("También de la última a la primera") }
                    }
                    OptCheck { key: "reverse"; text: qsTr("Invertir el orden") }
                    OptCheck { key: "pingPong"; text: qsTr("Ida y vuelta") }
                    OptSlider { key: "speed"; label: qsTr("Velocidad"); from: 0.25; to: 4; step: 0.05; suffix: "×"; decimals: 2 }
                    Label { text: qsTr("Repeticiones"); color: themeManager.textSecondary }
                    AppComboBox {
                        width: parent.width
                        readonly property var values: [0, 1, 2, 3, 5, 10]
                        model: [qsTr("Para siempre"), qsTr("1 vez"), qsTr("2 veces"), qsTr("3 veces"), qsTr("5 veces"), qsTr("10 veces")]
                        currentIndex: Math.max(0, values.indexOf(root.opts.loops))
                        onActivated: function (i) { animStudio.setOption("loops", values[i]); }
                    }
                }

                // ---- the file
                AdjustSection {
                    width: parent.width
                    title: qsTr("Formato")
                    Row {
                        spacing: 4
                        Repeater {
                            model: root.formatNames
                            delegate: AppToolButton {
                                required property int index
                                required property string modelData
                                text: modelData
                                checked: root.opts.format === index
                                onClicked: animStudio.setOption("format", index)
                            }
                        }
                    }
                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        color: themeManager.textSecondary
                        font.pixelSize: 11
                        text: root.opts.format === 0 ? qsTr("Se abre en todas partes. Hasta 256 colores y transparencia de todo o nada.")
                            : root.opts.format === 1 ? qsTr("Colores completos y transparencia suave; lo abren los navegadores y los visores modernos. Pesa más.")
                            : qsTr("El más liviano, con colores completos y transparencia suave; lo abren los navegadores modernos.")
                    }
                    Column {
                        width: parent.width
                        spacing: 6
                        visible: root.opts.format === 0
                        OptSlider { key: "gifColors"; label: qsTr("Colores"); from: 8; to: 256; step: 1 }
                        OptCheck { key: "gifDither"; text: qsTr("Tramado (degradados más suaves)") }
                        OptCheck { key: "gifLocal"; text: qsTr("Una paleta por fotograma (mejor, pesa más)") }
                    }
                    Column {
                        width: parent.width
                        spacing: 6
                        visible: root.opts.format === 2
                        OptCheck { key: "webpLossless"; text: qsTr("Sin pérdida de calidad") }
                        OptSlider { key: "webpQuality"; label: qsTr("Calidad"); from: 10; to: 100; step: 1; visible: !root.opts.webpLossless }
                    }
                }
            }
        }

        // ---- the save button and what it did
        Column {
            id: footer
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 14
            spacing: 8

            Rectangle { width: parent.width; height: 1; color: themeManager.border }
            Label {
                width: parent.width
                visible: root.resultText !== "" && !animStudio.exporting
                text: root.resultText
                color: root.resultOk ? themeManager.accent : "#e5484d"
                wrapMode: Text.WordWrap
                font.pixelSize: 12
            }
            AppButton {
                visible: root.resultOk && root.resultPath !== "" && !animStudio.exporting
                width: parent.width
                text: qsTr("Mostrar en la carpeta")
                onClicked: Qt.openUrlExternally("file:///" + root.resultPath.replace(/[\\/][^\\/]*$/, ""))
            }
            AppProgressBar {
                width: parent.width
                visible: animStudio.exporting
                value: animStudio.progress
            }
            Row {
                width: parent.width
                spacing: 8
                AppButton {
                    visible: animStudio.exporting
                    width: parent.width
                    text: qsTr("Cancelar")
                    iconName: "cancel"
                    onClicked: animStudio.cancelExport()
                }
                AppButton {
                    visible: !animStudio.exporting
                    width: parent.width
                    primary: true
                    text: qsTr("Guardar como %1…").arg(root.formatNames[root.opts.format])
                    iconName: "save"
                    enabled: animStudio.planCount > 0
                    onClicked: { root.resultText = ""; saveDialog.open(); }
                }
            }
        }
    }

    FileDialog {
        id: addDialog
        title: qsTr("Elegí las imágenes de la animación")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Imágenes (%1)").arg(appController.supportedExtensions.join(" "))]
        onAccepted: animStudio.addUrls(selectedFiles)
    }
    FileDialog {
        id: saveDialog
        title: qsTr("Guardar la animación")
        fileMode: FileDialog.SaveFile
        defaultSuffix: root.formatSuffixes[root.opts.format]
        nameFilters: [root.formatNames[root.opts.format] + " (*." + root.formatSuffixes[root.opts.format] + ")"]
        onAccepted: animStudio.exportTo(selectedFile)
    }
}
