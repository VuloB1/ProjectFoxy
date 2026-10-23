import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Drawn over a drop target while files are dragged over it ("Suelta aquí"): a veil with the accent border and, in the middle,
// a card with an icon and a line of text (on its own surface, so it reads over any picture). The mouse goes through it.
Item {
    id: root

    property bool active: false
    property string text: ""
    property string iconName: "open"

    anchors.fill: parent
    visible: opacity > 0
    opacity: active ? 1 : 0
    z: 50
    Behavior on opacity { NumberAnimation { duration: themeManager.animFast } }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 6
        radius: themeManager.radiusMedium === 0 ? 0 : 14
        color: Qt.rgba(themeManager.accent.r, themeManager.accent.g, themeManager.accent.b, 0.10)
        border.color: themeManager.accent
        border.width: 2
    }

    TextMetrics { id: metrics; text: root.text; font.pixelSize: 16; font.bold: true }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, metrics.width + 56)
        height: content.implicitHeight + 40
        radius: themeManager.radiusMedium === 0 ? 0 : 16
        color: themeManager.surfaceElevated
        border.color: themeManager.accent
        border.width: 1

        Column {
            id: content
            anchors.centerIn: parent
            spacing: 10
            AppIcon { anchors.horizontalCenter: parent.horizontalCenter; name: root.iconName; size: 40; color: themeManager.accent }
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.text
                color: themeManager.textPrimary
                font.pixelSize: 16
                font.bold: true
            }
        }
    }
}
