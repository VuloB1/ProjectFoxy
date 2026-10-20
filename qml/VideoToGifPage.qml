import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Crear > GIF animado > Desde un vídeo: the video with its crop frame, the timeline where the pieces to keep are chosen,
// and the frame rate and width of the animation. "Crear fotogramas" reads them (VideoToGif) into the studio's strip.
Item {
    id: root
    objectName: "videoToGifPage"

    signal closeRequested()

    readonly property int duration: Math.max(1, videoToGif.durationMs)
    readonly property int minPieceMs: 200
    readonly property int maxSeconds: 30
    readonly property real maxMegabytes: 25

    // what is chosen
    property int startMs: 0
    property int endMs: duration
    property int cursorMs: 0
    property var cuts: []            // times (ms) where the video is cut, inside start..end
    property var removedKeys: []     // start (ms) of the pieces that are left out
    property int fps: 10
    property int chosenWidth: 480
    property bool widthChosen: false // false: the width follows the frame (up to 480) until a width is picked
    readonly property int outWidth: widthChosen ? chosenWidth : Math.max(16, Math.min(480, cropWidthPx))
    readonly property var fpsChoices: [5, 8, 10, 12, 15, 20]
    readonly property var aspectChoices: [0 /* original */, 1, 16 / 9, 4 / 3, 3 / 4, 9 / 16, -1 /* free */]
    property int aspectIndex: 0

    // the pieces, in order: [{ from, to, removed }]
    readonly property var pieces: computePieces(startMs, endMs, cuts, removedKeys)
    readonly property var kept: pieces.filter(function (p) { return !p.removed; }).map(function (p) { return [p.from, p.to]; })
    readonly property int keptMs: kept.reduce(function (sum, p) { return sum + (p[1] - p[0]); }, 0)
    readonly property int frameCount: videoToGif.frameCount(kept, fps)

    // the crop in real pixels, and so the size of the animation
    readonly property int cropWidthPx: Math.max(2, Math.round(crop.cw * videoToGif.sourceWidth))
    readonly property int cropHeightPx: Math.max(2, Math.round(crop.ch * videoToGif.sourceHeight))
    readonly property int outHeight: Math.max(2, Math.round(outWidth * cropHeightPx / cropWidthPx))
    // a rough weight of the GIF: bytes per pixel of what video usually gives
    readonly property real megabytes: frameCount * outWidth * outHeight * 0.3 / 1048576
    readonly property bool tooMany: frameCount > videoToGif.maxFrames
    readonly property bool canCreate: videoToGif.loaded && !videoToGif.busy && frameCount > 0 && !tooMany

    function computePieces(a, b, cutList, removed) {
        const points = [a];
        cutList.slice().sort(function (x, y) { return x - y; }).forEach(function (c) {
            if (c > a + minPieceMs && c < b - minPieceMs && c > points[points.length - 1] + minPieceMs)
                points.push(c);
        });
        points.push(b);
        const out = [];
        for (let i = 0; i < points.length - 1; ++i)
            out.push({ from: points[i], to: points[i + 1], removed: removed.indexOf(points[i]) >= 0 });
        return out;
    }
    function clock(ms) {
        const s = Math.max(0, ms) / 1000;
        const m = Math.floor(s / 60);
        const r = (s - m * 60).toFixed(1).replace(".", ",");
        return m + ":" + (r.length < 4 ? "0" : "") + r;
    }
    function secondsText(ms) { return (ms / 1000).toFixed(1).replace(".", ",") + " s"; }
    function togglePiece(from) {
        const list = removedKeys.slice();
        const i = list.indexOf(from);
        if (i >= 0) list.splice(i, 1); else list.push(from);
        removedKeys = list;
    }
    function cutHere() {
        if (cursorMs <= startMs + minPieceMs || cursorMs >= endMs - minPieceMs)
            return;
        for (let i = 0; i < cuts.length; ++i)
            if (Math.abs(cuts[i] - cursorMs) < minPieceMs)
                return;
        cuts = cuts.concat([cursorMs]);
    }
    function setCursor(ms) {
        cursorMs = Math.max(0, Math.min(Math.max(0, duration - 60), Math.round(ms)));
        seekTimer.restart();
    }

    Timer { id: seekTimer; interval: 50; onTriggered: videoToGif.seek(root.cursorMs) }

    Connections {
        target: videoToGif
        function onLoadedChanged() {
            if (videoToGif.loaded) {
                root.startMs = 0; root.endMs = root.duration; root.cursorMs = 0;
                root.cuts = []; root.removedKeys = []; crop.reset(); root.aspectIndex = 0; crop.aspect = 0;
                root.widthChosen = false;
            }
        }
        function onFramesReady() { root.closeRequested(); }
    }

    Rectangle { anchors.fill: parent; color: themeManager.background }

    DialogHeader {
        id: header
        iconName: "film"
        title: qsTr("Vídeo a GIF")
        hint: videoToGif.loaded ? qsTr("%1 · %2×%3 · %4").arg(videoToGif.fileName).arg(videoToGif.sourceWidth).arg(videoToGif.sourceHeight).arg(root.secondsText(root.duration)) : videoToGif.fileName
        closeEnabled: !videoToGif.busy
        margin: 18
        height: 60
        onCloseRequested: { videoToGif.close(); root.closeRequested(); }
    }

    // opening, or not opened
    Column {
        anchors.centerIn: parent
        spacing: 12
        visible: !videoToGif.loaded
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: videoToGif.error === "open" ? qsTr("No se pudo abrir este vídeo") : qsTr("Abriendo el vídeo…")
            color: themeManager.textPrimary
            font.pixelSize: 15
        }
        Label {
            visible: videoToGif.error === "open"
            anchors.horizontalCenter: parent.horizontalCenter
            width: 420
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            color: themeManager.textSecondary
            text: qsTr("Los vídeos MP4 (H.264), MOV y MKV suelen abrirse bien. Algunos códecs, como VP9 o AV1, no se pueden leer.")
        }
        AppButton {
            visible: videoToGif.error === "open"
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Cerrar")
            onClicked: { videoToGif.close(); root.closeRequested(); }
        }
    }

    // ---------------------------------------------------------------- the editor
    Item {
        id: editor
        visible: videoToGif.loaded
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: side.left
        anchors.bottom: bottomBar.top
        anchors.margins: 14

        // the picture and its crop frame
        Item {
            id: previewBox
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: parent.height - controls.height - 10

            Rectangle { anchors.fill: parent; color: "#101014"; radius: themeManager.radiusSmall }
            Image {
                id: picture
                anchors.fill: parent
                anchors.margins: 4
                source: videoToGif.loaded ? "image://videoprev/" + videoToGif.previewRevision : ""
                cache: false
                fillMode: Image.PreserveAspectFit
                sourceSize.width: 1600
                smooth: true
            }
            VideoCropBox {
                id: crop
                objectName: "videoCrop"
                anchors.fill: picture
                area: Qt.rect((picture.width - picture.paintedWidth) / 2, (picture.height - picture.paintedHeight) / 2,
                              picture.paintedWidth, picture.paintedHeight)
                srcWidth: Math.max(1, videoToGif.sourceWidth)
                srcHeight: Math.max(1, videoToGif.sourceHeight)
            }
            Rectangle {
                x: 12; y: parent.height - height - 10
                width: tag.width + 14; height: 22; radius: 6; color: "#aa000000"
                Label { id: tag; anchors.centerIn: parent; text: root.clock(root.cursorMs); color: "#ffffff"; font.pixelSize: 12 }
            }
        }

        Column {
            id: controls
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            spacing: 8

            // the proportion of the crop
            Flow {
                width: parent.width
                spacing: 10
                Label { height: 32; verticalAlignment: Text.AlignVCenter; text: qsTr("Recortar la imagen"); color: themeManager.textSecondary }
                AppChoiceChips {
                    width: Math.min(parent.width, 460)
                    model: [{ text: qsTr("Todo") }, { text: "1:1" }, { text: "16:9" }, { text: "4:3" }, { text: "3:4" }, { text: "9:16" }, { text: qsTr("Libre") }]
                    currentIndex: root.aspectIndex
                    onActivated: function (i) {
                        root.aspectIndex = i;
                        const a = root.aspectChoices[i];
                        if (i === 0) { crop.reset(); crop.aspect = 0; }
                        else if (a < 0) { crop.aspect = 0; if (crop.whole) { crop.cx = 0.1; crop.cy = 0.1; crop.cw = 0.8; crop.ch = 0.8; } }
                        else crop.applyAspect(a);
                    }
                }
                Label {
                    height: 32
                    verticalAlignment: Text.AlignVCenter
                    visible: !crop.whole
                    text: qsTr("Arrastra el marco; las esquinas lo hacen más grande o más pequeño.")
                    color: themeManager.textSecondary
                    font.pixelSize: 11
                }
            }

            // the timeline
            Item {
                id: timeline
                width: parent.width
                height: 50
                readonly property real perMs: width / root.duration
                function msAt(x) { return Math.max(0, Math.min(root.duration, x / perMs)); }

                Rectangle { y: 18; width: parent.width; height: 14; radius: 5; color: themeManager.border }
                Repeater {
                    model: root.pieces
                    delegate: Rectangle {
                        required property var modelData
                        x: modelData.from * timeline.perMs
                        y: 18
                        width: Math.max(1, (modelData.to - modelData.from) * timeline.perMs)
                        height: 14
                        radius: 4
                        color: modelData.removed ? "#7a3a45" : themeManager.accent
                        opacity: modelData.removed ? 0.65 : 1
                        border.color: themeManager.background
                        border.width: 1
                    }
                }
                // outside the chosen range
                Rectangle { y: 18; width: root.startMs * timeline.perMs; height: 14; color: "#000000"; opacity: 0.55 }
                Rectangle { x: root.endMs * timeline.perMs; y: 18; width: parent.width - x; height: 14; color: "#000000"; opacity: 0.55 }

                // click or drag on the bar: the cursor
                MouseArea {
                    anchors.fill: parent
                    onPressed: function (m) { root.setCursor(timeline.msAt(m.x)); }
                    onPositionChanged: function (m) { if (pressed) root.setCursor(timeline.msAt(m.x)); }
                }
                Rectangle {
                    x: root.cursorMs * timeline.perMs - 1; y: 8; width: 2; height: 34
                    color: "#f2c230"
                }
                // the two ends of the range
                Repeater {
                    model: 2
                    delegate: Rectangle {
                        id: handle
                        required property int index
                        readonly property bool isStart: index === 0
                        x: (isStart ? root.startMs : root.endMs) * timeline.perMs - width / 2
                        y: 12; width: 10; height: 26; radius: 3
                        color: themeManager.textPrimary
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -5
                            cursorShape: Qt.SizeHorCursor
                            onPositionChanged: function (m) {
                                if (!pressed) return;
                                const ms = Math.round(timeline.msAt(mapToItem(timeline, m.x, 0).x));
                                if (handle.isStart) { root.startMs = Math.max(0, Math.min(ms, root.endMs - root.minPieceMs)); root.setCursor(root.startMs); }
                                else { root.endMs = Math.min(root.duration, Math.max(ms, root.startMs + root.minPieceMs)); root.setCursor(Math.max(root.startMs, root.endMs - 60)); }
                            }
                        }
                    }
                }
            }

            Row {
                spacing: 8
                AppButton { text: qsTr("Cortar en el cursor"); onClicked: root.cutHere() }
                AppButton { text: qsTr("Deshacer cortes"); enabled: root.cuts.length > 0 || root.removedKeys.length > 0; onClicked: { root.cuts = []; root.removedKeys = []; } }
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("%1 → %2").arg(root.clock(root.startMs)).arg(root.clock(root.endMs))
                    color: themeManager.textSecondary
                }
            }

            // the pieces, each can be left out
            Flow {
                width: parent.width
                spacing: 6
                visible: root.pieces.length > 1
                Repeater {
                    model: root.pieces
                    delegate: Rectangle {
                        required property var modelData
                        height: 28
                        width: pieceRow.width + 16
                        radius: 8
                        color: "transparent"
                        border.color: themeManager.border
                        border.width: 1
                        opacity: modelData.removed ? 0.6 : 1
                        Row {
                            id: pieceRow
                            anchors.centerIn: parent
                            spacing: 8
                            Label {
                                text: root.clock(modelData.from) + " – " + root.clock(modelData.to) + " · " + root.secondsText(modelData.to - modelData.from)
                                color: themeManager.textPrimary
                                font.strikeout: modelData.removed
                            }
                            Label {
                                text: modelData.removed ? qsTr("Poner") : qsTr("Quitar")
                                color: themeManager.accent
                                TapHandler { onTapped: root.togglePiece(modelData.from) }
                                HoverHandler { cursorShape: Qt.PointingHandCursor }
                            }
                        }
                    }
                }
            }
        }
    }

    // ---------------------------------------------------------------- the right side: what is made
    Rectangle {
        id: side
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: bottomBar.top
        width: 330
        color: themeManager.surface
        visible: videoToGif.loaded
        Rectangle { anchors.left: parent.left; width: 1; height: parent.height; color: themeManager.border }

        Column {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 18

            Column {
                width: parent.width
                spacing: 8
                Label { text: qsTr("Fotogramas por segundo"); color: themeManager.textSecondary }
                AppChoiceChips {
                    model: root.fpsChoices.map(function (f) { return { text: String(f) }; })
                    currentIndex: root.fpsChoices.indexOf(root.fps)
                    onActivated: function (i) { root.fps = root.fpsChoices[i]; }
                }
                Label {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: qsTr("10 es lo habitual; 15 o 20 se ve más fluido y pesa más.")
                    color: themeManager.textSecondary
                    font.pixelSize: 11
                }
            }

            Column {
                width: parent.width
                spacing: 8
                Label { text: qsTr("Ancho de la imagen"); color: themeManager.textSecondary }
                AppChoiceChips {
                    readonly property var widths: [0, 1280, 720, 480, 320, 240, 160, 120]
                    model: [{ text: qsTr("Original") }, { text: "1280" }, { text: "720" }, { text: "480" }, { text: "320" }, { text: "240" }, { text: "160" }, { text: "120" }]
                    currentIndex: {
                        for (let i = 0; i < widths.length; ++i)
                            if ((widths[i] === 0 ? root.cropWidthPx : widths[i]) === root.outWidth) return i;
                        return -1;
                    }
                    onActivated: function (i) { root.chosenWidth = widths[i] === 0 ? root.cropWidthPx : widths[i]; root.widthChosen = true; }
                }
                Row {
                    spacing: 8
                    AppSpinBox {
                        from: 16; to: 4096
                        editable: true
                        value: root.outWidth
                        onValueModified: { root.chosenWidth = value; root.widthChosen = true; }
                    }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("px · alto %1 px").arg(root.outHeight)
                        color: themeManager.textSecondary
                    }
                }
                Label {
                    width: parent.width
                    visible: root.outWidth > root.cropWidthPx
                    wrapMode: Text.WordWrap
                    text: qsTr("El recorte tiene %1 px de ancho: a %2 px saldría borroso.").arg(root.cropWidthPx).arg(root.outWidth)
                    color: "#ffc25a"
                    font.pixelSize: 11
                }
            }

            Rectangle {
                width: parent.width
                height: summary.height + 24
                radius: themeManager.radiusMedium === 0 ? 0 : 10
                color: themeManager.surfaceElevated
                border.color: themeManager.border
                Column {
                    id: summary
                    x: 12; y: 12
                    width: parent.width - 24
                    spacing: 4
                    Label { text: qsTr("Se van a crear"); color: themeManager.textSecondary; font.pixelSize: 11 }
                    Label {
                        text: qsTr("%1 fotogramas · %2").arg(root.frameCount).arg(root.secondsText(root.keptMs))
                        color: themeManager.textPrimary
                        font.pixelSize: 16
                    }
                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        text: qsTr("GIF de %1 (aproximado)").arg(root.megabytes < 1 ? qsTr("menos de 1 MB") : qsTr("unos %1 MB").arg(root.megabytes < 10 ? root.megabytes.toFixed(1).replace(".", ",") : Math.round(root.megabytes)))
                        color: themeManager.textSecondary
                    }
                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        visible: text.length > 0
                        color: "#ffc25a"
                        font.pixelSize: 11
                        text: {
                            const w = [];
                            if (root.frameCount === 0) w.push(qsTr("No queda ningún tramo."));
                            if (root.tooMany) w.push(qsTr("Pasa de %1 fotogramas: baja los fotogramas por segundo o quita tramos.").arg(videoToGif.maxFrames));
                            if (root.keptMs > root.maxSeconds * 1000) w.push(qsTr("Más de %1 s: el GIF saldrá muy pesado.").arg(root.maxSeconds));
                            if (root.megabytes > root.maxMegabytes) w.push(qsTr("Pasa de %1 MB: reduce el ancho o los fotogramas por segundo.").arg(root.maxMegabytes));
                            return w.join(" ");
                        }
                    }
                }
            }
        }
    }

    // ---------------------------------------------------------------- the bottom
    Rectangle {
        id: bottomBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 52
        color: themeManager.surface
        visible: videoToGif.loaded
        Rectangle { width: parent.width; height: 1; color: themeManager.border }
        Label {
            anchors.left: parent.left; anchors.leftMargin: 18
            anchors.right: buttons.left; anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
            color: videoToGif.error === "read" ? "#ff8a8a" : themeManager.textSecondary
            text: videoToGif.error === "read" ? qsTr("El vídeo dejó de dar fotogramas. Prueba con otro tramo o con otro archivo.")
                                              : qsTr("Los fotogramas entran en la tira del estudio, donde puedes quitar, reordenar y ponerles texto o efectos.")
        }
        Row {
            id: buttons
            anchors.right: parent.right; anchors.rightMargin: 18
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            AppButton { text: qsTr("Cancelar"); onClicked: { videoToGif.close(); root.closeRequested(); } }
            AppButton {
                objectName: "createFramesButton"
                text: qsTr("Crear fotogramas")
                primary: true
                enabled: root.canCreate
                onClicked: videoToGif.create(root.kept, root.fps, crop.cx, crop.cy, crop.cw, crop.ch, root.outWidth)
            }
        }
    }

    // ---------------------------------------------------------------- reading the frames
    Rectangle {
        anchors.fill: parent
        visible: videoToGif.busy
        color: "#cc000000"
        MouseArea { anchors.fill: parent } // nothing under it can be touched
        Rectangle {
            anchors.centerIn: parent
            width: 380; height: 130
            radius: themeManager.radiusMedium === 0 ? 0 : 12
            color: themeManager.surface
            border.color: themeManager.border
            Column {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 10
                Label { text: qsTr("Leyendo fotogramas…"); color: themeManager.textPrimary; font.pixelSize: 15 }
                Label {
                    text: qsTr("%1 de %2").arg(Math.round(videoToGif.progress * root.frameCount)).arg(root.frameCount)
                    color: themeManager.textSecondary
                }
                AppProgressBar { width: parent.width; from: 0; to: 1; value: videoToGif.progress }
                AppButton { anchors.right: parent.right; text: qsTr("Cancelar"); onClicked: videoToGif.cancel() }
            }
        }
    }
}
