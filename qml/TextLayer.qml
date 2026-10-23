import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Text of the picture, as in Windows' Photos: what is not text goes a little darker and the lines (a paragraph's lines
// run together into one shape) stay as they are, with the corners of the theme. The words can be selected by dragging
// (a double click takes the line, a triple click the paragraph, Ctrl+A everything) and copied with Ctrl+C or the
// right-click menu. The words come from `textRecognizer` (boxes in picture pixels, with their line and paragraph);
// `picture` is the Image item of ImageCanvas, whose scale and position already follow the zoom and the pan, and
// `viewport` its Flickable, to know when the view moved.
Item {
    id: root

    property Item picture: null
    property Item viewport: null
    property real radius: 8
    readonly property var words: textRecognizer.words
    // one entry per line: its box in picture pixels, its words (first..last) and its paragraph
    property var lines: []
    // the selection: indices into `words`, in reading order
    property int anchorWord: -1
    property int activeWord: -1
    readonly property int firstSelected: Math.min(anchorWord, activeWord)
    readonly property int lastSelected: Math.max(anchorWord, activeWord)
    readonly property bool hasSelection: anchorWord >= 0 && activeWord >= 0
    signal exitRequested()

    function buildLines() {
        const out = [];
        for (let i = 0; i < words.length; ++i) {
            const w = words[i];
            const last = out.length > 0 ? out[out.length - 1] : null;
            if (last && last.line === w.line) {
                last.x1 = Math.max(last.x1, w.x + w.w);
                last.y1 = Math.max(last.y1, w.y + w.h);
                last.y0 = Math.min(last.y0, w.y);
                last.last = i;
            } else {
                out.push({ line: w.line, para: w.para, x0: w.x, y0: w.y, x1: w.x + w.w, y1: w.y + w.h, first: i, last: i });
            }
        }
        // the shape of each line: a little air around the letters, and the lines of one paragraph meeting halfway
        for (const l of out) {
            const h = l.y1 - l.y0;
            l.t = l.y0 - 0.15 * h;
            l.b = l.y1 + 0.15 * h;
            l.l = l.x0 - 0.3 * h;
            l.r = l.x1 + 0.3 * h;
        }
        for (let i = 0; i + 1 < out.length; ++i) {
            if (out[i].para !== out[i + 1].para)
                continue;
            const mid = (out[i].y1 + out[i + 1].y0) / 2;
            out[i].b = Math.max(out[i].b, mid);
            out[i + 1].t = Math.min(out[i + 1].t, mid);
        }
        lines = out;
    }

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
    // the words of the line / of the paragraph that word i belongs to
    function selectLineOf(i) {
        for (const l of lines)
            if (i >= l.first && i <= l.last) {
                anchorWord = l.first;
                activeWord = l.last;
                return;
            }
    }
    function selectParagraphOf(i) {
        let first = -1, last = -1;
        for (const l of lines)
            if (l.para === words[i].para) {
                if (first < 0)
                    first = l.first;
                last = l.last;
            }
        anchorWord = first;
        activeWord = last;
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

    onWordsChanged: { buildLines(); clearSelection(); canvas.requestPaint() }
    onAnchorWordChanged: canvas.requestPaint()
    onActiveWordChanged: canvas.requestPaint()
    onRadiusChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()
    onVisibleChanged: if (visible) fadeIn.restart()
    NumberAnimation { id: fadeIn; target: root; property: "opacity"; from: 0; to: 1; duration: themeManager.animMedium; easing.type: themeManager.easingCurve }
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
        function shape(ctx, pic, x0, y0, x1, y1) {
            const p = pic.mapToItem(root, x0, y0);
            const q = pic.mapToItem(root, x1, y1);
            const w = q.x - p.x, h = q.y - p.y;
            const r = Math.min(root.radius, h / 2, w / 2);
            if (r > 0.5)
                ctx.roundedRect(p.x, p.y, w, h, r, r);
            else
                ctx.rect(p.x, p.y, w, h);
        }
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            const pic = root.picture;
            const lines = root.lines;
            // nothing to show until the words are known (the veil without them would only darken the picture)
            if (!pic || pic.width <= 0 || lines.length === 0)
                return;
            // the picture's rectangle on this item (it is not rotated while a text is being read)
            const a = pic.mapToItem(root, 0, 0);
            const b = pic.mapToItem(root, pic.width, pic.height);
            ctx.fillStyle = "rgba(0, 0, 0, 0.5)";
            ctx.fillRect(Math.min(a.x, b.x), Math.min(a.y, b.y), Math.abs(b.x - a.x), Math.abs(b.y - a.y));
            // the lines are left as they are in the picture: a hole in the veil
            ctx.globalCompositeOperation = "destination-out";
            ctx.fillStyle = "#000000";
            ctx.beginPath();
            for (const l of lines)
                shape(ctx, pic, l.l, l.t, l.r, l.b);
            ctx.fill();
            ctx.globalCompositeOperation = "source-over";
            if (!root.hasSelection)
                return;
            const c = themeManager.accent;
            ctx.fillStyle = Qt.rgba(c.r, c.g, c.b, 0.42);
            ctx.beginPath();
            for (const l of lines) {
                const from = Math.max(l.first, root.firstSelected), to = Math.min(l.last, root.lastSelected);
                if (from > to)
                    continue;
                const w0 = root.words[from], w1 = root.words[to];
                const h = l.y1 - l.y0;
                shape(ctx, pic, from === l.first ? l.l : w0.x - 0.15 * h, l.t, to === l.last ? l.r : w1.x + w1.w + 0.15 * h, l.b);
            }
            ctx.fill();
        }
    }

    // A press on a word starts a selection; anywhere else it falls through to the picture (so it can still be dragged).
    MouseArea {
        id: area
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true
        cursorShape: root.wordAt(mouseX, mouseY, 6) >= 0 ? Qt.IBeamCursor : Qt.ArrowCursor
        property double lastPress: 0
        property int lastWord: -1
        property int presses: 0
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
            const now = Date.now();
            presses = (i === lastWord && now - lastPress < Qt.styleHints.mouseDoubleClickInterval) ? presses + 1 : 1;
            lastPress = now;
            lastWord = i;
            if (presses === 2) {
                root.selectLineOf(i);
            } else if (presses >= 3) {
                root.selectParagraphOf(i);
            } else {
                root.anchorWord = i;
                root.activeWord = i;
            }
        }
        onPositionChanged: function (mouse) {
            if (!pressed || !(pressedButtons & Qt.LeftButton) || presses > 1)
                return;
            const i = root.wordAt(mouse.x, mouse.y, 100000);
            if (i >= 0)
                root.activeWord = i;
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

    Shortcut { sequences: [StandardKey.Copy]; enabled: root.visible && root.hasSelection; onActivated: root.copySelection() }
    Shortcut { sequences: [StandardKey.SelectAll]; enabled: root.visible; onActivated: root.selectAll() }
    Shortcut { sequence: "Escape"; enabled: root.visible; onActivated: root.exitRequested() }
}
