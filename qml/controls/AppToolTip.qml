import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Themed replacement for the stock ToolTip: same public API (visible, text,
// delay, ...). Modern skins get a small elevated pill; Win98 gets the flat
// gray face with a 1px black frame instead of Basic's dark default, which
// clashed with that theme.
ToolTip {
    id: control

    delay: 400

    contentItem: Label {
        text: control.text
        font: control.font
        color: themeManager.textPrimary
    }

    background: Loader {
        sourceComponent: themeManager.skin === "win98" ? win98Bg : modernBg

        Component {
            id: modernBg
            Rectangle {
                radius: themeManager.radiusSmall
                color: themeManager.surfaceElevated
                border.color: themeManager.border
                border.width: 1
            }
        }

        Component {
            id: win98Bg
            Rectangle {
                color: themeManager.controlFace
                border.color: themeManager.bevelDarkest
                border.width: 1
            }
        }
    }
}
