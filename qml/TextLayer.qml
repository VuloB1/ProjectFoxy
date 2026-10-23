import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Text of the picture, as in Windows' Photos: what is not text goes a little darker, the words can be selected by dragging
// (or with a double click, or Ctrl+A) and copied with Ctrl+C or the right-click menu. The words come from `textRecognizer`
// (boxes in picture pixels); `picture` is the Image item of ImageCanvas, whose scale and position already follow the
// zoom and the pan, and `viewport` its Flickable, to know when the view moved.
Item {
    id: root

    property Item picture: null
    property Item viewport: null
    readonly property var words: textRecognizer.words
    // the selection: indices into `words`, in reading order
    property int anchorWord: -1
    property int activeWord: -1
    readonly property int firstSelected: Math.min(anchorWord, activeWord)
    readonly property int lastSelected: Math.max(anchorWord, activeWord)
    readonly property bool hasSelection: anchorWord >= 0 && activeWord >= 0
    signal exitRequested()

    function selectedText() {
        if (!hasSelection)
            return "";
        let out = "";
        for (let i = firstSelected; i <= lastSelected; ++i) {
            const w = words[i];
            if (i > firstSelected)
                out += w.line !== words[i - 1].line ? "\n" : (w.joined ? "" : " ");
            out += w.text;
        }
        return out;
    }
    function copySelection() {
        if (hasSelection)
            textRecognizer.copyText(selectedText());
    }
    function selectAll() {
        if (words.length > 0) {
            anchorWord = 0;
            activeWord = words.length - 1;
        }
    }
    function clearSelection() {
        anchorWord = -1;
        activeWord = -1;
    }

    // Squared distance, in picture pixels, from (px, py) to the box of word i (0 when inside).
    function distanceTo(i, px, py) {
        const w = words[i];
        const dx = Math.max(w.x - px, 0, px - (w.x + w.w));
        const dy = Math.max(w.y - py, 0, py - (w.y + w.h));
        return dx * dx + dy * dy;
    }
    // The word under (or nearest to) a point of this item; -1 when none is closer than `reach` screen pixels.
    function wordAt(mx, my, reach) {
        if (!picture || words.length === 0)
            return -1;
        const p = picture.mapFromItem(root, mx, my);
        const limit = reach / Math.max(0.01, picture.scale);
        let best = -1, bestD = limit * limit;
        for (let i = 0; i < words.length; ++i) {
            const d = distanceTo(i, p.x, p.y);
            if (d <= bestD) {
                bestD = d;
                best = i;
            }
        }
        return best;
    }

    onWordsChanged: { clearSelection(); canvas.requestPaint() }
    onAnchorWordChanged: canvas.requestPaint()
    onActiveWordChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()
    Connections {
        target: root.picture
        function onScaleChanged() { canvas.requestPaint() }
        function onWidthChanged() { canvas.requestPaint() }
        function onRotationChanged() { canvas.requestPaint() }
    }
    Connections {
        target: root.viewport
        function onContentXChanged() { canvas.requestPaint() }
        function onContentYChanged() { canvas.requestPaint() }
        function onContentWidthChanged() { canvas.requestPaint() }
        function onContentHeightChanged() { canvas.requestPaint() }
    }
    Connections {
        target: themeManager
        function onThemeChanged() { canvas.requestPaint() }
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            const pic = root.picture;
            if (!pic || pic.width <= 0)
                return;
            // the picture's rectangle on this item (it is not rotated while a text is being read)
            const a = pic.mapToItem(root, 0, 0);
            const b = pic.mapToItem(root, pic.width, pic.height);
            const x0 = Math.min(a.x, b.x), y0 = Math.min(a.y, b.y);
            ctx.fillStyle = "rgba(0, 0, 0, 0.45)";
            ctx.fillRect(x0, y0, Math.abs(b.x - a.x), Math.abs(b.y - a.y));
            const pad = 2;
            const words = root.words;
            // the words are left as they are in the picture: a hole in the veil
            ctx.globalCompositeOperation = "destination-out";
            ctx.fillStyle = "#000000";
            for (let i = 0; i < words.length; ++i) {
                const w = words[i];
                const p = pic.mapToItem(root, w.x, w.y);
                const q = pic.mapToItem(root, w.x + w.w, w.y + w.h);
                ctx.fillRect(p.x - pad, p.y - pad, q.x - p.x + 2 * pad, q.y - p.y + 2 * pad);
            }
            ctx.globalCompositeOperation = "source-over";
            if (root.hasSelection) {
                const c = themeManager.accent;
                ctx.fillStyle = Qt.rgba(c.r, c.g, c.b, 0.45);
                for (let i = root.firstSelected; i <= root.lastSelected; ++i) {
                    const w = words[i];
                    const p = pic.mapToItem(root, w.x, w.y);
                    const q = pic.mapToItem(root, w.x + w.w, w.y + w.h);
                    ctx.fillRect(p.x - pad, p.y - pad, q.x - p.x + 2 * pad, q.y - p.y + 2 * pad);
                }
            }
        }
    }

    // A press on a word starts a selection; anywhere else it falls through to the picture (so it can still be dragged).
    MouseArea {
        id: area
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true
        cursorShape: root.wordAt(mouseX, mouseY, 6) >= 0 ? Qt.IBeamCursor : Qt.ArrowCursor
        onPressed: function (mouse) {
            const i = root.wordAt(mouse.x, mouse.y, 6);
            if (mouse.button === Qt.RightButton) {
                if (i >= 0 && (!root.hasSelection || i < root.firstSelected || i > root.lastSelected)) {
                    root.anchorWord = i;
                    root.activeWord = i;
                }
                textMenu.popup();
                return;
            }
            if (i < 0) {
                root.clearSelection();
                mouse.accepted = false;
                return;
            }
            root.anchorWord = i;
            root.activeWord = i;
        }
        onPositionChanged: function (mouse) {
            if (!pressed || !(pressedButtons & Qt.LeftButton))
                return;
            const i = root.wordAt(mouse.x, mouse.y, 100000);
            if (i >= 0)
                root.activeWord = i;
        }
        onDoubleClicked: function (mouse) {
            const i = root.wordAt(mouse.x, mouse.y, 6);
            if (i >= 0) {
                root.anchorWord = i;
                root.activeWord = i;
            }
        }
    }

    AppMenu {
        id: textMenu
        AppMenuItem {
            iconName: "copy"
            text: qsTr("Copiar texto")
            enabled: root.hasSelection
            onTriggered: root.copySelection()
        }
        AppMenuItem {
            iconName: "text"
            text: qsTr("Seleccionar todo el texto")
            enabled: root.words.length > 0
            onTriggered: root.selectAll()
        }
        AppMenuSeparator {}
        AppMenuItem {
            iconName: "close"
            text: qsTr("Salir del reconocimiento de texto")
            onTriggered: root.exitRequested()
        }
    }

    Shortcut { sequence: StandardKey.Copy; enabled: root.visible && root.hasSelection; onActivated: root.copySelection() }
    Shortcut { sequence: StandardKey.SelectAll; enabled: root.visible; onActivated: root.selectAll() }
    Shortcut { sequence: "Escape"; enabled: root.visible; onActivated: root.exitRequested() }
}
