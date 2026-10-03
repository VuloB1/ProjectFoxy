import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Themed replacement for the stock Slider: same public API (value/from/to/
// onMoved/...). Modern = flat groove + accent fill + circular handle.
Slider {
    id: control

    // Opt-in "magnetic center" - while dragging, a raw value that lands
    // within snapThreshold of `snapValue` gets clamped to exactly that value
    // instead, so it's easy to land back on the neutral value (e.g.
    // brightness/straighten, or 100% in the resize scale) without
    // micromanaging the mouse. Moving further keeps sliding normally since
    // the clamp simply stops applying once the raw value is outside the
    // threshold. Off (snapThreshold: 0) by default.
    property real snapThreshold: 0
    property real snapValue: 0

    // Where the accent fill starts. Defaults to the left end (an ordinary
    // "how much" slider); a signed slider sets it to its neutral value so the
    // fill grows outward from the middle in whichever direction it is moved.
    property real fillFrom: from

    // Three colours (left, middle, right) the groove is painted with instead of the
    // plain accent fill - for sliders whose position IS a colour (temperature, tint).
    // Empty = an ordinary groove.
    property var trackColors: []
    // A small tick on the groove at `snapValue`, so the resting place is easy to see.
    property bool neutralMark: false

    // Snapped as soon as the drag produces the value - i.e. BEFORE moved()
    // fires - so a call site's onMoved already sees the snapped number
    // (snapping from a Connections{onMoved} ran after the call site's own
    // handler, which had already used the raw value).
    onValueChanged: {
        if (pressed && snapThreshold > 0 && value !== snapValue
                && Math.abs(value - snapValue) <= snapThreshold)
            value = snapValue;
    }

    background: Loader {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: 4
        sourceComponent: themeManager.skin === "win98" ? win98Groove : modernGroove

        Component {
            id: modernGroove
            Rectangle {
                anchors.fill: parent
                radius: height / 2
                color: themeManager.border
                opacity: control.enabled ? 1.0 : 0.5

                // the colour of the values along the slider
                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    visible: control.trackColors.length === 3
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: control.trackColors.length === 3 ? control.trackColors[0] : "transparent" }
                        GradientStop { position: 0.5; color: control.trackColors.length === 3 ? control.trackColors[1] : "transparent" }
                        GradientStop { position: 1.0; color: control.trackColors.length === 3 ? control.trackColors[2] : "transparent" }
                    }
                }

                Rectangle {
                    visible: control.trackColors.length !== 3
                    readonly property real startPos: control.to === control.from
                        ? 0 : (control.fillFrom - control.from) / (control.to - control.from)
                    x: Math.min(startPos, control.visualPosition) * parent.width
                    width: Math.abs(control.visualPosition - startPos) * parent.width
                    height: parent.height
                    radius: parent.radius
                    color: themeManager.accent
                }

                Rectangle {
                    visible: control.neutralMark && control.to > control.from
                    readonly property real pos: (control.snapValue - control.from) / (control.to - control.from)
                    x: pos * (parent.width - 16) + 8 - width / 2
                    y: -5
                    width: 2
                    height: parent.height + 10
                    radius: 1
                    color: themeManager.textSecondary
                }
            }
        }

        // A real Win98 trackbar groove is a plain sunken line with no
        // color fill - only the thumb's position shows the value.
        Component {
            id: win98Groove
            Win98Bevel {
                anchors.fill: parent
                sunken: true
                contentInset: 1
            }
        }
    }

    handle: Loader {
        readonly property bool isWin98: themeManager.skin === "win98"
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: isWin98 ? 11 : 16
        height: isWin98 ? 22 : 16
        sourceComponent: themeManager.skin === "win98" ? win98Handle : modernHandle

        Component {
            id: modernHandle
            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: themeManager.surfaceElevated
                border.color: themeManager.accent
                border.width: 2
                opacity: control.enabled ? 1.0 : 0.5
            }
        }

        // The classic trackbar thumb: a tall raised bevel block, taller
        // than the groove it rides in.
        Component {
            id: win98Handle
            Win98Bevel {
                anchors.fill: parent
                sunken: control.pressed
                contentInset: 2
            }
        }
    }
}
