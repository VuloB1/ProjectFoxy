import QtQuick
import QtQuick.Controls
import ImageViewerApp

// A question with two answers: a round icon, what is about to happen, and Cancelar /
// the confirming button on the right (red when the action destroys something).
// Open it with open(); `confirmed` fires only on the confirming button - Escape, a click
// outside or Cancelar all just close it.
AppDialog {
    id: root

    property string heading: ""
    property string message: ""
    // A smaller second line (what can be undone, what is kept...).
    property string detail: ""
    property string confirmText: qsTr("Aceptar")
    property string confirmIcon: "check"
    property string symbol: "trash"
    property bool danger: false

    signal confirmed()

    modal: true
    standardButtons: Dialog.NoButton
    anchors.centerIn: Overlay.overlay
    padding: 24
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    readonly property color accentTone: danger ? Qt.color("#e5484d") : themeManager.accent

    contentItem: Column {
        spacing: 22
        width: 380

        Row {
            spacing: 16
            width: parent.width

            // the round icon
            Rectangle {
                width: 46
                height: 46
                radius: themeManager.radiusMedium === 0 ? 0 : 23
                color: Qt.rgba(root.accentTone.r, root.accentTone.g, root.accentTone.b, 0.16)
                AppIcon {
                    anchors.centerIn: parent
                    name: root.symbol
                    size: 24
                    color: root.accentTone
                }
            }

            Column {
                width: parent.width - 46 - 16
                spacing: 6
                Label {
                    width: parent.width
                    text: root.heading
                    color: themeManager.textPrimary
                    font.bold: true
                    font.pixelSize: 17
                    wrapMode: Text.WordWrap
                }
                Label {
                    width: parent.width
                    visible: root.message.length > 0
                    text: root.message
                    color: themeManager.textPrimary
                    wrapMode: Text.WordWrap
                }
                Label {
                    width: parent.width
                    visible: root.detail.length > 0
                    text: root.detail
                    color: themeManager.textSecondary
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }
        }

        Row {
            spacing: 10
            anchors.right: parent.right

            AppButton {
                width: 110
                text: qsTr("Cancelar")
                onClicked: root.close()
            }
            AppButton {
                width: 140
                text: root.confirmText
                danger: root.danger
                primary: !root.danger
                onClicked: {
                    root.close();
                    root.confirmed();
                }
            }
        }
    }
}
