import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Themed replacement for the stock Dialog: same public API (title/modal/
// standardButtons/onAccepted/anchors.centerIn: Overlay.overlay/...). Only
// `background` is overridden - header/footer (title label, the Yes/No
// button box from `standardButtons`) are left as Qt Quick Controls' own
// defaults so that wiring isn't put at risk of breaking.
//
Dialog {
    id: control

    readonly property int cornerRadius: themeManager.radiusMedium

    background: Rectangle {
        anchors.fill: parent
        radius: control.cornerRadius
        color: themeManager.surfaceElevated
        border.color: themeManager.border
        border.width: 1
    }

    header: Label {
        visible: control.title.length > 0
        text: control.title
        color: themeManager.textPrimary
        font.bold: true
        padding: 12
        background: Rectangle {
            color: themeManager.surfaceElevated
            radius: control.cornerRadius
            // Square off the bottom corners so the header's rounding only
            // shows at the top, matching the panel below it.
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: parent.radius
                color: parent.color
                visible: parent.radius > 0
            }
        }
    }
}
