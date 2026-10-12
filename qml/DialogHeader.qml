import QtQuick
import QtQuick.Controls
import ImageViewerApp

// The top of a dialog or studio panel: an icon, a title, a line saying what it is for, a close button on the
// right and a thin line under it. The same look as the page header of the settings.
Item {
    id: root

    property string iconName: ""
    property string title: ""
    property string hint: ""
    // The close button is shown unless the owner says it cannot be used now (a job is running).
    property bool closeEnabled: true
    // Panels that close some other way (the same button that opened them) hide it and use a smaller margin.
    property bool showClose: true
    property int margin: 28
    signal closeRequested()

    width: parent ? parent.width : 0
    height: 80

    Row {
        x: root.margin
        anchors.verticalCenter: parent.verticalCenter
        spacing: 14
        AppIcon {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.iconName.length > 0
            name: root.iconName
            size: 26
            color: themeManager.accent
        }
        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3
            Label { text: root.title; color: themeManager.textPrimary; font.bold: true; font.pixelSize: 18 }
            Label {
                visible: root.hint.length > 0
                text: root.hint
                color: themeManager.textSecondary
                font.pixelSize: 12
            }
        }
    }

    AppToolButton {
        visible: root.showClose
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        text: qsTr("Cerrar")
        iconName: "close"
        iconOnly: true
        enabled: root.closeEnabled
        onClicked: root.closeRequested()
    }

    Rectangle {
        x: root.margin
        y: parent.height - 1
        width: parent.width - 2 * root.margin
        height: 1
        color: themeManager.border
        opacity: 0.6
    }
}
