import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Themed replacement for the stock ScrollBar: a thin rounded thumb over a
// transparent track (Modern), or the classic raised gray block on a light
// track (Win98). Only shows up when the content is actually taller/wider than
// the view. Attach it like the stock one: `ScrollBar.vertical: AppScrollBar {}`
// - and size the scrolled content by this bar's implicitWidth so the two never
// overlap.
ScrollBar {
    id: control

    readonly property bool isWin98: themeManager.skin === "win98"

    implicitWidth: isWin98 ? 16 : 8
    implicitHeight: isWin98 ? 16 : 8
    padding: isWin98 ? 0 : 1
    visible: size < 1.0

    background: Rectangle {
        visible: control.isWin98
        color: Qt.lighter(themeManager.controlFace, 1.12)
    }

    contentItem: Loader {
        sourceComponent: control.isWin98 ? win98Thumb : modernThumb

        Component {
            id: modernThumb
            Rectangle {
                implicitWidth: 6
                implicitHeight: 6
                radius: width / 2
                color: themeManager.textSecondary
                opacity: control.pressed ? 0.7 : control.hovered ? 0.55 : 0.35
            }
        }

        Component {
            id: win98Thumb
            Win98Bevel {
                implicitWidth: 16
                implicitHeight: 16
                sunken: control.pressed
            }
        }
    }
}
