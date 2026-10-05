import QtQuick
import QtQuick.Controls
import ImageViewerApp

// The caption of one control of an effect (see EffectParamRow): double-click it to put the control
// back to the effect's default; the tooltip says so (or shows the control's own hint).
Item {
    id: root

    property alias text: captionLabel.text
    property int paramIndex: 0
    property real defaultValue: 0
    property bool modified: false
    property string hint: ""

    implicitHeight: captionLabel.implicitHeight

    Label {
        id: captionLabel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        color: themeManager.textSecondary
        elide: Text.ElideRight
    }
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onDoubleClicked: appController.setEffectValue(root.paramIndex, root.defaultValue)
        AppToolTip {
            visible: parent.containsMouse && (root.modified || root.hint.length > 0)
            delay: 700
            text: root.hint.length > 0 ? root.hint : qsTr("Doble clic para restablecer")
        }
    }
}
