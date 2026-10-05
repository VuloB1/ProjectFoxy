import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import ImageViewerApp

// Crear > Collage: the collage in the middle, the layouts and options on the right. A picture can be dragged
// and zoomed inside its cell without any limit (and, if asked, spill out over its neighbours); the dividing lines
// between cells can be dragged; or the layout can be set free and every cell moved and resized on its own.
// Everything goes through `collageStudio` (src/app/CollageStudio.h); this page shows it and passes the mouse on.
Item {
    id: root

    signal closeRequested()

    readonly property var opts: collageStudio.options
    readonly property var cellData: collageStudio.cells
    readonly property int sel: collageStudio.selected
    readonly property var selCell: sel >= 0 && sel < cellData.length ? cellData[sel] : null
    readonly property bool free: opts.free === true
    property bool resultOk: true
    property string resultText: ""
    property string resultPath: ""
    readonly property var formatNames: ["PNG", "JPG", "WebP"]

    function sizeText(bytes) {
        if (bytes >= 1048576) return (bytes / 1048576).toFixed(1) + " MB";
        return Math.max(1, Math.round(bytes / 1024)) + " KB";
    }

    Rectangle { anchors.fill: parent; color: themeManager.background }

    Connections {
        target: collageStudio
        function onExportFinished(ok, path, bytes, error) {
            root.resultOk = ok;
            root.resultPath = ok ? path : "";
            root.resultText = ok ? qsTr("Guardado: %1 (%2)").arg(path.replace(/^.*[\\/]/, "")).arg(root.sizeText(bytes))
                                 : (error !== "" ? error : qsTr("No se pudo guardar."));
        }
    }
    Component.onCompleted: {
        if (collageStudio.selected < 0 && collageStudio.cellCount > 0)
            collageStudio.select(0);
    }

    // ---------------------------------------------------------------- the left side: the collage
    Item {
        id: left
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: side.left

        Row {
            id: topBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.topMargin: 10
            anchors.leftMargin: 12
            spacing: 6
            height: 34
            AppIcon { anchors.verticalCenter: parent.verticalCenter; name: "collage"; size: 22; color: themeManager.accent }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Crear collage")
                color: themeManager.textPrimary
                font.bold: true
                font.pixelSize: 15
                rightPadding: 12
            }
            AppButton { text: qsTr("Agregar fotos…"); onClicked: addDialog.open() }
            AppButton {
                text: qsTr("Toda la carpeta")
                enabled: folderModel.count > 0
                onClicked: collageStudio.addPaths(folderModel.filePaths())
                AppToolTip { visible: parent.hovered; text: qsTr("Agrega las imágenes de la carpeta que está abierta, hasta llenar 12 celdas") }
            }
            AppButton { text: qsTr("Mezclar fotos"); onClicked: collageStudio.shufflePictures() }
            AppButton { text: qsTr("Quitar fotos"); onClicked: collageStudio.clearPictures() }
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

        Item {
            id: stage
            anchors.top: topBar.bottom
            anchors.topMargin: 10
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: hint.top
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            clip: true

            readonly property real aspect: root.opts.sizeW / Math.max(1, root.opts.sizeH)
            readonly property real boxW: Math.min(width, height * aspect)
            readonly property real boxH: boxW / aspect

            Item {
                id: box
                anchors.centerIn: parent
                width: stage.boxW
                height: stage.boxH

                // behind the picture, for a transparent background
                Canvas {
                    anchors.fill: parent
                    visible: root.opts.transparentBackground
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
                // the next picture is drawn here, out of sight, and shown once it is ready - so the collage never blinks
                Image {
                    id: preview
                    visible: false
                    source: "image://collageprev/" + collageStudio.revision
                    sourceSize: Qt.size(1000, 1000)
                    asynchronous: true
                    cache: true
                    property url shownUrl
                    onStatusChanged: if (status === Image.Ready) shownUrl = source
                }
                Image {
                    anchors.fill: parent
                    source: preview.shownUrl
                    sourceSize: Qt.size(1000, 1000)
                    cache: true
                    smooth: true
                    fillMode: Image.Stretch
                }
                Rectangle { anchors.fill: parent; color: "transparent"; border.width: 1; border.color: themeManager.border }

                // empty cells say so
                Repeater {
                    model: root.cellData
                    delegate: Item {
                        required property var modelData
                        required property int index
                        x: modelData.x * box.width
                        y: modelData.y * box.height
                        width: modelData.w * box.width
                        height: modelData.h * box.height
                        visible: modelData.empty
                        Rectangle {
                            anchors.fill: parent
                            color: Qt.rgba(0.5, 0.5, 0.55, 0.18)
                            border.width: 1
                            border.color: Qt.rgba(1, 1, 1, 0.35)
                        }
                        AppIcon { anchors.centerIn: parent; name: "plus"; size: Math.min(34, parent.width * 0.4, parent.height * 0.4); color: Qt.rgba(1, 1, 1, 0.7) }
                    }
                }

                // the selected cell
                Rectangle {
                    visible: root.selCell !== null
                    x: root.selCell ? root.selCell.x * box.width : 0
                    y: root.selCell ? root.selCell.y * box.height : 0
                    width: root.selCell ? root.selCell.w * box.width : 0
                    height: root.selCell ? root.selCell.h * box.height : 0
                    color: "transparent"
                    border.width: 2
                    border.color: themeManager.accent
                }
                // handles to resize it (free layout)
                Repeater {
                    model: root.free && root.selCell ? 8 : 0
                    delegate: Rectangle {
                        required property int index
                        readonly property var pos: box.handlePos(index)
                        x: pos.x - 5
                        y: pos.y - 5
                        width: 10
                        height: 10
                        radius: 2
                        color: themeManager.accent
                        border.width: 1
                        border.color: "#ffffff"
                    }
                }
                // the dividing lines
                Repeater {
                    model: root.free ? [] : collageStudio.dividers
                    delegate: Item {
                        required property var modelData
                        readonly property bool v: modelData.vertical
                        x: v ? modelData.pos * box.width - 1.5 : modelData.from * box.width
                        y: v ? modelData.from * box.height : modelData.pos * box.height - 1.5
                        width: v ? 3 : (modelData.to - modelData.from) * box.width
                        height: v ? (modelData.to - modelData.from) * box.height : 3
                        Rectangle {
                            anchors.fill: parent
                            color: Qt.rgba(themeManager.accent.r, themeManager.accent.g, themeManager.accent.b, box.hoverDivider >= 0 ? 0.5 : 0.28)
                        }
                        // a grip in the middle
                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.v ? 8 : 34
                            height: parent.v ? 34 : 8
                            radius: 4
                            color: themeManager.accent
                            border.width: 1
                            border.color: "#ffffff"
                        }
                    }
                }

                // ---- the mouse
                property int hoverDivider: -1
                property int hoverHandle: -1
                function handlePos(i) {
                    const c = root.selCell;
                    if (!c) return Qt.point(0, 0);
                    const x0 = c.x * width, y0 = c.y * height, x1 = (c.x + c.w) * width, y1 = (c.y + c.h) * height;
                    const xm = (x0 + x1) / 2, ym = (y0 + y1) / 2;
                    return [Qt.point(x0, y0), Qt.point(xm, y0), Qt.point(x1, y0), Qt.point(x1, ym),
                            Qt.point(x1, y1), Qt.point(xm, y1), Qt.point(x0, y1), Qt.point(x0, ym)][i];
                }
                function dividerAt(mx, my) {
                    const list = collageStudio.dividers;
                    for (let i = 0; i < list.length; ++i) {
                        const d = list[i];
                        if (d.vertical) {
                            if (Math.abs(mx - d.pos * width) < 9 && my >= d.from * height - 4 && my <= d.to * height + 4) return i;
                        } else if (Math.abs(my - d.pos * height) < 9 && mx >= d.from * width - 4 && mx <= d.to * width + 4) {
                            return i;
                        }
                    }
                    return -1;
                }
                function handleAt(mx, my) {
                    if (!root.free || !root.selCell) return -1;
                    for (let i = 0; i < 8; ++i) {
                        const p = handlePos(i);
                        if (Math.abs(mx - p.x) < 9 && Math.abs(my - p.y) < 9) return i;
                    }
                    return -1;
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    // "", "divider", "resize", "move", "pan"
                    property string mode: ""
                    property int target: -1
                    property real lastX: 0
                    property real lastY: 0
                    cursorShape: {
                        if (mode === "divider" || (mode === "" && box.hoverDivider >= 0)) {
                            const list = collageStudio.dividers;
                            const i = mode === "divider" ? target : box.hoverDivider;
                            return list[i] && list[i].vertical ? Qt.SizeHorCursor : Qt.SizeVerCursor;
                        }
                        if (mode === "resize" || box.hoverHandle >= 0) return Qt.SizeAllCursor;
                        if (mode === "move" || mode === "pan") return Qt.ClosedHandCursor;
                        return Qt.OpenHandCursor;
                    }
                    onPressed: function (m) {
                        lastX = m.x;
                        lastY = m.y;
                        const nx = m.x / box.width, ny = m.y / box.height;
                        const handle = box.handleAt(m.x, m.y);
                        if (m.button === Qt.LeftButton && handle >= 0) {
                            mode = "resize";
                            target = handle;
                            return;
                        }
                        const div = root.free ? -1 : box.dividerAt(m.x, m.y);
                        if (m.button === Qt.LeftButton && div >= 0 && collageStudio.beginDividerDrag(div)) {
                            mode = "divider";
                            target = div;
                            return;
                        }
                        const cell = collageStudio.cellAt(nx, ny);
                        if (cell < 0) {
                            mode = "";
                            return;
                        }
                        collageStudio.select(cell);
                        target = cell;
                        const panning = m.button === Qt.RightButton || (m.modifiers & Qt.ShiftModifier) || !root.free;
                        mode = panning ? "pan" : "move";
                    }
                    onPositionChanged: function (m) {
                        const nx = m.x / box.width, ny = m.y / box.height;
                        if (!pressed) {
                            box.hoverDivider = root.free ? -1 : box.dividerAt(m.x, m.y);
                            box.hoverHandle = box.handleAt(m.x, m.y);
                            return;
                        }
                        const dx = (m.x - lastX) / box.width, dy = (m.y - lastY) / box.height;
                        lastX = m.x;
                        lastY = m.y;
                        if (mode === "divider") {
                            const list = collageStudio.dividers;
                            const v = list[target] ? list[target].vertical : true;
                            collageStudio.dragDivider(v ? nx : ny);
                        } else if (mode === "resize") collageStudio.resizeCell(root.sel, target, dx, dy);
                        else if (mode === "move") collageStudio.moveCell(target, dx, dy);
                        else if (mode === "pan") collageStudio.panCell(target, dx, dy);
                    }
                    onReleased: {
                        if (mode === "divider")
                            collageStudio.endDividerDrag();
                        mode = "";
                    }
                    onCanceled: { if (mode === "divider") collageStudio.endDividerDrag(); mode = ""; }
                    onDoubleClicked: function (m) {
                        const cell = collageStudio.cellAt(m.x / box.width, m.y / box.height);
                        if (cell >= 0) collageStudio.resetPicture(cell);
                    }
                    onWheel: function (w) {
                        const cell = collageStudio.cellAt(w.x / box.width, w.y / box.height);
                        if (cell < 0) return;
                        collageStudio.select(cell);
                        collageStudio.zoomCell(cell, Math.pow(1.0016, w.angleDelta.y), w.x / box.width, w.y / box.height);
                    }
                }

                // files dropped on a cell replace its picture; anywhere else they fill the empty cells
                DropArea {
                    anchors.fill: parent
                    onDropped: function (drop) {
                        if (!drop.hasUrls || drop.urls.length === 0)
                            return;
                        const cell = collageStudio.cellAt(drop.x / box.width, drop.y / box.height);
                        if (cell >= 0) {
                            collageStudio.setPicture(cell, drop.urls[0].toString());
                            collageStudio.select(cell);
                            if (drop.urls.length > 1)
                                collageStudio.addUrls(drop.urls.slice(1));
                        } else {
                            collageStudio.addUrls(drop.urls);
                        }
                    }
                }
            }
        }

        Label {
            id: hint
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: themeManager.textSecondary
            font.pixelSize: 11
            text: root.free
                  ? qsTr("Arrastrá una celda para moverla y sus puntos para cambiarle el tamaño. Shift o botón derecho: mover la foto dentro. Rueda: acercar la foto. Doble clic: restablecerla.")
                  : qsTr("Arrastrá una foto para moverla dentro de su celda y usá la rueda para acercarla (sin límites: puede quedar más chica que la celda). Arrastrá las líneas para cambiar el tamaño de las celdas.")
        }
    }

    // ---------------------------------------------------------------- the right side: layout and options
    Rectangle {
        id: side
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 330
        color: themeManager.surface
        Rectangle { anchors.left: parent.left; width: 1; height: parent.height; color: themeManager.border }

        component OptSlider: Item {
            property string key: ""
            property string label: ""
            property real from: 0
            property real to: 100
            property real step: 1
            property string suffix: ""
            property int decimals: 0
            property real shown: collageStudio.options[key]
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
                onMoved: collageStudio.setOption(parent.key, value)
            }
        }
        component OptCheck: AppCheckBox {
            property string key: ""
            width: parent ? parent.width : 0
            checked: collageStudio.options[key] === true
            onToggled: collageStudio.setOption(key, checked)
        }
        // a layout drawn small
        component PresetThumb: Item {
            id: thumb
            property var rects: []
            property bool picked: false
            property string label: ""
            signal clicked()
            width: 62
            height: 62
            Rectangle {
                anchors.fill: parent
                radius: themeManager.radiusSmall
                color: picked ? Qt.rgba(themeManager.accent.r, themeManager.accent.g, themeManager.accent.b, 0.22) : (thumbHover.hovered ? Qt.rgba(1, 1, 1, 0.06) : "transparent")
                border.width: picked ? 2 : 1
                border.color: picked ? themeManager.accent : themeManager.border
            }
            Canvas {
                id: canvas
                anchors.fill: parent
                anchors.margins: 6
                property color fill: picked ? themeManager.accent : themeManager.textSecondary
                onFillChanged: requestPaint()
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                Component.onCompleted: requestPaint()
                onPaint: {
                    const ctx = getContext("2d");
                    ctx.reset();
                    ctx.fillStyle = fill;
                    for (const r of rects)
                        ctx.fillRect(r[0] * width + 1, r[1] * height + 1, r[2] * width - 2, r[3] * height - 2);
                }
                Connections { target: thumb; function onRectsChanged() { canvas.requestPaint(); } }
            }
            HoverHandler { id: thumbHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: thumb.clicked() }
            AppToolTip { visible: thumbHover.hovered; text: thumb.label }
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

                // ---- the layout
                AdjustSection {
                    width: parent.width
                    title: qsTr("Diseño")
                    Row {
                        width: parent.width
                        spacing: 6
                        Label { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Fotos"); color: themeManager.textSecondary; width: 60 }
                        AppToolButton { text: qsTr("Una menos"); iconName: "minus"; iconOnly: true; enabled: collageStudio.cellCount > 1; onClicked: collageStudio.setCellCount(collageStudio.cellCount - 1) }
                        Label { anchors.verticalCenter: parent.verticalCenter; width: 26; horizontalAlignment: Text.AlignHCenter; text: collageStudio.cellCount; color: themeManager.textPrimary; font.bold: true }
                        AppToolButton { text: qsTr("Una más"); iconName: "plus"; iconOnly: true; enabled: collageStudio.cellCount < 12; onClicked: collageStudio.setCellCount(collageStudio.cellCount + 1) }
                    }
                    Row {
                        spacing: 4
                        AppToolButton {
                            text: qsTr("Rejilla")
                            checked: !root.free
                            onClicked: collageStudio.setOption("free", false)
                            AppToolTip { visible: parent.hovered; text: qsTr("Celdas que encajan, con líneas divisorias que se arrastran") }
                        }
                        AppToolButton {
                            text: qsTr("Libre")
                            checked: root.free
                            onClicked: collageStudio.setOption("free", true)
                            AppToolTip { visible: parent.hovered; text: qsTr("Cada celda se mueve y se redimensiona por su cuenta, y pueden superponerse") }
                        }
                        AppToolButton { text: qsTr("Otro mosaico"); onClicked: collageStudio.newMosaic() }
                    }
                    Flow {
                        width: parent.width
                        spacing: 6
                        Repeater {
                            model: collageStudio.presets
                            delegate: PresetThumb {
                                required property int index
                                required property var modelData
                                rects: modelData.rects
                                label: modelData.name
                                picked: collageStudio.presetIndex === index
                                onClicked: collageStudio.applyPreset(index)
                            }
                        }
                    }
                    Label {
                        text: qsTr("Mis plantillas")
                        color: themeManager.textSecondary
                        font.pixelSize: 11
                        topPadding: 4
                    }
                    Row {
                        width: parent.width
                        spacing: 6
                        AppTextField {
                            id: templateName
                            width: parent.width - saveTemplateButton.width - 6
                            placeholderText: qsTr("Nombre de la plantilla")
                            maximumLength: 40
                            onAccepted: saveTemplateButton.clicked()
                        }
                        AppToolButton {
                            id: saveTemplateButton
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Guardar")
                            enabled: templateName.text.trim().length > 0
                            onClicked: {
                                if (collageStudio.saveTemplate(templateName.text))
                                    templateName.text = "";
                            }
                            AppToolTip { visible: parent.hovered; text: qsTr("Guarda este diseño y cómo se ve (separación, bordes, fondo...) para usarlo de nuevo, sin las fotos") }
                        }
                    }
                    Label {
                        width: parent.width
                        visible: collageStudio.templates.length === 0
                        text: qsTr("Todavía no guardaste ninguna.")
                        color: themeManager.textSecondary
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                    Flow {
                        width: parent.width
                        spacing: 6
                        Repeater {
                            model: collageStudio.templates
                            delegate: Item {
                                required property int index
                                required property var modelData
                                width: 62
                                height: 62
                                PresetThumb {
                                    rects: modelData.rects
                                    label: qsTr("%1 (%2 fotos)").arg(modelData.name).arg(modelData.count)
                                    onClicked: collageStudio.applyTemplate(index)
                                }
                                Rectangle {
                                    id: delBadge
                                    anchors.top: parent.top
                                    anchors.right: parent.right
                                    anchors.margins: 2
                                    width: 16
                                    height: 16
                                    radius: 8
                                    visible: delHover.hovered || tplHover.hovered
                                    color: delHover.hovered ? "#d9534f" : Qt.rgba(0, 0, 0, 0.55)
                                    AppIcon { anchors.centerIn: parent; name: "close"; size: 9; color: "#fff" }
                                    HoverHandler { id: delHover; cursorShape: Qt.PointingHandCursor }
                                    TapHandler { onTapped: collageStudio.deleteTemplate(index) }
                                    AppToolTip { visible: delHover.hovered; text: qsTr("Borrar esta plantilla") }
                                }
                                HoverHandler { id: tplHover }
                            }
                        }
                    }
                }

                // ---- the picture picked
                AdjustSection {
                    width: parent.width
                    title: root.selCell ? qsTr("Foto %1").arg(root.sel + 1) : qsTr("Foto")
                    Label {
                        width: parent.width
                        visible: root.selCell === null
                        text: qsTr("Tocá una celda para ajustar su foto.")
                        color: themeManager.textSecondary
                        font.pixelSize: 11
                    }
                    Column {
                        width: parent.width
                        spacing: 6
                        visible: root.selCell !== null
                        Label {
                            width: parent.width
                            text: root.selCell && !root.selCell.empty ? root.selCell.name : qsTr("Celda vacía")
                            color: themeManager.textPrimary
                            elide: Text.ElideMiddle
                        }
                        Item {
                            width: parent.width
                            implicitHeight: 40
                            Label { text: qsTr("Zoom"); color: themeManager.textSecondary }
                            Label { anchors.right: parent.right; text: root.selCell ? Math.round(root.selCell.zoom * 100) + "%" : ""; color: themeManager.textPrimary; font.bold: true }
                            AppSlider {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                from: 0.1; to: 5
                                value: root.selCell ? root.selCell.zoom : 1
                                snapThreshold: 0.06
                                snapValue: 1
                                onMoved: collageStudio.setCellProperty(root.sel, "zoom", value)
                            }
                        }
                        Item {
                            width: parent.width
                            implicitHeight: 40
                            Label { text: qsTr("Giro"); color: themeManager.textSecondary }
                            Label { anchors.right: parent.right; text: root.selCell ? Math.round(root.selCell.rotation) + "°" : ""; color: themeManager.textPrimary; font.bold: true }
                            AppSlider {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                from: -180; to: 180; stepSize: 1
                                value: root.selCell ? root.selCell.rotation : 0
                                snapThreshold: 4
                                snapValue: 0
                                neutralMark: true
                                onMoved: collageStudio.setCellProperty(root.sel, "rotation", value)
                            }
                        }
                        Row {
                            spacing: 4
                            AppToolButton { text: qsTr("Espejo horizontal"); iconName: "flip-h"; iconOnly: true; checked: root.selCell ? root.selCell.flipH : false; onClicked: collageStudio.setCellProperty(root.sel, "flipH", !root.selCell.flipH) }
                            AppToolButton { text: qsTr("Espejo vertical"); iconName: "flip-v"; iconOnly: true; checked: root.selCell ? root.selCell.flipV : false; onClicked: collageStudio.setCellProperty(root.sel, "flipV", !root.selCell.flipV) }
                        }
                        AppCheckBox {
                            width: parent.width
                            text: qsTr("Dejar que la foto salga de su celda")
                            checked: root.selCell ? root.selCell.overflow : false
                            onToggled: collageStudio.setCellProperty(root.sel, "overflow", checked)
                            AppToolTip { visible: parent.hovered; text: qsTr("La foto no se corta en el borde de la celda: pasa por encima de las vecinas") }
                        }
                        Flow {
                            width: parent.width
                            spacing: 4
                            AppButton { text: qsTr("Restablecer"); onClicked: collageStudio.resetPicture(root.sel) }
                            AppButton { text: qsTr("Cambiar foto…"); onClicked: pickDialog.open() }
                            AppButton { text: qsTr("Quitar foto"); enabled: root.selCell && !root.selCell.empty; onClicked: collageStudio.clearPicture(root.sel) }
                            AppButton { text: qsTr("Traer al frente"); visible: root.free; onClicked: collageStudio.bringToFront(root.sel) }
                        }
                    }
                }

                // ---- how the cells look
                AdjustSection {
                    width: parent.width
                    title: qsTr("Celdas")
                    OptSlider { key: "spacing"; label: qsTr("Separación"); from: 0; to: 15; step: 0.1; suffix: "%"; decimals: 1 }
                    OptSlider { key: "margin"; label: qsTr("Margen"); from: 0; to: 25; step: 0.1; suffix: "%"; decimals: 1 }
                    OptSlider { key: "radius"; label: qsTr("Esquinas redondeadas"); from: 0; to: 100; step: 1; suffix: "%" }
                    OptSlider { key: "borderWidth"; label: qsTr("Borde"); from: 0; to: 8; step: 0.1; suffix: "%"; decimals: 1 }
                    Row {
                        width: parent.width
                        spacing: 10
                        visible: collageStudio.options.borderWidth > 0
                        Label { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Color del borde"); color: themeManager.textSecondary; width: 110 }
                        AppColorSwatch {
                            anchors.verticalCenter: parent.verticalCenter
                            value: Qt.rgba(((collageStudio.options.borderColor >> 16) & 255) / 255, ((collageStudio.options.borderColor >> 8) & 255) / 255, (collageStudio.options.borderColor & 255) / 255, 1)
                            onPicked: function (c) { collageStudio.setOption("borderColor", (Math.round(c.r * 255) << 16) | (Math.round(c.g * 255) << 8) | Math.round(c.b * 255)); }
                        }
                    }
                    OptSlider { key: "shadow"; label: qsTr("Sombra"); from: 0; to: 100; step: 1; suffix: "%" }
                    Column {
                        width: parent.width
                        spacing: 6
                        visible: collageStudio.options.shadow > 0
                        OptSlider { key: "shadowBlur"; label: qsTr("Desenfoque de la sombra"); from: 0; to: 10; step: 0.1; decimals: 1; suffix: "%" }
                        OptSlider { key: "shadowOffset"; label: qsTr("Distancia de la sombra"); from: 0; to: 6; step: 0.1; decimals: 1; suffix: "%" }
                    }
                    Row {
                        width: parent.width
                        spacing: 10
                        OptCheck { key: "cellFillOn"; text: qsTr("Relleno de las celdas"); width: parent.width - 80 }
                        AppColorSwatch {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: collageStudio.options.cellFillOn
                            value: Qt.rgba(((collageStudio.options.cellFill >> 16) & 255) / 255, ((collageStudio.options.cellFill >> 8) & 255) / 255, (collageStudio.options.cellFill & 255) / 255, 1)
                            onPicked: function (c) { collageStudio.setOption("cellFill", (Math.round(c.r * 255) << 16) | (Math.round(c.g * 255) << 8) | Math.round(c.b * 255)); }
                        }
                    }
                }

                // ---- background
                AdjustSection {
                    width: parent.width
                    title: qsTr("Fondo")
                    Row {
                        spacing: 4
                        Repeater {
                            model: [qsTr("Color"), qsTr("Degradado"), qsTr("Foto desenfocada")]
                            delegate: AppToolButton {
                                required property int index
                                required property string modelData
                                text: modelData
                                checked: collageStudio.options.background === index
                                onClicked: collageStudio.setOption("background", index)
                            }
                        }
                    }
                    Row {
                        width: parent.width
                        spacing: 10
                        visible: collageStudio.options.background < 2
                        AppColorSwatch {
                            anchors.verticalCenter: parent.verticalCenter
                            value: Qt.rgba(((collageStudio.options.color1 >> 16) & 255) / 255, ((collageStudio.options.color1 >> 8) & 255) / 255, (collageStudio.options.color1 & 255) / 255, 1)
                            onPicked: function (c) { collageStudio.setOption("color1", (Math.round(c.r * 255) << 16) | (Math.round(c.g * 255) << 8) | Math.round(c.b * 255)); }
                        }
                        AppColorSwatch {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: collageStudio.options.background === 1
                            value: Qt.rgba(((collageStudio.options.color2 >> 16) & 255) / 255, ((collageStudio.options.color2 >> 8) & 255) / 255, (collageStudio.options.color2 & 255) / 255, 1)
                            onPicked: function (c) { collageStudio.setOption("color2", (Math.round(c.r * 255) << 16) | (Math.round(c.g * 255) << 8) | Math.round(c.b * 255)); }
                        }
                        OptCheck { key: "transparentBackground"; text: qsTr("Transparente"); width: 130; visible: collageStudio.options.background === 0 }
                    }
                    OptSlider { key: "gradientAngle"; label: qsTr("Ángulo del degradado"); from: -180; to: 180; step: 1; suffix: "°"; visible: collageStudio.options.background === 1 }
                    OptSlider { key: "photoBlur"; label: qsTr("Desenfoque"); from: 0; to: 30; step: 1; suffix: "%"; visible: collageStudio.options.background === 2 }
                }

                // ---- size and file
                AdjustSection {
                    width: parent.width
                    title: qsTr("Tamaño y archivo")
                    Row {
                        width: parent.width
                        spacing: 8
                        AppSpinBox {
                            width: (parent.width - 28) / 2
                            from: 100; to: 8000; stepSize: 50
                            editable: true
                            value: collageStudio.options.sizeW
                            onValueModified: collageStudio.setOption("sizeW", value)
                        }
                        Label { anchors.verticalCenter: parent.verticalCenter; text: "×"; color: themeManager.textSecondary }
                        AppSpinBox {
                            width: (parent.width - 28) / 2
                            from: 100; to: 8000; stepSize: 50
                            editable: true
                            value: collageStudio.options.sizeH
                            onValueModified: collageStudio.setOption("sizeH", value)
                        }
                    }
                    Flow {
                        width: parent.width
                        spacing: 4
                        Repeater {
                            model: [
                                { n: qsTr("Cuadrado"), w: 2000, h: 2000 },
                                { n: qsTr("4:3"), w: 2400, h: 1800 },
                                { n: qsTr("3:4"), w: 1800, h: 2400 },
                                { n: qsTr("16:9"), w: 3200, h: 1800 },
                                { n: qsTr("9:16"), w: 1800, h: 3200 },
                                { n: qsTr("A4 vertical"), w: 2480, h: 3508 },
                                { n: qsTr("A4 horizontal"), w: 3508, h: 2480 }
                            ]
                            delegate: AppToolButton {
                                required property var modelData
                                text: modelData.n
                                checked: collageStudio.options.sizeW === modelData.w && collageStudio.options.sizeH === modelData.h
                                onClicked: { collageStudio.setOption("sizeW", modelData.w); collageStudio.setOption("sizeH", modelData.h); }
                            }
                        }
                    }
                    Row {
                        spacing: 4
                        Repeater {
                            model: root.formatNames
                            delegate: AppToolButton {
                                required property int index
                                required property string modelData
                                text: modelData
                                checked: collageStudio.options.format === index
                                onClicked: collageStudio.setOption("format", index)
                            }
                        }
                    }
                    OptSlider {
                        key: "quality"
                        label: collageStudio.options.format === 2 ? qsTr("Calidad (100 = sin pérdida)") : qsTr("Calidad")
                        from: 40; to: 100; step: 1
                        visible: collageStudio.options.format !== 0
                    }
                }
            }
        }

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
                visible: root.resultText !== "" && !collageStudio.exporting
                text: root.resultText
                color: root.resultOk ? themeManager.accent : "#e5484d"
                wrapMode: Text.WordWrap
                font.pixelSize: 12
            }
            AppButton {
                visible: root.resultOk && root.resultPath !== "" && !collageStudio.exporting
                width: parent.width
                text: qsTr("Mostrar en la carpeta")
                onClicked: Qt.openUrlExternally("file:///" + root.resultPath.replace(/[\\/][^\\/]*$/, ""))
            }
            AppButton {
                visible: collageStudio.exporting
                width: parent.width
                text: qsTr("Guardando… Cancelar")
                iconName: "cancel"
                onClicked: collageStudio.cancelExport()
            }
            AppButton {
                visible: !collageStudio.exporting
                width: parent.width
                primary: true
                text: qsTr("Guardar como %1…").arg(root.formatNames[collageStudio.options.format])
                iconName: "save"
                onClicked: { root.resultText = ""; saveDialog.open(); }
            }
        }
    }

    FileDialog {
        id: addDialog
        title: qsTr("Elegí las fotos del collage")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Imágenes (%1)").arg(appController.supportedExtensions.join(" "))]
        onAccepted: collageStudio.addUrls(Array.from(selectedFiles, u => u.toString()))
    }
    FileDialog {
        id: pickDialog
        title: qsTr("Elegí la foto de esta celda")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Imágenes (%1)").arg(appController.supportedExtensions.join(" "))]
        onAccepted: collageStudio.setPicture(root.sel, selectedFile.toString())
    }
    FileDialog {
        id: saveDialog
        title: qsTr("Guardar el collage")
        fileMode: FileDialog.SaveFile
        defaultSuffix: ["png", "jpg", "webp"][collageStudio.options.format]
        nameFilters: [root.formatNames[collageStudio.options.format] + " (*." + ["png", "jpg", "webp"][collageStudio.options.format] + ")"]
        onAccepted: collageStudio.exportTo(selectedFile)
    }
}
