import QtQuick
import QtQuick.Controls
import ImageViewerApp

// A titled group of controls that folds away when its header is clicked, so
// the long Ajustes list stays scannable. Put the section's controls directly
// inside it; give them `width: parent.width`.
Item {
    id: root

    property string title: ""
    // an icon (AppIcon name) that says what the section is about; it goes before the title
    property string icon: ""
    // the explanation of the section: shown as a tip on a small "i" next to the title, not as text taking room
    property string hint: ""
    property bool expanded: true
    default property alias content: body.data

    implicitHeight: header.height + bodyClip.height

    Item {
        id: header
        width: parent.width
        height: 34

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
            opacity: 0.55
        }
        HoverHandler { id: headerHover }

        AppIcon {
            id: sectionIcon
            visible: root.icon.length > 0
            anchors.left: parent.left
            anchors.leftMargin: 2
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 1
            name: root.icon
            size: 18
            color: root.expanded ? themeManager.accent : themeManager.textSecondary
        }
        Label {
            id: titleLabel
            anchors.left: sectionIcon.visible ? sectionIcon.right : parent.left
            anchors.leftMargin: sectionIcon.visible ? 8 : 2
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 1
            text: root.title
            color: themeManager.textPrimary
            font.pixelSize: 13
            font.weight: Font.DemiBold
        }
        // the explanation, on hover
        AppIcon {
            id: hintIcon
            visible: root.hint.length > 0
            anchors.left: titleLabel.right
            anchors.leftMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 1
            name: "info"
            size: 15
            color: hintHover.hovered ? themeManager.accent : themeManager.textSecondary
            opacity: hintHover.hovered ? 1 : 0.7
            HoverHandler { id: hintHover }
            AppToolTip { visible: hintHover.hovered; text: root.hint }
        }
        AppIcon {
            id: chevron
            anchors.right: parent.right
            anchors.rightMargin: 2
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 1
            name: "next"
            size: 16
            color: themeManager.textSecondary
            rotation: root.expanded ? 90 : 0
            Behavior on rotation { NumberAnimation { duration: themeManager.animFast } }
        }
        MouseArea {
            anchors.fill: parent
            z: -1
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
