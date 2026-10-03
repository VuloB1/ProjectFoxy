import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Themed replacement for the stock TabBar: same public API (currentIndex/
// onCurrentIndexChanged/...). Children are declared as AppTabButton, same as
// a stock TabBar would take TabButton children.
TabBar {
    id: control

    background: Rectangle {
        color: "transparent"
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: themeManager.border }
    }
}
