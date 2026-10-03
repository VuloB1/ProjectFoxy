import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Small message box floated over the canvas: the crop instructions and the
// zoom / image-position readout both use it, so they always look the same.
// Modern skins: translucent dark card with white text, readable over any
// image. Win98: the theme's own raised gray bevel with black text (a dark
// translucent card was the one thing in that theme that wasn't gray).
Item {
    id: root

    property string text: ""
    // Width the text wraps at; the box itself only grows as wide as the text
    // actually needs.
    property real maxTextWidth: 420

    readonly property bool retro: themeManager.skin === "win98"

    implicitWidth: label.implicitWidth + 28
    implicitHeight: label.implicitHeight + 16
    width: implicitWidth
    height: implicitHeight

    Loader {
        anchors.fill: parent
        sourceComponent: root.retro ? win98Bg : modernBg

        Component {
            id: modernBg
            Rectangle {
                radius: themeManager.radiusMedium
                color: "#992a2a2e"
            }
        }

        Component {
            id: win98Bg
            Win98Bevel {}
        }
    }

    Label {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.retro ? themeManager.textPrimary : "#ffffff"
        wrapMode: Text.WordWrap
        width: root.maxTextWidth
        horizontalAlignment: Text.AlignHCenter
    }
}
