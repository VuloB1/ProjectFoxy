import QtQuick
import QtQuick.Controls
import ImageViewerApp

// One picture of the "Varias imágenes" view: it shows a file, and zooms and pans on its own - the
// MultiView around it decides whether the other panes follow.
//
// The view is kept as numbers that mean the same for any picture: `zoom` (1 = the picture fitted in the
// pane, 2 = twice as big...) and the point of the picture, as fractions of its width and height
// (`cx`, `cy`), that sits in the middle of the pane. Panes showing pictures of other sizes, or of other
// proportions, can then share them and still look at the same part of their picture.
Item {
    id: pane
    clip: true

    property string path: ""
    property bool active: false
    // The most pixels along a side this pane may ask for when it is zoomed in far.
    property int maxHighSide: 8192

    property real zoom: 1
    property real cx: 0.5
    property real cy: 0.5
    readonly property real maxZoom: 64

    // The picture's real size once it is known (0 until it has been decoded once).
    property size srcSize: Qt.size(0, 0)

    // The picture's proportions, from what has loaded so far.
    readonly property real aspect: lowImage.implicitWidth > 0 && lowImage.implicitHeight > 0
                                   ? lowImage.implicitWidth / lowImage.implicitHeight : 1.5
    readonly property real fitW: aspect >= width / Math.max(1, height) ? width : height * aspect
    readonly property real fitH: fitW / aspect
    readonly property real shownW: fitW * zoom
    readonly property real shownH: fitH * zoom
    // 1 = the picture's own pixels, one for one.
    readonly property real screenScale: srcSize.width > 0 ? shownW / srcSize.width : 0
    // Where the picture really is shown: the wanted centre, kept so that no empty margin opens up.
    readonly property real cxEff: clampCenter(cx, shownW, width)
    readonly property real cyEff: clampCenter(cy, shownH, height)
    readonly property bool loading: lowImage.status === Image.Loading || highImage.status === Image.Loading
    readonly property bool failed: path !== "" && lowImage.status === Image.Error

    // The user changed the view of this pane (wheel, drag, double click); `invert` is set while Shift is
    // held, which means "only this one" when the panes are linked and "all of them" when they are not.
    signal userView(real zoom, real cx, real cy, bool invert)
    signal pressed()
    signal closeRequested()
    signal fileDropped(var urls)

    function clampCenter(c, shown, view) {
        if (shown <= view + 0.5)
            return 0.5;
        const half = view / 2 / shown;
        return Math.min(1 - half, Math.max(half, c));
    }

    function zoomAt(px, py, factor, invert) {
        const nz = Math.min(maxZoom, Math.max(1, zoom * factor));
        if (nz === zoom)
            return;
        // the point of the picture under the cursor stays under it
        const u = cxEff + (px - width / 2) / shownW;
        const v = cyEff + (py - height / 2) / shownH;
        const ncx = u - (px - width / 2) / (fitW * nz);
        const ncy = v - (py - height / 2) / (fitH * nz);
        userView(nz, ncx, ncy, invert);
    }

    function panBy(dx, dy, invert) {
        if (shownW <= width + 0.5 && shownH <= height + 0.5)
            return;
        userView(zoom, cxEff - dx / shownW, cyEff - dy / shownH, invert);
    }

    // The real pixels, one for one, around the middle of the pane.
    function actualPixels() {
        if (srcSize.width <= 0 || fitW <= 0)
            return;
        zoom = Math.min(maxZoom, Math.max(1, srcSize.width / fitW));
    }

    // ---- the picture: a modest copy that is always there, and the sharper one that comes in on top when zoomed in far
    readonly property string baseUrl: path !== "" ? "image://pane/" + encodeURIComponent(path) : ""
    property int highSide: 0   // 0 = none wanted yet
    Timer {
        id: tierTimer
        interval: 200
        onTriggered: {
            const dpr = Screen.devicePixelRatio;
            const need = Math.max(pane.shownW, pane.shownH) * dpr;
            const have = Math.max(lowImage.implicitWidth, lowImage.implicitHeight);
            if (have <= 0 || need <= have * 0.95)
                return;
            let side = 4096;
            while (side < need && side < pane.maxHighSide)
                side *= 2;
            side = Math.min(side, pane.maxHighSide);
            // never more than the file has
            if (pane.srcSize.width > 0)
                side = Math.min(side, Math.max(pane.srcSize.width, pane.srcSize.height));
            if (side > pane.highSide && side > have)
                pane.highSide = side;
        }
    }
    onShownWChanged: tierTimer.restart()
    onPathChanged: { highSide = 0; srcSize = Qt.size(0, 0); refreshSize(); }
    function refreshSize() { if (path !== "") srcSize = paneImages.sourceSize(path); }
    Component.onCompleted: refreshSize()
    Connections {
        target: paneImages
        function onSourceSizeKnown(p) { if (p === pane.path) pane.refreshSize(); }
    }

    Image {
        id: lowImage
        source: pane.baseUrl
        sourceSize: Qt.size(2048, 2048)
        asynchronous: true
        mipmap: true
        smooth: !appSettings.pixelMode && pane.screenScale < 2
        width: pane.shownW
        height: pane.shownH
        x: pane.width / 2 - pane.cxEff * pane.shownW
        y: pane.height / 2 - pane.cyEff * pane.shownH
    }
    Image {
        id: highImage
        source: pane.highSide > 0 ? pane.baseUrl + "?s=" + pane.highSide : ""
        sourceSize: Qt.size(pane.highSide, pane.highSide)
        asynchronous: true
        cache: false
        mipmap: true
        smooth: lowImage.smooth
        visible: status === Image.Ready
        width: lowImage.width
        height: lowImage.height
        x: lowImage.x
        y: lowImage.y
    }

    // ---- empty / failed / loading
    Label {
        anchors.centerIn: parent
        visible: pane.path === ""
        text: qsTr("Soltá una imagen acá")
        color: themeManager.textSecondary
    }
    Label {
        anchors.centerIn: parent
        visible: pane.failed
        text: qsTr("No se pudo abrir esta imagen")
        color: themeManager.textSecondary
    }
    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        visible: pane.loading && lowImage.status !== Image.Ready
        text: qsTr("Cargando…")
        color: themeManager.accent
    }

    // ---- the mouse
    MouseArea {
        id: mouse
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
        hoverEnabled: true
        cursorShape: dragging ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        property bool dragging: false
        property real lastX: 0
        property real lastY: 0
        onPressed: function (m) {
            pane.pressed();
            dragging = true;
            lastX = m.x;
            lastY = m.y;
        }
        onPositionChanged: function (m) {
            if (!dragging)
                return;
            pane.panBy(m.x - lastX, m.y - lastY, (m.modifiers & Qt.ShiftModifier) !== 0);
            lastX = m.x;
            lastY = m.y;
        }
        onReleased: dragging = false
        onCanceled: dragging = false
        onDoubleClicked: function (m) { pane.userView(1, 0.5, 0.5, (m.modifiers & Qt.ShiftModifier) !== 0); }
        onWheel: function (w) {
            pane.pressed();
            const d = w.angleDelta.y !== 0 ? w.angleDelta.y : w.angleDelta.x;
            pane.zoomAt(w.x, w.y, Math.pow(1.0016, d), (w.modifiers & Qt.ShiftModifier) !== 0);
        }
    }

    // ---- name, size and close button, over the picture
    Rectangle {
        id: caption
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 8
        visible: pane.path !== ""
        height: 24
        width: Math.min(pane.width - 52, captionText.implicitWidth + 16)
        radius: themeManager.radiusSmall
        color: Qt.rgba(0, 0, 0, 0.55)
        Label {
            id: captionText
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideMiddle
            color: "#ffffff"
            font.pixelSize: 12
            text: {
                const name = pane.path.replace(/^.*[\\/]/, "");
                return pane.srcSize.width > 0 ? name + "  ·  " + pane.srcSize.width + " × " + pane.srcSize.height : name;
            }
        }
    }
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        width: 24
        height: 24
        radius: themeManager.radiusSmall
        color: closeArea.containsMouse ? Qt.rgba(0.9, 0.2, 0.2, 0.85) : Qt.rgba(0, 0, 0, 0.55)
        visible: pane.path !== ""
        AppIcon { anchors.centerIn: parent; name: "close"; size: 14; color: "#ffffff" }
        MouseArea {
            id: closeArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: pane.closeRequested()
        }
        AppToolTip { visible: closeArea.containsMouse; text: qsTr("Quitar esta imagen") }
    }
    // the zoom, so it is clear when the panes differ
    Rectangle {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 8
        visible: pane.path !== "" && pane.zoom > 1.001
        height: 22
        width: zoomText.implicitWidth + 14
        radius: themeManager.radiusSmall
        color: Qt.rgba(0, 0, 0, 0.55)
        Label {
            id: zoomText
            anchors.centerIn: parent
            color: "#ffffff"
            font.pixelSize: 11
            text: pane.screenScale > 0 ? Math.round(pane.screenScale * 100) + "%" : "×" + pane.zoom.toFixed(1)
        }
    }

    DropArea {
        anchors.fill: parent
        onDropped: function (drop) {
            if (drop.hasUrls)
                pane.fileDropped(drop.urls);
        }
    }

    // the pane the last gesture was in
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: pane.active ? 2 : 0
        border.color: themeManager.accent
        radius: 0
    }
}
