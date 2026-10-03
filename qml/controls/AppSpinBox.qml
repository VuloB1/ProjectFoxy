import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Themed replacement for the stock SpinBox: same public API (value/from/to/
// onValueModified/...). Field + stacked up/down mini buttons on the right.
SpinBox {
    id: control

    leftPadding: 8
    rightPadding: 30

    background: Loader {
        sourceComponent: themeManager.skin === "win98" ? win98Bg : flatBg

        Component {
            id: flatBg
            Rectangle {
                implicitHeight: 32
                radius: themeManager.radiusSmall
                color: themeManager.surfaceElevated
                border.color: control.activeFocus || control.hovered ? themeManager.accent : themeManager.border
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

    contentItem: TextInput {
        text: control.displayText
        font: control.font
        color: themeManager.textPrimary
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        selectByMouse: true
        selectionColor: themeManager.accent
    }

    up.indicator: Item {
        x: control.width - width - 3
        y: 3
        width: 22
        height: control.height / 2 - 4

        Loader {
            anchors.fill: parent
            sourceComponent: themeManager.skin === "win98" ? win98Up : flatUp
            Component {
                id: flatUp
                Rectangle {
                    anchors.fill: parent
                    radius: themeManager.radiusSmall
                    color: control.up.pressed ? Qt.darker(themeManager.controlFace, 1.2)
                           : control.up.hovered ? Qt.tint(themeManager.controlFace, Qt.rgba(themeManager.textPrimary.r, themeManager.textPrimary.g, themeManager.textPrimary.b, 0.14))
                           : themeManager.controlFace
                    border.color: themeManager.border
                }
            }
            Component {
                id: win98Up
                Win98Bevel { anchors.fill: parent; sunken: control.up.pressed }
            }
        }
        Text { text: "▲"; anchors.centerIn: parent; color: themeManager.textPrimary; font.pixelSize: 7 }
    }

    down.indicator: Item {
        x: control.width - width - 3
        y: control.height / 2 + 1
        width: 22
        height: control.height / 2 - 4

        Loader {
            anchors.fill: parent
            sourceComponent: themeManager.skin === "win98" ? win98Down : flatDown
            Component {
                id: flatDown
                Rectangle {
                    anchors.fill: parent
                    radius: themeManager.radiusSmall
                    color: control.down.pressed ? Qt.darker(themeManager.controlFace, 1.2)
                           : control.down.hovered ? Qt.tint(themeManager.controlFace, Qt.rgba(themeManager.textPrimary.r, themeManager.textPrimary.g, themeManager.textPrimary.b, 0.14))
                           : themeManager.controlFace
                    border.color: themeManager.border
                }
            }
            Component {
                id: win98Down
                Win98Bevel { anchors.fill: parent; sunken: control.down.pressed }
            }
        }
        Text { text: "▼"; anchors.centerIn: parent; color: themeManager.textPrimary; font.pixelSize: 7 }
    }
}
