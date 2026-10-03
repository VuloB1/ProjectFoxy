import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import ImageViewerApp

// Themed replacement for the stock Menu: children are declared directly as
// AppMenuItem/AppMenuSeparator, same as a stock Menu - only the look changes.
// Modern skins get a rounded, softly shadowed card with room around its items;
// Win98 keeps the raised bevel panel.
Menu {
    id: control

    padding: themeManager.skin === "win98" ? 2 : 6
    implicitWidth: 230

    background: Loader {
        sourceComponent: themeManager.skin === "win98" ? win98Bg : flatBg

        Component {
            id: flatBg
            Item {
                implicitWidth: 230
                Rectangle {
                    id: card
                    anchors.fill: parent
                    radius: 10
                    border.color: themeManager.border
                    border.width: 1
                    color: themeManager.surfaceElevated
                }
                MultiEffect {
                    source: card
                    anchors.fill: card
                    z: -1
                    shadowEnabled: true
                    shadowColor: "#55000000"
                    shadowBlur: 0.7
                    shadowVerticalOffset: 6
                }
            }
        }

        // A raised bevel panel, like the real thing.
        Component {
            id: win98Bg
            Win98Bevel {
                implicitWidth: 200
                contentInset: 2
            }
        }
    }
}
