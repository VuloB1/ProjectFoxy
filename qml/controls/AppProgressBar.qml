import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Themed replacement for the stock ProgressBar: same public API (from/to/
// value/...). Static accent fill - no animated "marching chunks" (deferred).
ProgressBar {
    id: control

    background: Loader {
        sourceComponent: themeManager.skin === "win98" ? win98Bg : flatBg

        Component {
            id: flatBg
            Rectangle {
                implicitHeight: 8
                radius: height / 2
                color: themeManager.border
            }
        }

        Component {
            id: win98Bg
            Win98Bevel { implicitHeight: 20; sunken: true; face: themeManager.fieldFace; contentInset: 2 }
        }
    }

    contentItem: Item {
        id: fill
        implicitHeight: themeManager.skin === "win98" ? 20 : 8
        readonly property int blockSize: 10
        readonly property int blockSpacing: 2
        readonly property int blockCount: Math.max(1, Math.floor((width + blockSpacing) / (blockSize + blockSpacing)))
        readonly property int blocksLit: Math.round(control.position * blockCount)

        // Static accent fill for Modern - no animated "marching chunks"
        // (deferred).
        Rectangle {
            visible: themeManager.skin !== "win98"
            width: control.visualPosition * parent.width
            height: parent.height
            radius: height / 2
            color: themeManager.accent
        }

        // The classic segmented block look, at least statically sized to
        // the current progress (not animated/marching).
        Row {
            visible: themeManager.skin === "win98"
            anchors.fill: parent
            anchors.margins: 2
            spacing: fill.blockSpacing
            clip: true
            Repeater {
                model: fill.blockCount
                delegate: Rectangle {
                    required property int index
                    width: fill.blockSize
                    height: parent.height
                    color: index < fill.blocksLit ? themeManager.accent : "transparent"
                }
            }
        }
    }
}
