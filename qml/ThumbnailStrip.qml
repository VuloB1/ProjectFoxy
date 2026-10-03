import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Vertical filmstrip sidebar for the current folder, backed by the C++
// FolderModel (QAbstractListModel over ThumbnailCache/ThumbnailImageProvider).
// The panel is exactly as wide as its thumbnails plus an even margin: `columns` columns of
// `cell`-pixel thumbnails (both in Configuración > Apariencia). Dragging the panel's right
// edge with the mouse makes the thumbnails bigger or smaller (the width follows the mouse; the
// size is stored when the button is released). The collapse control is a small tab attached to
// the panel's right edge and sticking out over the canvas, as if it were a bump of the border;
// collapsing slides the panel away to nothing (the thumbnails travel with its edge at their
// normal size instead of being squeezed) and leaves just the tab against the window's edge,
// so the folder position isn't lost.
//
// When the panel is open is up to appSettings.stripMode: "manual" (the tab decides), "open"
// (always, no tab), "closed" (never, no tab) or "auto" (closed while editing, during the
// slideshow and in narrow windows; the tab can still flip it until the situation changes).
Item {
    id: root

    // Set by the window: true while editing or running the slideshow.
    property bool busy: false

    readonly property string mode: appSettings.stripMode
    readonly property int margin: 8
    readonly property int gap: 6
    readonly property int cols: appSettings.stripColumns
    readonly property int minCell: 48
    // While the edge is being dragged this holds the size the thumbnails have right now.
    property int liveCell: -1
    readonly property int cell: Math.min(maxCell, liveCell > 0 ? liveCell : appSettings.stripThumbSize)
    // The panel never takes more than half of the window.
    readonly property int maxCell: Math.max(minCell, Math.min(200,
        Math.floor(((parent ? parent.width : 1000) * 0.5 - 2 * margin - (cols - 1) * gap) / cols)))
    readonly property int expandedWidth: 2 * margin + cols * cell + (cols - 1) * gap
    // Thumbnails are cut at 160 px, or at 256 when they are shown bigger than ~110.
    readonly property int edge: appSettings.stripThumbSize > 110 ? 256 : 160

    // --- open / closed ------------------------------------------------------------------
    property bool manualCollapsed: false   // "manual": what the tab says
    property bool flipped: false           // "auto": the tab overrode the automatic choice
    readonly property bool autoSuggestsClosed: busy || (parent ? parent.width < 900 : false)
    readonly property bool collapsed: mode === "open" ? false
                                      : mode === "closed" ? true
                                      : mode === "auto" ? (autoSuggestsClosed !== flipped)
                                      : manualCollapsed
    readonly property bool tabShown: mode === "manual" || mode === "auto"
    // a new situation (editing starts, the window narrows...) puts the automatic choice back
    onAutoSuggestsClosedChanged: flipped = false
    onModeChanged: flipped = false

    width: collapsed ? 0 : expandedWidth
    // The tab hangs outside this item's own bounds, so it has to paint above
    // the canvas that comes after it in the window.
    z: 3

    Behavior on width {
        enabled: root.liveCell < 0 // follow the mouse exactly while dragging
        NumberAnimation { duration: themeManager.animMedium; easing.type: themeManager.easingCurve }
    }

    AppPanelBackground {
        anchors.fill: parent
        visible: root.width > 0.5
        clip: true

        GridView {
            id: list
            // Pinned to the panel's right edge: while the panel grows or shrinks the
            // thumbnails slide in and out with it.
            x: root.margin + (root.width - root.expandedWidth)
            y: root.margin
            width: root.cols * cellWidth
            height: parent.height - 2 * root.margin
            cellWidth: root.cell + root.gap
            cellHeight: root.cell + root.gap
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: folderModel

            AppWheelScroll { view: list; step: 100 }

            // Keep the current thumbnail in view as folderModel.currentIndex
            // changes from arrow keys / floating-bar prev-next, not just clicks.
            currentIndex: folderModel.currentIndex
            onCurrentIndexChanged: positionViewAtIndex(currentIndex, GridView.Contain)
            onCellHeightChanged: Qt.callLater(function () { list.positionViewAtIndex(list.currentIndex, GridView.Contain); })

            delegate: Item {
                id: slot
                required property int index
                required property string fileName
                required property string thumbnailSource
                required property bool isCurrent

                width: list.cellWidth
                height: list.cellHeight

                Rectangle {
                    width: root.cell
                    height: root.cell
                    radius: themeManager.radiusSmall
                    color: slot.isCurrent ? themeManager.accent
                           : cellHover.hovered ? Qt.tint(themeManager.surfaceElevated, Qt.rgba(themeManager.accent.r, themeManager.accent.g, themeManager.accent.b, 0.55))
                           : themeManager.surfaceElevated
                    HoverHandler { id: cellHover }

                    Behavior on color {
                        ColorAnimation { duration: themeManager.animFast }
                    }

                    Image {
                        anchors.fill: parent
                        anchors.margins: 3
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        cache: false // ThumbnailCache already owns caching
                        sourceSize: Qt.size(root.edge, root.edge)
                        source: slot.thumbnailSource

                        AppToolTip {
                            visible: hoverHandler.hovered
                            text: slot.fileName
                        }

                        HoverHandler { id: hoverHandler }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: folderModel.setCurrentIndex(slot.index)
                    }
                }
            }
        }
    }

    // The draggable edge: the panel's right border. A thin line shows where it is while the
    // mouse is over it or dragging; the tab (declared later, so on top) keeps its own clicks.
    Rectangle {
        visible: !root.collapsed && root.width > 0.5
        x: root.width - 2
        width: 2
        height: parent.height
        color: themeManager.accent
        opacity: edgeMouse.pressed ? 0.9 : edgeMouse.containsMouse ? 0.6 : 0
        Behavior on opacity { NumberAnimation { duration: themeManager.animFast } }
    }
    MouseArea {
        id: edgeMouse
        visible: !root.collapsed && root.width > 0.5
        x: root.width - 4
        width: 8
        height: parent.height
        hoverEnabled: true
        cursorShape: Qt.SizeHorCursor

        property real startX: 0
        property int startCell: 0

        onPressed: function (mouse) {
            startX = mapToItem(null, mouse.x, 0).x; // window coordinates: the item moves as it is dragged
            startCell = root.cell;
            root.liveCell = startCell;
        }
        onPositionChanged: function (mouse) {
            if (!pressed)
                return;
            const dx = mapToItem(null, mouse.x, 0).x - startX;
            root.liveCell = Math.max(root.minCell, Math.min(root.maxCell, Math.round(startCell + dx / root.cols)));
        }
        function finish() {
            if (root.liveCell > 0)
                appSettings.stripThumbSize = root.liveCell;
            root.liveCell = -1;
        }
        onReleased: finish()
        onCanceled: finish()
    }

    // The tab: same fill and outline as the panel, rounded only on the outer
    // side. It starts on the panel's own 1px border column and paints its
    // left column over it, so the border line is interrupted there and the
    // tab reads as part of the panel.
    Rectangle {
        id: collapseTab
        visible: root.tabShown
        x: root.width - 1
        width: 21
        height: 52
        anchors.verticalCenter: parent.verticalCenter
        topLeftRadius: 0
        bottomLeftRadius: 0
        topRightRadius: themeManager.radiusMedium === 0 ? 0 : 8
        bottomRightRadius: themeManager.radiusMedium === 0 ? 0 : 8
        color: tabMouse.containsMouse ? Qt.tint(themeManager.surface, Qt.rgba(themeManager.textPrimary.r, themeManager.textPrimary.g, themeManager.textPrimary.b, 0.14)) : themeManager.surface
        border.color: themeManager.border
        border.width: 1
        Behavior on color { ColorAnimation { duration: themeManager.animFast } }

        Rectangle {
            x: 0
            y: 1
            width: 1
            height: parent.height - 2
            color: parent.color
        }

        AppIcon {
            x: 1 + (parent.width - 1 - width) / 2
            anchors.verticalCenter: parent.verticalCenter
            name: root.collapsed ? "next" : "prev"
            size: 16
            color: themeManager.textPrimary
        }

        MouseArea {
            id: tabMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                if (root.mode === "auto")
                    root.flipped = !root.flipped;
                else
                    root.manualCollapsed = !root.manualCollapsed;
            }
        }
    }
}
