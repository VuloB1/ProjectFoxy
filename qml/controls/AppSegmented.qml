import QtQuick
import QtQuick.Controls
import ImageViewerApp

// A row of choices of which exactly one is picked. Each is a text or an icon (with a tooltip):
//   model: [{ text: "GIF" }, { icon: "fit", tip: "Fit" }]
// `currentIndex` is bound by the owner; `activated(index)` says what was clicked.
Rectangle {
    id: root

    property var model: []
    property int currentIndex: 0
    signal activated(int index)

    width: parent ? parent.width : 0
    height: 32
    radius: themeManager.radiusMedium === 0 ? 0 : 8
    color: Qt.rgba(themeManager.textPrimary.r, themeManager.textPrimary.g, themeManager.textPrimary.b, 0.06)
    border.color: themeManager.border
    border.width: 1

    Row {
        anchors.fill: parent
        anchors.margins: 2
        spacing: 2
        Repeater {
            model: root.model
            delegate: Rectangle {
                id: seg
                required property int index
                required property var modelData
                readonly property bool picked: root.currentIndex === index
                width: (root.width - 4 - (root.model.length - 1) * 2) / root.model.length
                height: parent.height
                radius: themeManager.radiusMedium === 0 ? 0 : 6
                color: picked ? Qt.rgba(themeManager.accent.r, themeManager.accent.g, themeManager.accent.b, 0.24)
                              : (hover.hovered ? Qt.rgba(themeManager.textPrimary.r, themeManager.textPrimary.g, themeManager.textPrimary.b, 0.08) : "transparent")
                Label {
                    visible: !!seg.modelData.text
                    anchors.centerIn: parent
                    text: seg.modelData.text || ""
                    color: seg.picked ? themeManager.accent : themeManager.textPrimary
                    font.bold: seg.picked
                }
                AppIcon {
                    visible: !!seg.modelData.icon
                    anchors.centerIn: parent
                    name: seg.modelData.icon || ""
                    size: 18
                    color: seg.picked ? themeManager.accent : themeManager.textPrimary
                }
                HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: root.activated(seg.index) }
                AppToolTip { visible: hover.hovered && !!seg.modelData.tip; text: seg.modelData.tip || "" }
            }
        }
    }
}
