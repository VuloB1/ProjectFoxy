import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Themed replacement for the stock TextField: same public API (text/
// onEditingFinished/...). Same flat-bordered-box visual language as
// ScrubNumberField.
TextField {
    id: control

    color: themeManager.textPrimary
    placeholderTextColor: themeManager.textSecondary
    selectionColor: themeManager.accent
    selectedTextColor: themeManager.accentText

    background: Loader {
        sourceComponent: themeManager.skin === "win98" ? win98Bg : flatBg

        Component {
            id: flatBg
            Rectangle {
                implicitHeight: 32
                radius: themeManager.radiusSmall
                color: themeManager.surfaceElevated
                border.color: control.activeFocus ? themeManager.accent : themeManager.border
                border.width: 1
            }
        }

        Component {
            id: win98Bg
            Win98Bevel {
                implicitHeight: 32
                sunken: true
                face: themeManager.fieldFace
                contentInset: 2
            }
        }
    }
}
