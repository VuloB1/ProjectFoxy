import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Effects
import ImageViewerApp

// "Varias imágenes": two to six pictures side by side, each in its own pane. Zoom and pan are LINKED by
// default - zooming or dragging in one pane moves them all, so the same spot can be compared across
// pictures - and the link can be switched off to look at each one on its own. Holding Shift while zooming
// or dragging does the opposite of the link for that gesture only (just this pane while linked, all of
// them while not). Every pane takes any file (RAW, HEIC... too): drop one on it to replace it.
Item {
    id: root

    readonly property int maxPanes: 6

    property bool linked: true
    // Panes one below the other instead of side by side.
    property bool stacked: false
    property int activeIndex: 0
    readonly property alias count: panes.count
    // What the last gesture left the view at, used when the link is switched back on.
    property real lastZoom: 1
    property real lastCx: 0.5
    property real lastCy: 0.5

    signal closeRequested()

    // ---- what is shown ----------------------------------------------------------------------------
    ListModel { id: panes }

    function currentFolderPaths() { return folderModel.filePaths(); }

    // Starts with the picture that is open and the next one in its folder.
    function begin() {
        panes.clear();
        const all = currentFolderPaths();
        const at = folderModel.currentIndex;
        if (at >= 0 && at < all.length) {
            panes.append({ path: all[at] });
            if (all.length > 1)
                panes.append({ path: all[(at + 1) % all.length] });
        }
        activeIndex = 0;
        resetViews();
    }

    function shown(path) {
        for (let i = 0; i < panes.count; ++i)
            if (panes.get(i).path === path)
                return true;
        return false;
    }

    function addPath(path) {
        if (panes.count >= maxPanes || path === "")
            return false;
        panes.append({ path: path });
        return true;
    }

    // Grows or shrinks to `n` panes: new ones show the pictures that follow the last one in the folder.
    function setCount(n) {
        n = Math.max(1, Math.min(maxPanes, n));
        while (panes.count > n)
            panes.remove(panes.count - 1);
        const all = currentFolderPaths();
        while (panes.count < n) {
            let from = panes.count > 0 ? all.indexOf(panes.get(panes.count - 1).path) : folderModel.currentIndex;
            let found = "";
            for (let step = 1; step <= all.length; ++step) {
                const candidate = all[(from + step + all.length) % all.length];
                if (!shown(candidate)) { found = candidate; break; }
            }
            panes.append({ path: found });   // an empty pane waits for a dropped file
            if (found === "")
                break;
        }
        if (activeIndex >= panes.count)
            activeIndex = panes.count - 1;
    }

    function urlToPath(url) {
        const s = url.toString();
        return decodeURIComponent(s.replace(/^file:\/\/\//, "").replace(/^file:\/\//, ""));
    }

    // Files dropped on pane `index`: the first replaces its picture, the rest take the panes after it.
    function dropped(index, urls) {
        let at = index;
        for (const u of urls) {
            const p = urlToPath(u);
            if (at < panes.count)
                panes.setProperty(at, "path", p);
            else if (!addPath(p))
                break;
            ++at;
        }
    }

    function paneItem(i) { return repeater.itemAt(i); }

    // ---- the views ----------------------------------------------------------------------------------
    function applyView(item, z, cx, cy) {
        item.zoom = z;
        item.cx = cx;
        item.cy = cy;
    }

    // `invert` is Shift: the opposite of what the link says, for this gesture.
    function userView(index, z, cx, cy, invert) {
        activeIndex = index;
        lastZoom = z; lastCx = cx; lastCy = cy;
        const together = root.linked !== invert;
        for (let i = 0; i < panes.count; ++i) {
            const item = paneItem(i);
            if (item && (i === index || together))
                applyView(item, z, cx, cy);
        }
    }

    function resetViews() {
        lastZoom = 1; lastCx = 0.5; lastCy = 0.5;
        for (let i = 0; i < panes.count; ++i) {
            const item = paneItem(i);
            if (item) applyView(item, 1, 0.5, 0.5);
        }
    }

    // Every picture at its own real pixels, around the middle (or, linked, around the shared point).
    function actualPixels() {
        for (let i = 0; i < panes.count; ++i) {
            const item = paneItem(i);
            if (!item) continue;
            item.cx = linked ? lastCx : item.cx;
            item.cy = linked ? lastCy : item.cy;
            item.actualPixels();
        }
    }

    function setLinked(on) {
        linked = on;
        if (on) {   // the others catch up with the pane that was used last
            for (let i = 0; i < panes.count; ++i) {
                const item = paneItem(i);
                if (item) applyView(item, lastZoom, lastCx, lastCy);
            }
        }
    }

    // ---- layout -------------------------------------------------------------------------------------
    readonly property int columns: {
        const n = Math.max(1, panes.count);
        if (n === 1) return 1;
        if (n === 2) return stacked ? 1 : 2;
        if (n === 3) return stacked ? 1 : 3;
        if (n === 4) return 2;
        return stacked ? 2 : 3;
    }
    readonly property int rowsCount: Math.max(1, Math.ceil(Math.max(1, panes.count) / columns))
    readonly property real gap: 4

    Rectangle { anchors.fill: parent; color: themeManager.background }

    Repeater {
        id: repeater
        model: panes
        delegate: PaneView {
            id: pv
            required property int index
            required path
            readonly property int col: index % root.columns
            readonly property int row: Math.floor(index / root.columns)
            x: col * (root.width + root.gap) / root.columns
            y: row * (root.height + root.gap) / root.rowsCount
            width: (root.width + root.gap) / root.columns - root.gap
            height: (root.height + root.gap) / root.rowsCount - root.gap
            active: root.activeIndex === index && panes.count > 1
            maxHighSide: panes.count <= 2 ? 16384 : (panes.count <= 4 ? 8192 : 6144)
            // a pane that appears late starts with the shared view
            Component.onCompleted: if (root.linked) root.applyView(pv, root.lastZoom, root.lastCx, root.lastCy)
            onUserView: function (z, cx, cy, invert) { root.userView(index, z, cx, cy, invert); }
            onPressed: root.activeIndex = index
            onCloseRequested: {
                panes.remove(index);
                if (panes.count === 0)
                    root.closeRequested();
            }
            onFileDropped: function (urls) { root.dropped(index, urls); }
        }
    }

    // ---- the bar over the panes -----------------------------------------------------------------------
    Item {
        id: bar
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 16
        anchors.horizontalCenter: parent.horizontalCenter
        width: barRow.implicitWidth + 24
        height: 48
        z: 5

        Rectangle {
            id: pill
            anchors.fill: parent
            radius: themeManager.radiusMedium === 0 ? 0 : height / 2
            color: themeManager.surfaceElevated
            border.color: themeManager.border
            border.width: 1
            MouseArea { anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.AllButtons }
        }
        MultiEffect {
            source: pill
            anchors.fill: pill
            shadowEnabled: true
            shadowColor: "#40000000"
            shadowBlur: 0.6
            shadowVerticalOffset: 3
        }

        Row {
            id: barRow
            anchors.centerIn: parent
            spacing: 2

            AppToolButton {
                text: root.linked ? qsTr("Zoom vinculado: activado (Shift mueve solo una)") : qsTr("Zoom vinculado: desactivado")
                iconName: root.linked ? "link" : "link-off"
                iconOnly: true
                checkable: true
                checked: root.linked
                onClicked: root.setLinked(!root.linked)
            }
            AppToolButton { text: qsTr("Ajustar todas a la ventana"); iconName: "fit"; iconOnly: true; onClicked: root.resetViews() }
            AppToolButton { text: qsTr("100 %"); onClicked: root.actualPixels(); AppToolTip { visible: parent.hovered; text: qsTr("Cada imagen en sus píxeles reales") } }

            Rectangle { width: 1; height: 24; color: themeManager.border; anchors.verticalCenter: parent.verticalCenter }

            AppToolButton { text: qsTr("Una imagen menos"); iconName: "minus"; iconOnly: true; enabled: panes.count > 1; onClicked: root.setCount(panes.count - 1) }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                width: 22
                horizontalAlignment: Text.AlignHCenter
                text: panes.count
                color: themeManager.textPrimary
                font.bold: true
            }
            AppToolButton { text: qsTr("Una imagen más"); iconName: "plus"; iconOnly: true; enabled: panes.count < root.maxPanes; onClicked: root.setCount(panes.count + 1) }
            AppToolButton {
                text: root.stacked ? qsTr("Una al lado de la otra") : qsTr("Una debajo de la otra")
                iconName: root.stacked ? "split-h" : "split-v"
                iconOnly: true
                enabled: panes.count >= 2 && panes.count !== 4
                onClicked: root.stacked = !root.stacked
            }

            Rectangle { width: 1; height: 24; color: themeManager.border; anchors.verticalCenter: parent.verticalCenter }

            AppToolButton { text: qsTr("Elegir imágenes…"); iconName: "open"; iconOnly: true; onClicked: pickDialog.open() }
            AppToolButton { text: qsTr("Volver a una sola imagen"); iconName: "close"; iconOnly: true; onClicked: root.closeRequested() }
        }
    }

    // A one-line hint under the bar, until the first gesture.
    Label {
        id: hint
        anchors.bottom: bar.top
        anchors.bottomMargin: 8
        anchors.horizontalCenter: parent.horizontalCenter
        z: 4
        visible: panes.count > 1 && opacity > 0
        opacity: hintShown ? 1 : 0
        property bool hintShown: true
        text: root.linked ? qsTr("Rueda para acercar, arrastrá para mover: todas se mueven juntas. Shift mueve solo una.")
                          : qsTr("El zoom no está vinculado: cada imagen se mueve por su cuenta. Shift mueve todas.")
        color: "#ffffff"
        font.pixelSize: 12
        padding: 6
        leftPadding: 12
        rightPadding: 12
        background: Rectangle { radius: themeManager.radiusSmall; color: Qt.rgba(0, 0, 0, 0.55) }
        Behavior on opacity { NumberAnimation { duration: 400 } }
        Timer { interval: 5000; running: hint.visible; onTriggered: hint.hintShown = false }
        Connections { target: root; function onLinkedChanged() { hint.hintShown = true; } }
    }

    FileDialog {
        id: pickDialog
        title: qsTr("Elegí las imágenes que querés ver juntas")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Imágenes (%1)").arg(appController.supportedExtensions.join(" "))]
        onAccepted: {
            panes.clear();
            for (const u of selectedFiles) {
                if (!root.addPath(root.urlToPath(u)))
                    break;
            }
            root.activeIndex = 0;
            root.resetViews();
            if (panes.count === 0)
                root.closeRequested();
        }
    }
}
