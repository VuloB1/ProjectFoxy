import QtQuick
import QtQuick.Controls
import ImageViewerApp

// The window's own title bar (the native frame is switched off in Main.qml): icon, name, and the
// minimise / maximise / close buttons, drawn in the program's theme. Dragging it moves the window and
// a double click maximises it; both go through the system, so snapping and the like keep working.
AppPanelBackground {
    id: root
    border.width: 0
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: themeManager.border }

    required property var window
    readonly property bool maximized: window.visibility === Window.Maximized

    function toggleMaximize() { maximized ? window.showNormal() : window.showMaximized() }

    // The drag area is everything but the buttons.
    Item {
        anchors.fill: parent
        anchors.rightMargin: buttons.width
        DragHandler {
            target: null
            onActiveChanged: if (active) root.window.startSystemMove()
        }
        TapHandler { onDoubleTapped: root.toggleMaximize() }

        Row {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 10
            spacing: 8
            Image {
                anchors.verticalCenter: parent.verticalCenter
                width: 16; height: 16
                source: "qrc:/qt/qml/ImageViewerApp/resources/icons/icon.png"
                sourceSize: Qt.size(32, 32)
                smooth: true
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: root.window.title
                color: themeManager.textPrimary
            }
        }
    }

    Row {
        id: buttons
        anchors.right: parent.right
        height: parent.height

        component CaptionButton: Rectangle {
            id: btn
            property bool danger: false
            property bool nativeHover: false // Windows owns the maximise button's mouse (WindowsFrame.cpp)
            property alias glyph: glyphItem.sourceComponent
            property string label: ""
            signal clicked
            width: 46
            height: parent.height
            readonly property bool over: hover.hovered || nativeHover
            color: over ? (danger ? "#e81123" : Qt.rgba(themeManager.textPrimary.r, themeManager.textPrimary.g, themeManager.textPrimary.b, 0.14)) : "transparent"
            readonly property color ink: danger && over ? "white" : themeManager.textPrimary
            Accessible.role: Accessible.Button
            Accessible.name: label
            Loader { id: glyphItem; anchors.centerIn: parent }
            HoverHandler { id: hover }
            TapHandler { onTapped: btn.clicked() }
            AppToolTip { visible: btn.over; text: btn.label }
        }

        CaptionButton {
            objectName: "minimizeButton"
            label: qsTr("Minimizar")
            onClicked: root.window.showMinimized()
            glyph: Component { Rectangle { width: 10; height: 1; color: parent.parent.ink } }
        }
        CaptionButton {
            objectName: "maximizeButton"
            label: root.maximized ? qsTr("Restaurar") : qsTr("Maximizar")
            onClicked: root.toggleMaximize()
            glyph: Component {
                Item {
                    width: 10; height: 10
                    Rectangle { // restored: a second square behind
                        visible: root.maximized
                        x: 2; y: 0; width: 8; height: 8; color: "transparent"; border.color: parent.parent.parent.ink
                    }
                    Rectangle {
                        x: 0; y: root.maximized ? 2 : 0; width: root.maximized ? 8 : 10; height: root.maximized ? 8 : 10
                        color: root.maximized ? themeManager.surface : "transparent"
                        border.color: parent.parent.parent.ink
                    }
                }
            }
        }
        CaptionButton {
            objectName: "closeButton"
            danger: true
            label: qsTr("Cerrar")
            onClicked: root.window.close()
            glyph: Component {
                Item {
                    width: 10; height: 10
                    Rectangle { anchors.centerIn: parent; width: 14; height: 1; rotation: 45; color: parent.parent.parent.ink }
                    Rectangle { anchors.centerIn: parent; width: 14; height: 1; rotation: -45; color: parent.parent.parent.ink }
                }
            }
        }
    }
}
