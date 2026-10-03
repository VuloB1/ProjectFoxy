import QtQuick
import ImageViewerApp

// A numeric field that supports both direct typing and a Photoshop/Blender
// -style "scrub" drag: press and drag the mouse left/right to raise/lower
// the value continuously, without needing to type. A plain click (press and
// release without moving) instead focuses the text for typing an exact
// value. `valueEdited` fires only for genuine user commits (drag steps or a
// finished edit) - never for a value assigned programmatically by a caller -
// so callers can hook it up without fighting their own resets.
Item {
    id: root
    property real value: 0
    property real from: 0
    property real to: 999999
    property int decimals: 0
    property real dragStep: 1 // value change per pixel dragged
    property string suffix: ""
    signal valueEdited(real newValue)

    implicitWidth: 92
    implicitHeight: 32

    Loader {
        anchors.fill: parent
        sourceComponent: themeManager.skin === "win98" ? win98Bg : flatBg

        Component {
            id: flatBg
            Rectangle {
                anchors.fill: parent
                radius: themeManager.radiusSmall
                color: themeManager.surfaceElevated
                border.color: input.activeFocus || hoverArea.hovered ? themeManager.accent : themeManager.border
                border.width: 1
            }
        }

        Component {
            id: win98Bg
            Win98Bevel {
                anchors.fill: parent
                sunken: true
                face: themeManager.fieldFace
                contentInset: 2
            }
        }
    }

    HoverHandler { id: hoverArea }

    function clampValue(v) {
        return Math.min(to, Math.max(from, v));
    }

    function commit(v) {
        const c = clampValue(Number(v.toFixed(decimals)));
        if (c !== value)
            value = c;
        valueEdited(c);
    }

    function refreshText() {
        if (!input.activeFocus)
            input.text = value.toFixed(decimals) + suffix;
    }

    onValueChanged: refreshText()
    Component.onCompleted: refreshText()

    TextInput {
        id: input
        anchors.fill: parent
        anchors.margins: 6
        verticalAlignment: TextInput.AlignVCenter
        horizontalAlignment: TextInput.AlignHCenter
        color: themeManager.textPrimary
        selectByMouse: true
        validator: DoubleValidator { bottom: root.from; top: root.to; decimals: root.decimals }

        onEditingFinished: {
            const parsed = parseFloat(text);
            if (!isNaN(parsed))
                root.commit(parsed);
            else
                root.refreshText();
        }
    }

    MouseArea {
        id: dragArea
        anchors.fill: parent
        cursorShape: Qt.SizeHorCursor
        preventStealing: true
        property real pressX: 0
        property real startValue: 0
        property bool dragging: false

        onPressed: function (mouse) {
            pressX = mouse.x;
            startValue = root.value;
            dragging = false;
        }
        onPositionChanged: function (mouse) {
            if (!pressed)
                return;
            if (!dragging && Math.abs(mouse.x - pressX) > 4)
                dragging = true;
            if (dragging)
                root.commit(startValue + (mouse.x - pressX) * root.dragStep);
        }
        onReleased: function () {
            if (!dragging) {
                input.forceActiveFocus();
                input.selectAll();
            }
            dragging = false;
        }
    }
}
