import QtQuick
import QtQuick.Controls
import ImageViewerApp

// RGB / R / G / B picker shared by the Levels and Curves editors. Index 0 is
// the combined channel, 1..3 red, green, blue (same order the C++ side uses).
Row {
    id: root

    property int channel: 0
    signal picked(int channel)

    spacing: 4
    readonly property real cell: (width - spacing * 3) / 4

    Repeater {
        model: [qsTr("RGB"), qsTr("R"), qsTr("G"), qsTr("B")]
        delegate: AppToolButton {
            required property int index
            required property string modelData
            width: root.cell
            text: modelData
            // Not `checkable`: a click would toggle (and so detach) the
            // binding below; the owner moves the selection via picked().
            checked: root.channel === index
            onClicked: root.picked(index)
        }
    }
}
