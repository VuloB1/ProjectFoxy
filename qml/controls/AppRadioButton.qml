import QtQuick
import QtQuick.Controls
import ImageViewerApp

// One option of a "pick exactly one" list: a round indicator plus a label.
// Deliberately NOT a stock RadioButton - its `checked` state is a plain input
// here (bind it to whatever owns the choice) and a click only emits
// clicked(), so the choice can never drift from the model it represents.
Item {
    id: root

    property string text: ""
    property bool checked: false
    signal clicked()

    implicitWidth: indicatorBox.width + 6 + label.implicitWidth
    implicitHeight: 24

    readonly property bool hovered: hover.hovered
    HoverHandler { id: hover }

    Loader {
        id: indicatorBox
        width: 16
        height: 16
        anchors.verticalCenter: parent.verticalCenter
        sourceComponent: themeManager.skin === "win98" ? win98Indicator : modernIndicator

        Component {
            id: modernIndicator
            Rectangle {
                radius: width / 2
                color: themeManager.surfaceElevated
                // The plain theme border is nearly invisible on the light
                // theme's white surface, so an unselected option gets the
                // (softened) text color as its outline.
                border.color: root.checked ? themeManager.accent
                              : Qt.rgba(themeManager.textSecondary.r, themeManager.textSecondary.g,
                                        themeManager.textSecondary.b, 0.65)
                border.width: root.checked ? 2 : 1
                opacity: root.enabled ? 1.0 : 0.5

                Rectangle {
                    anchors.centerIn: parent
                    width: 8
                    height: 8
                    radius: 4
                    visible: root.checked
                    color: themeManager.accent
                }
            }
        }

        // A sunken white disc with a black dot, like the real control. Drawn
        // on a Canvas because Win98Bevel can only do rectangles.
        Component {
            id: win98Indicator
            Canvas {
                onPaint: {
                    const ctx = getContext("2d");
                    ctx.reset();
                    ctx.beginPath();
                    ctx.arc(8, 8, 7, 0, 2 * Math.PI);
                    ctx.fillStyle = themeManager.fieldFace;
                    ctx.fill();
                    ctx.lineWidth = 1;
                    ctx.beginPath();
                    ctx.arc(8, 8, 7, Math.PI * 0.75, Math.PI * 1.75);
                    ctx.strokeStyle = themeManager.bevelDark;
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.arc(8, 8, 7, Math.PI * 1.75, Math.PI * 2.75);
                    ctx.strokeStyle = themeManager.bevelLightSoft;
                    ctx.stroke();
                    if (root.checked) {
                        ctx.beginPath();
                        ctx.arc(8, 8, 3, 0, 2 * Math.PI);
                        ctx.fillStyle = themeManager.textPrimary;
                        ctx.fill();
                    }
                }
                Component.onCompleted: requestPaint()
                Connections {
                    target: themeManager
                    function onThemeChanged() { requestPaint(); }
                }
                Connections {
                    target: root
                    function onCheckedChanged() { requestPaint(); }
                }
            }
        }
    }

    Label {
        id: label
        anchors.left: indicatorBox.right
        anchors.leftMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        text: root.text
        color: root.checked ? themeManager.textPrimary : themeManager.textSecondary
        opacity: root.enabled ? 1.0 : 0.5
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
