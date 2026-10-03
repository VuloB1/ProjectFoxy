import QtQuick
import QtQuick.Controls
import ImageViewerApp

// A titled group of controls that folds away when its header is clicked, so
// the long Ajustes list stays scannable. Put the section's controls directly
// inside it; give them `width: parent.width`.
Item {
    id: root

    property string title: ""
    property bool expanded: true
    default property alias content: body.data

    implicitHeight: header.height + bodyClip.height

    Item {
        id: header
        width: parent.width
        height: 30

        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 1
            radius: themeManager.radiusSmall
            color: Qt.rgba(themeManager.textPrimary.r, themeManager.textPrimary.g, themeManager.textPrimary.b, 0.08)
            visible: headerHover.hovered
        }
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: themeManager.border
        }
        HoverHandler { id: headerHover }

        AppIcon {
            id: chevron
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 1
            name: "next"
            size: 16
            color: themeManager.textSecondary
            rotation: root.expanded ? 90 : 0
            Behavior on rotation { NumberAnimation { duration: themeManager.animFast } }
        }
        Label {
            anchors.left: chevron.right
            anchors.leftMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 1
            text: root.title
            color: themeManager.textPrimary
            font.bold: true
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.expanded = !root.expanded
        }
    }

    Item {
        id: bodyClip
        anchors.top: header.bottom
        width: parent.width
        height: root.expanded ? body.implicitHeight + 6 : 0
        clip: true
        Behavior on height { NumberAnimation { duration: themeManager.animFast; easing.type: themeManager.easingCurve } }

        Column {
            id: body
            width: parent.width
            spacing: 8
        }
    }
}
