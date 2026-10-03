import QtQuick
import QtQuick.Controls
import ImageViewerApp

// "Cargando…" as a pill that fills up, in place of a spinner. The decoder cannot say how far
// along it is, so the fill is an estimate: it moves fast at first and slows as it nears
// the end (it never claims to be done), then jumps to full the moment the picture arrives
// and fades away. It stays hidden for the first moments, so a picture that opens at once
// never flashes it.
Item {
    id: root

    // True while a picture is being opened.
    property bool loading: false
    // Seconds before the pill shows up at all.
    property real showAfter: 0.25

    property real progress: 0
    property real elapsed: 0
    property bool shown: false

    implicitWidth: card.width
    implicitHeight: card.height
    visible: card.opacity > 0.01

    onLoadingChanged: {
        if (loading) {
            finishAnim.stop();
            fadeOut.stop();
            progress = 0;
            elapsed = 0;
            shown = false;
            tick.start();
        } else {
            tick.stop();
            if (shown) {
                finishAnim.restart(); // the last stretch, then fade out
            } else {
                card.opacity = 0;
            }
        }
    }

    Timer {
        id: tick
        interval: 33
        repeat: true
        onTriggered: {
            root.elapsed += interval / 1000;
            if (!root.shown && root.elapsed >= root.showAfter) {
                root.shown = true;
                card.opacity = 1;
            }
            // 1 - e^(-t/1.6), capped: ~63 % after 1.6 s, ~85 % after 3 s, never above 94 %.
            root.progress = 0.94 * (1 - Math.exp(-root.elapsed / 1.6));
        }
    }

    SequentialAnimation {
        id: finishAnim
        NumberAnimation { target: root; property: "progress"; to: 1; duration: 140; easing.type: Easing.OutCubic }
        PauseAnimation { duration: 120 }
        NumberAnimation { id: fadeOut; target: card; property: "opacity"; to: 0; duration: 220 }
    }

    Rectangle {
        id: card
        opacity: 0
        width: 230
        height: 66
        radius: themeManager.radiusMedium === 0 ? 0 : 14
        color: themeManager.surfaceElevated
        border.color: themeManager.border
        border.width: 1
        Behavior on opacity { NumberAnimation { duration: 160 } }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 13
            text: qsTr("Abriendo imagen…")
            color: themeManager.textPrimary
        }

        // the pill
        Rectangle {
            id: track
            x: 20
            y: 40
            width: parent.width - 40
            height: 12
            radius: themeManager.radiusMedium === 0 ? 0 : height / 2
            color: themeManager.border
            clip: true

            Rectangle {
                width: Math.max(parent.height, parent.width * root.progress)
                height: parent.height
                radius: parent.radius
                color: themeManager.accent
            }
        }
    }
}
