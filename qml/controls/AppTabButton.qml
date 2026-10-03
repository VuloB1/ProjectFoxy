import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Themed replacement for the stock TabButton: same public API (text/checked
// managed by the parent TabBar/...). Selected tab = accent underline.
TabButton {
    id: control

    // Optional icon shown above the label - unset (default) behaves exactly
    // as before (plain single-line label). Named iconName (not `icon`)
    // because AbstractButton already has a FINAL `icon` grouped property.
    // Value is one of the names AppIcon.qml understands, not a glyph.
    property string iconName: ""

    // See AppToolButton.qml for why this has to be measured independently
    // and used to override implicitWidth directly, instead of letting it
    // derive from contentItem the usual way (and for the +4). Measured bold
    // (not `control.font` as-is) because the label below goes bold only when
    // this tab is the selected one - sizing for the lighter weight left the
    // selected state's wider text eliding when it shouldn't.
    implicitWidth: labelMetrics.width + 4 + leftPadding + rightPadding

    TextMetrics {
        id: labelMetrics
        font.family: control.font.family
        font.pixelSize: control.font.pixelSize
        font.bold: true
        text: control.text
    }

    background: Loader {
        sourceComponent: themeManager.skin === "win98" ? win98Bg : modernBg

        Component {
            id: modernBg
            Rectangle {
                anchors.fill: parent
                color: control.hovered && !control.checked
                       ? Qt.rgba(themeManager.textPrimary.r, themeManager.textPrimary.g, themeManager.textPrimary.b, 0.08)
                       : "transparent"
                radius: themeManager.radiusSmall
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: control.checked ? 2 : 0
                    color: themeManager.accent
                }
            }
        }

        // Both raised bevel boxes; the selected one matches the page's own
        // face color (reads as "merged" with the panel below), the rest a
        // touch darker (reads as "behind" it).
        Component {
            id: win98Bg
            Win98Bevel {
                anchors.fill: parent
                face: control.checked ? themeManager.controlFace : Qt.darker(themeManager.controlFace, 1.08)
                contentInset: 2
            }
        }
    }

    contentItem: Column {
        spacing: 2

        AppIcon {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: control.iconName.length > 0
            name: control.iconName
            size: 20
            color: control.checked || control.hovered ? themeManager.textPrimary : themeManager.textSecondary
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            // Capped at control.availableWidth so a tab squeezed by the
            // TabBar elides instead of spilling into its neighbor. Safe now
            // that control.implicitWidth above no longer derives from this
            // label.
            width: Math.min(labelMetrics.width + 4, control.availableWidth)
            text: control.text
            color: control.checked || control.hovered ? themeManager.textPrimary : themeManager.textSecondary
            font.bold: control.checked
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
}
