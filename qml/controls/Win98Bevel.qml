import QtQuick
import ImageViewerApp

// Shared chiseled 3D bevel surface for the Windows 98 skin: a 2px border
// built from an outer white/black ring plus an inner light-gray/dark-gray
// ring, faking the classic raised (button at rest) or sunken (pressed,
// text/number fields, checkbox box, slider groove) look with no shaders.
// `sunken` swaps which corner pair (top+left vs bottom+right) gets the
// light vs dark tones.
Item {
    id: root
    property bool sunken: false
    property color face: themeManager.controlFace
    // Real Win98 buttons keep a flat 1px margin between the bevel and the
    // content; some sunken fields (text/number entry) want the fill to
    // reach all the way to the inner ring instead.
    property int contentInset: 2

    Rectangle {
        anchors.fill: parent
        anchors.margins: root.contentInset
        color: root.face
    }

    // outer ring
    Rectangle {
        anchors.left: parent.left; anchors.top: parent.top; anchors.right: parent.right
        height: 1
        color: root.sunken ? themeManager.bevelDarkest : themeManager.bevelLight
    }
    Rectangle {
        anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
        width: 1
        color: root.sunken ? themeManager.bevelDarkest : themeManager.bevelLight
    }
    Rectangle {
        anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.right: parent.right
        height: 1
        color: root.sunken ? themeManager.bevelLight : themeManager.bevelDarkest
    }
    Rectangle {
        anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.right: parent.right
        width: 1
        color: root.sunken ? themeManager.bevelLight : themeManager.bevelDarkest
    }

    // inner ring, inset by 1px
    Rectangle {
        anchors.left: parent.left; anchors.top: parent.top; anchors.right: parent.right
        anchors.leftMargin: 1; anchors.topMargin: 1; anchors.rightMargin: 1
        height: 1
        color: root.sunken ? themeManager.bevelDark : themeManager.bevelLightSoft
    }
    Rectangle {
        anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
        anchors.leftMargin: 1; anchors.topMargin: 1; anchors.bottomMargin: 1
        width: 1
        color: root.sunken ? themeManager.bevelDark : themeManager.bevelLightSoft
    }
    Rectangle {
        anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.right: parent.right
        anchors.leftMargin: 1; anchors.bottomMargin: 1; anchors.rightMargin: 1
        height: 1
        color: root.sunken ? themeManager.bevelLightSoft : themeManager.bevelDark
    }
    Rectangle {
        anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.right: parent.right
        anchors.topMargin: 1; anchors.bottomMargin: 1; anchors.rightMargin: 1
        width: 1
        color: root.sunken ? themeManager.bevelLightSoft : themeManager.bevelDark
    }
}
