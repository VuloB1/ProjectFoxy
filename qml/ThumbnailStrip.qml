import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Vertical filmstrip sidebar for the current folder, backed by the C++
// FolderModel (QAbstractListModel over ThumbnailCache/ThumbnailImageProvider).
// The panel is exactly as wide as the thumbnails plus an even margin. The
// collapse control is a small tab attached to the panel's right edge and
// sticking out over the canvas, as if it were a bump of the border; collapsing
// slides the panel away to nothing (the thumbnails travel with its edge at
// their normal size instead of being squeezed) and leaves just the tab against
// the window's edge, so the folder position isn't lost.
Item {
    id: root

    property bool collapsed: false
    readonly property int margin: 8
    readonly property int thumbSize: 88
    readonly property int expandedWidth: thumbSize + 2 * margin

    width: collapsed ? 0 : expandedWidth
    // The tab hangs outside this item's own bounds, so it has to paint above
    // the canvas that comes after it in the window.
    z: 3

    Behavior on width {
        NumberAnimation { duration: themeManager.animMedium; easing.type: themeManager.easingCurve }
    }

    AppPanelBackground {
        anchors.fill: parent
        visible: root.width > 0.5
        clip: true

        ListView {
            id: list
            // Fixed size, pinned to the panel's right edge: while the panel
            // grows or shrinks the thumbnails slide in and out with it.
            x: root.margin + (root.width - root.expandedWidth)
            y: root.margin
            width: root.thumbSize
            height: parent.height - 2 * root.margin
            orientation: ListView.Vertical
            spacing: 6
            clip: true
            model: folderModel

            AppWheelScroll { view: list; step: 100 }

            // Keep the current thumbnail in view as folderModel.currentIndex
            // changes from arrow keys / floating-bar prev-next, not just clicks.
            currentIndex: folderModel.currentIndex
            highlightMoveDuration: themeManager.animMedium
            onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)

            delegate: Rectangle {
                required property int index
                required property string fileName
                required property string thumbnailSource
                required property bool isCurrent

                width: list.width
                height: width
                radius: themeManager.radiusSmall
                color: isCurrent ? themeManager.accent
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
                    source: thumbnailSource

                    AppToolTip {
                        visible: hoverHandler.hovered
                        text: fileName
                    }

                    HoverHandler { id: hoverHandler }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: folderModel.setCurrentIndex(index)
                }
            }
        }
    }

    // The tab: same fill and outline as the panel, rounded only on the outer
    // side. It starts on the panel's own 1px border column and paints its
    // left column over it, so the border line is interrupted there and the
    // tab reads as part of the panel.
    Rectangle {
        id: collapseTab
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
            onClicked: root.collapsed = !root.collapsed
        }
    }
}
