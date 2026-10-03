import QtQuick
import QtQuick.Controls
import ImageViewerApp

// The files of the open folder as a list to pick from, for the batch dialogs: a check box,
// a small preview and the name on every row, a box to filter by name, and Todos / Ninguno /
// Invertir. Click a row to tick it, Shift+click to tick (or untick) everything between it and
// the last row clicked. The "Todos / Ninguno / Invertir" buttons act on the rows that are
// showing, so with a filter typed they pick "all the *.png".
Item {
    id: root

    // path -> true for the ticked files. Always reassigned (never edited in place) so the
    // rows and the counter notice.
    property var selected: ({})
    property int selectedCount: 0
    property string filter: ""
    // [{ path, name, thumb }] of the whole folder, then the part that matches the filter.
    property var rows: []
    property var shownRows: []
    property int lastClicked: -1

    // Something changed in the selection (a tick, a button, a new list).
    signal selectionChanged()

    // The ticked files, in the folder's order.
    function selectedPaths() {
        const out = [];
        for (const r of rows)
            if (selected[r.path] === true)
                out.push(r.path);
        return out;
    }

    // (Re)reads the folder; every file starts ticked or not.
    function reset(tickAll) {
        const list = [];
        for (const p of folderModel.filePaths()) {
            const name = p.substring(p.lastIndexOf("/") + 1);
            list.push({ path: p, name: name, thumb: "image://thumb/" + encodeURIComponent(p) });
        }
        rows = list;
        filter = "";
        searchField.text = "";
        lastClicked = -1;
        const sel = {};
        if (tickAll)
            for (const r of list)
                sel[r.path] = true;
        selected = sel;
        applyFilter();
    }

    function applyFilter() {
        const f = filter.trim().toLowerCase();
        shownRows = f === "" ? rows : rows.filter(function (r) { return r.name.toLowerCase().indexOf(f) >= 0; });
        recount();
    }

    function recount() {
        let n = 0;
        for (const r of rows)
            if (selected[r.path] === true)
                ++n;
        selectedCount = n;
        selectionChanged();
    }

    function tickShown(mode) { // "all" | "none" | "invert"
        const sel = Object.assign({}, selected);
        for (const r of shownRows) {
            if (mode === "all") sel[r.path] = true;
            else if (mode === "none") delete sel[r.path];
            else if (sel[r.path] === true) delete sel[r.path];
            else sel[r.path] = true;
        }
        selected = sel;
        recount();
    }

    function clickRow(index, shift) {
        const sel = Object.assign({}, selected);
        const path = shownRows[index].path;
        if (shift && lastClicked >= 0 && lastClicked < shownRows.length) {
            const on = sel[shownRows[lastClicked].path] === true; // copy the state of the row it started from
            for (let i = Math.min(index, lastClicked); i <= Math.max(index, lastClicked); ++i) {
                if (on) sel[shownRows[i].path] = true;
                else delete sel[shownRows[i].path];
            }
        } else if (sel[path] === true) {
            delete sel[path];
        } else {
            sel[path] = true;
        }
        lastClicked = index;
        selected = sel;
        recount();
    }

    onFilterChanged: applyFilter()

    // ---- top: filter and the quick buttons --------------------------------------
    Column {
        id: top
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 8

        AppTextField {
            id: searchField
            width: parent.width
            placeholderText: qsTr("Buscar por nombre…")
            onTextChanged: root.filter = text
        }
        Row {
            spacing: 6
            AppToolButton { hPad: 10; text: qsTr("Todos"); onClicked: root.tickShown("all") }
            AppToolButton { hPad: 10; text: qsTr("Ninguno"); onClicked: root.tickShown("none") }
            AppToolButton { hPad: 10; text: qsTr("Invertir"); onClicked: root.tickShown("invert") }
        }
    }

    // ---- the list -------------------------------------------------------------
    Rectangle {
        id: frame
        anchors.top: top.bottom
        anchors.topMargin: 8
        anchors.bottom: counter.top
        anchors.bottomMargin: 8
        anchors.left: parent.left
        anchors.right: parent.right
        radius: themeManager.radiusMedium === 0 ? 0 : 8
        color: themeManager.surface
        border.color: themeManager.border
        border.width: 1
        clip: true

        ListView {
            id: list
            anchors.fill: parent
            anchors.margins: 4
            model: root.shownRows
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: AppScrollBar {}
            AppWheelScroll { view: list; step: 100 }

            delegate: Rectangle {
                id: row
                required property int index
                required property var modelData
                readonly property bool ticked: root.selected[modelData.path] === true

                width: list.width
                height: 50
                radius: themeManager.radiusSmall
                color: rowHover.hovered ? Qt.rgba(themeManager.textPrimary.r, themeManager.textPrimary.g, themeManager.textPrimary.b, 0.08)
                       : ticked ? Qt.rgba(themeManager.accent.r, themeManager.accent.g, themeManager.accent.b, 0.12) : "transparent"

                HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
                TapHandler {
                    onTapped: function (eventPoint, button) {
                        root.clickRow(row.index, (point.modifiers & Qt.ShiftModifier) !== 0);
                    }
                }

                // the tick box
                Rectangle {
                    id: box
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    width: 20
                    height: 20
                    radius: themeManager.radiusSmall
                    color: row.ticked ? themeManager.accent : "transparent"
                    border.width: 1
                    border.color: row.ticked ? themeManager.accent : themeManager.textSecondary
                    AppIcon {
                        anchors.centerIn: parent
                        visible: row.ticked
                        name: "check"
                        size: 14
                        strokeWidth: 2.6
                        color: themeManager.accentText
                    }
                }

                // a small preview on a plain tile (a transparent picture shows its see-through parts)
                Rectangle {
                    id: tile
                    anchors.left: box.right
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    width: 38
                    height: 38
                    radius: themeManager.radiusSmall
                    color: themeManager.surfaceElevated
                    clip: true
                    Image {
                        anchors.fill: parent
                        anchors.margins: 1
                        source: row.modelData.thumb
                        sourceSize: Qt.size(96, 96)
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        cache: false
                    }
                }

                Label {
                    anchors.left: tile.right
                    anchors.leftMargin: 12
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.modelData.name
                    color: themeManager.textPrimary
                    elide: Text.ElideMiddle
                }
            }

            Label {
                anchors.centerIn: parent
                visible: root.shownRows.length === 0
                text: root.rows.length === 0 ? qsTr("No hay imágenes en la carpeta.") : qsTr("Ningún archivo coincide.")
                color: themeManager.textSecondary
            }
        }
    }

    Label {
        id: counter
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        text: qsTr("%1 de %2 elegidos").arg(root.selectedCount).arg(root.rows.length)
        color: root.selectedCount > 0 ? themeManager.textPrimary : themeManager.textSecondary
        font.bold: true
    }
    Label {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        text: qsTr("Mayús + clic: un tramo")
        color: themeManager.textSecondary
        font.pixelSize: 11
    }
}
