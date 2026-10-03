import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Themed replacement for the stock MenuItem: same public API (text/checkable/
// checked/onTriggered/enabled/...), plus an optional icon and a `danger` look
// for destructive entries. Highlight = translucent accent tint (red for danger).
MenuItem {
    id: control

    // One of the names AppIcon.qml understands; empty = no icon (the text lines up anyway).
    property string iconName: ""
    // A destructive action (deleting a file): red text and icon, red highlight.
    property bool danger: false

    readonly property color dangerColor: "#e5484d"
    readonly property color tint: danger ? dangerColor : themeManager.accent
    readonly property bool win98: themeManager.skin === "win98"

    implicitHeight: win98 ? 28 : 36

    background: Loader {
        active: control.highlighted
        sourceComponent: control.win98 ? win98Highlight : modernHighlight

        Component {
            id: modernHighlight
            Rectangle {
                anchors.fill: parent
                radius: 6
                color: Qt.rgba(control.tint.r, control.tint.g, control.tint.b, control.danger ? 0.16 : 0.22)
            }
        }

        // The classic solid navy selection bar - no gradient, no radius.
        Component {
            id: win98Highlight
            Rectangle {
                anchors.fill: parent
                color: themeManager.accent
            }
        }
    }

    contentItem: Row {
        spacing: 10
        leftPadding: 10
        rightPadding: control.checkable ? 28 : 10
        opacity: control.enabled ? 1.0 : 0.45

        AppIcon {
            anchors.verticalCenter: parent.verticalCenter
            visible: control.iconName.length > 0 && !control.win98
            width: visible ? 18 : 0
            name: control.iconName
            size: 18
            color: control.highlighted && control.win98 ? themeManager.accentText
                   : control.danger ? control.dangerColor
                   : control.highlighted ? themeManager.textPrimary : themeManager.textSecondary
        }
        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            color: control.highlighted && control.win98 ? themeManager.accentText
                   : control.danger ? control.dangerColor : themeManager.textPrimary
        }
    }

    indicator: Item {
        visible: control.checkable
        x: control.width - width - 10
        y: control.topPadding + (control.availableHeight - height) / 2
        width: 14
        height: 14
        Text {
            anchors.centerIn: parent
            visible: control.checked
            text: "✓"
            color: control.highlighted && control.win98 ? themeManager.accentText : themeManager.accent
            font.bold: true
        }
    }
}
