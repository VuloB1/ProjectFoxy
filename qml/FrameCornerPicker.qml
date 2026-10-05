import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Which of a rectangle's four corners are rounded / cut: a small rectangle with a dot on each corner,
// each dot a switch. `mask` has one bit per corner - top left 1, top right 2, bottom right 4, bottom left 8 -
// the order the frame effect numbers them in.
Item {
    id: root

    property int mask: 15
    signal changed(int newMask)

    implicitWidth: 78
    implicitHeight: 54

    Rectangle {
        anchors.fill: parent
        anchors.margins: 9
        radius: 3
        color: "transparent"
        border.width: 1
        border.color: themeManager.border
    }

    Repeater {
        model: [
            { bit: 1, ax: 0, ay: 0, tip: qsTr("Esquina superior izquierda") },
            { bit: 2, ax: 1, ay: 0, tip: qsTr("Esquina superior derecha") },
            { bit: 4, ax: 1, ay: 1, tip: qsTr("Esquina inferior derecha") },
            { bit: 8, ax: 0, ay: 1, tip: qsTr("Esquina inferior izquierda") }
        ]
        delegate: Rectangle {
            id: dot
            required property var modelData
            readonly property bool on: (root.mask & modelData.bit) !== 0
            width: 18
            height: 18
            radius: 9
            x: modelData.ax * (root.width - width)
            y: modelData.ay * (root.height - height)
            color: on ? themeManager.accent : themeManager.controlFace
            border.width: 1
            border.color: on ? themeManager.accent : themeManager.border
            Behavior on color { ColorAnimation { duration: themeManager.animFast } }

            MouseArea {
                id: area
                anchors.fill: parent
                anchors.margins: -3
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.changed(dot.on ? root.mask & ~dot.modelData.bit : root.mask | dot.modelData.bit)
            }
            AppToolTip { visible: area.containsMouse; text: dot.modelData.tip }
        }
    }
}
