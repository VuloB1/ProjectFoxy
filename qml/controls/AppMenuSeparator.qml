import QtQuick
import QtQuick.Controls
import ImageViewerApp

MenuSeparator {
    id: control
    topPadding: themeManager.skin === "win98" ? 2 : 4
    bottomPadding: topPadding
    contentItem: Rectangle {
        implicitHeight: 1
        // inset from the card's edges so the line floats inside it
        color: themeManager.border
        opacity: themeManager.skin === "win98" ? 1.0 : 0.8
    }
}
