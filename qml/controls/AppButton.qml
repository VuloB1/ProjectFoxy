import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Themed replacement for the stock Button: same public API (text, onClicked,
// enabled, checkable/checked, ...) so call sites only change the type name.
// background/contentItem branch on themeManager.skin via a Loader - the
// pattern every other qml/controls/App*.qml wrapper repeats, so a future
// theme is a new Component + one more branch, not a rewrite.
Button {
    id: control

    // Optional icon shown above the label - unset (default), this behaves
    // exactly as before (plain single-line label), so every existing call
    // site is unaffected. Named iconName (not `icon`) because AbstractButton
    // already has a FINAL `icon` grouped property (icon.source/color/...)
    // that QML refuses to let a subclass override. Value is one of the names
    // AppIcon.qml understands, not a glyph.
    property string iconName: ""

    // Shows only the icon (needs an iconName). `text` is still required: it
    // becomes the hover tooltip, so the button stays self-explanatory.
    property bool iconOnly: false
    // The one action a dialog is about (accent fill) / a destructive one (red fill). Modern
    // skins only: the flat Win98 look has a single kind of button.
    property bool primary: false
    property bool danger: false
    readonly property bool filled: (primary || danger) && themeManager.skin !== "win98"
    readonly property color labelColor: filled ? (danger ? "#ffffff" : themeManager.accentText) : themeManager.textPrimary
    // Set by a bar that wants its first (-1) and last (1) button to follow the
    // bar's own rounded ends (see FloatingToolbar.qml); 0 = an ordinary button.
    property int edge: 0
    readonly property real edgeRadius: themeManager.radiusMedium === 0 ? 0 : 26
    readonly property real leftCorner: edge === -1 ? edgeRadius : themeManager.radiusSmall
    readonly property real rightCorner: edge === 1 ? edgeRadius : themeManager.radiusSmall

    // See AppToolButton.qml for why this has to be measured independently
    // and used to override implicitWidth directly, instead of letting it
    // derive from contentItem the usual way (and for the +4). A call site
    // that sets an explicit `width:` (e.g. a row of equal-width buttons)
    // still wins over this - implicitWidth is only ever a fallback.
    implicitWidth: (iconOnly ? 22 : labelMetrics.width + 4) + leftPadding + rightPadding

    TextMetrics {
        id: labelMetrics
        font: control.font
        text: control.text
    }

    AppToolTip {
        visible: control.iconOnly && control.hovered
        text: control.text
    }

    readonly property color faceColor: {
        const base = control.filled ? (control.danger ? Qt.color("#e5484d") : themeManager.accent) : themeManager.controlFace;
        if (!control.enabled) return base;
        if (control.filled) {
            if (control.pressed) return Qt.darker(base, 1.2);
            return control.hovered ? Qt.lighter(base, 1.12) : base;
        }
        // A selected choice (e.g. the crop's current aspect ratio): the accent, darkened.
        if (control.checked) return Qt.tint(base, Qt.rgba(themeManager.accent.r, themeManager.accent.g, themeManager.accent.b, control.hovered ? 0.5 : 0.38));
        if (control.pressed) return Qt.darker(base, 1.2);
        // Tinted with the text colour: lightening the face alone was barely visible on dark themes.
        if (control.hovered) return Qt.tint(base, Qt.rgba(themeManager.textPrimary.r, themeManager.textPrimary.g, themeManager.textPrimary.b, 0.14));
        return base;
    }

    background: Loader {
        sourceComponent: themeManager.skin === "win98" ? win98Bg : modernBg

        Component {
            id: modernBg
            Rectangle {
                implicitHeight: 30
                topLeftRadius: control.leftCorner
                bottomLeftRadius: control.leftCorner
                topRightRadius: control.rightCorner
                bottomRightRadius: control.rightCorner
                color: control.faceColor
                border.color: control.filled ? "transparent" : control.checked ? themeManager.accent : themeManager.border
                border.width: 1
                opacity: control.enabled ? 1.0 : 0.5
                Behavior on color { ColorAnimation { duration: themeManager.animFast } }
            }
        }

        Component {
            id: win98Bg
            Win98Bevel {
                implicitHeight: 26
                sunken: control.pressed
            }
        }
    }

    contentItem: Column {
        spacing: 2

        // Win98's sunken-when-pressed bevel shifts the whole face down/right
        // by 1px - nudge the label to match, the classic "pressed" tell. This
        // MUST be a `transform`, not `x`/`y` directly: Control auto-centers
        // contentItem within the button by binding its x, but only when x
        // isn't already set - assigning x/y here directly (as an earlier
        // version of this file did) silently defeated that centering for any
        // button wider than its content (e.g. the aspect-ratio preset grid),
        // left-pinning the label instead of centering it. `transform` is a
        // separate, purely visual offset applied after layout, so it doesn't
        // fight Control's own positioning.
        transform: Translate {
            x: themeManager.skin === "win98" && control.pressed ? 1 : 0
            y: themeManager.skin === "win98" && control.pressed ? 1 : 0
        }

        AppIcon {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: control.iconName.length > 0
            name: control.iconName
            size: 22
            color: control.labelColor
            opacity: control.enabled ? 1.0 : 0.5
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: !control.iconOnly
            // Capped at control.availableWidth so a button squeezed by its
            // container (e.g. an explicit `width:` set at the call site)
            // elides instead of spilling into its neighbor. Safe now that
            // control.implicitWidth above no longer derives from this label.
            width: Math.min(labelMetrics.width + 4, control.availableWidth)
            text: control.text
            font: control.font
            color: control.labelColor
            opacity: control.enabled ? 1.0 : 0.5
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
}
