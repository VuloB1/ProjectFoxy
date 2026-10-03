import QtQuick
import QtQuick.Controls
import ImageViewerApp

// An on/off switch for settings (same API as a CheckBox: checked / onToggled): a pill with
// a sliding knob in the modern skins, the classic sunken check box in Win98.
CheckBox {
    id: control

    implicitWidth: themeManager.skin === "win98" ? 16 : 42
    implicitHeight: themeManager.skin === "win98" ? 16 : 24
    padding: 0
    hoverEnabled: true

    indicator: Loader {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.implicitWidth
        height: control.implicitHeight
        sourceComponent: themeManager.skin === "win98" ? win98Indicator : pill

        Component {
            id: pill
            Rectangle {
                radius: height / 2
                color: control.checked ? themeManager.accent : themeManager.border
                opacity: control.enabled ? 1.0 : 0.5
                Behavior on color { ColorAnimation { duration: themeManager.animFast } }

                // a faint ring while hovered
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -3
                    radius: height / 2
                    color: "transparent"
                    border.width: 2
                    border.color: Qt.rgba(themeManager.accent.r, themeManager.accent.g, themeManager.accent.b, 0.35)
                    visible: control.hovered
                }

                Rectangle {
                    id: knob
                    width: parent.height - 6
                    height: width
                    radius: width / 2
                    y: 3
                    x: control.checked ? parent.width - width - 3 : 3
                    color: control.checked ? themeManager.accentText : themeManager.textPrimary
                    Behavior on x { NumberAnimation { duration: themeManager.animFast; easing.type: themeManager.easingCurve } }
                }
            }
        }

        // A plain sunken box with a check mark, like AppCheckBox.
        Component {
            id: win98Indicator
            Item {
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

    contentItem: Item {}
}
