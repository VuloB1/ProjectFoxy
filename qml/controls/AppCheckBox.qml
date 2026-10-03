import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Themed replacement for the stock CheckBox: same public API (checked/
// onToggled/text/...). Bakes in the textSecondary-colored label that
// EditPanel.qml used to override manually at each call site.
CheckBox {
    id: control

    indicator: Loader {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: 16
        height: 16
        sourceComponent: themeManager.skin === "win98" ? win98Indicator : modernIndicator

        Component {
            id: modernIndicator
            Rectangle {
                anchors.fill: parent
                radius: themeManager.radiusSmall
                color: control.checked ? themeManager.accent : themeManager.surfaceElevated
                border.color: control.checked || control.hovered ? themeManager.accent : themeManager.border
                border.width: 1
                opacity: control.enabled ? 1.0 : 0.5

                Text {
                    anchors.centerIn: parent
                    visible: control.checked
                    text: "✓"
                    color: themeManager.accentText
                    font.pixelSize: 11
                    font.bold: true
                }
            }
        }

        // A plain sunken white box with a black check mark - no color fill
        // at all, exactly like the real thing.
        Component {
            id: win98Indicator
            Item {
                anchors.fill: parent
                Win98Bevel {
                    anchors.fill: parent
                    sunken: true
                    face: themeManager.fieldFace
                    contentInset: 1
                }
                Text {
                    anchors.centerIn: parent
                    visible: control.checked
                    text: "✓"
                    color: themeManager.textPrimary
                    font.pixelSize: 11
                    font.bold: true
                }
            }
        }
    }

    contentItem: Label {
        text: control.text
        color: control.hovered ? themeManager.textPrimary : themeManager.textSecondary
        leftPadding: control.indicator.width + 4
        verticalAlignment: Text.AlignVCenter
        opacity: control.enabled ? 1.0 : 0.5
    }
}
