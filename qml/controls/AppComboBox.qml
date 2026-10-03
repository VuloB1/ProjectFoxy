import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Themed replacement for the stock ComboBox: same public API (model/
// currentText/currentIndex/...).
ComboBox {
    id: control

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

        // A sunken text well with a small raised arrow button on the
        // right, like the real thing.
        Component {
            id: win98Bg
            Item {
                implicitHeight: 32
                Win98Bevel { anchors.fill: parent; sunken: true; face: themeManager.fieldFace; contentInset: 2 }
                Win98Bevel {
                    width: 22
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    anchors.margins: 2
                    sunken: control.pressed
                }
            }
        }
    }

    contentItem: Label {
        text: control.displayText
        color: themeManager.textPrimary
        leftPadding: 8
        rightPadding: 24
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: control.width - width - 8
        y: control.topPadding + (control.availableHeight - height) / 2
        text: "▾"
        color: themeManager.textSecondary
    }

    popup: Popup {
        y: control.height
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight, 200)
        padding: 1

        background: Rectangle {
            radius: themeManager.radiusMedium
            color: themeManager.surfaceElevated
            border.color: themeManager.border
            border.width: 1
        }

        contentItem: ListView {
            id: popupList
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
            AppWheelScroll { view: popupList }
        }
    }

    delegate: ItemDelegate {
        id: itemDelegate
        required property var modelData
        required property int index
        width: control.width
        highlighted: control.highlightedIndex === index || hovered

        contentItem: Label {
            text: itemDelegate.modelData
            color: itemDelegate.highlighted ? themeManager.accentText : themeManager.textPrimary
            verticalAlignment: Text.AlignVCenter
            leftPadding: 8
        }
        background: Rectangle {
            color: !itemDelegate.highlighted ? "transparent"
                   : themeManager.skin === "win98" ? themeManager.accent
                   : Qt.rgba(themeManager.accent.r, themeManager.accent.g, themeManager.accent.b, 0.25)
        }
    }
}
