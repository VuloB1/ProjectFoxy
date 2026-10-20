import QtQuick
import QtQuick.Controls
import ImageViewerApp

// A few choices as buttons, one of them picked; they wrap onto more lines when they do not fit. The replacement of a
// dropdown when there are only a handful of options (a dropdown hides them; this shows them).
//   model: ["Fundido", { text: "Zoom", tip: "Acerca la imagen" }]
// `currentIndex` is bound by the owner; `activated(index)` says what was clicked.
Flow {
    id: root

    property var model: []
    property int currentIndex: -1
    signal activated(int index)

    width: parent ? parent.width : 0
    spacing: 6

    Repeater {
        model: root.model
        delegate: Rectangle {
            id: chip
            required property int index
            required property var modelData
            readonly property bool picked: root.currentIndex === index
            readonly property string label: typeof modelData === "string" ? modelData : (modelData.text ?? "")
            readonly property string tip: typeof modelData === "string" ? "" : (modelData.tip ?? "")
            height: 30
            width: Math.min(root.width, chipText.implicitWidth + 24)
            radius: themeManager.radiusMedium === 0 ? 0 : 8
            color: picked ? Qt.rgba(themeManager.accent.r, themeManager.accent.g, themeManager.accent.b, 0.24)
                          : (hover.hovered ? Qt.rgba(themeManager.textPrimary.r, themeManager.textPrimary.g, themeManager.textPrimary.b, 0.1)
                                           : Qt.rgba(themeManager.textPrimary.r, themeManager.textPrimary.g, themeManager.textPrimary.b, 0.05))
            border.color: picked ? themeManager.accent : themeManager.border
            border.width: 1

            Label {
                id: chipText
                anchors.centerIn: parent
                width: Math.min(implicitWidth, root.width - 24)
                elide: Text.ElideRight
                text: chip.label
                color: chip.picked ? themeManager.accent : themeManager.textPrimary
                font.bold: chip.picked
            }
            HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: root.activated(chip.index) }
            AppToolTip { visible: hover.hovered && chip.tip.length > 0; text: chip.tip }
        }
    }
}
