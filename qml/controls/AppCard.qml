import QtQuick
import QtQuick.Controls
import ImageViewerApp

// A rounded panel that groups the controls of one topic, under a small upper-case heading. Children go in
// the column inside it (give them `width: parent.width`).
Rectangle {
    id: card

    default property alias content: inner.data
    property string heading: ""

    width: parent ? parent.width : 0
    implicitHeight: inner.implicitHeight + (heading.length > 0 ? 54 : 32)
    radius: themeManager.radiusMedium === 0 ? 0 : 12
    color: themeManager.surface
    border.color: themeManager.border
    border.width: 1

    Label {
        visible: card.heading.length > 0
        x: 18
        y: 14
        text: card.heading
        color: themeManager.textSecondary
        font.bold: true
        font.pixelSize: 11
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 0.8
    }
    Column {
        id: inner
        x: 18
        y: card.heading.length > 0 ? 40 : 16
        width: card.width - 36
        spacing: 12
    }
}
