import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Themed replacement for the stock ToolButton: transparent at rest, a
// tinted pill reveals on hover/press, a persistent tint when checked (e.g.
// the "Editar"/slideshow toggles). Same Loader-per-skin pattern as
// AppButton.qml.
ToolButton {
    id: control

    // Optional icon shown above the label - unset (default) behaves exactly
    // as before (plain single-line label). Named iconName (not `icon`)
    // because AbstractButton already has a FINAL `icon` grouped property.
    // Value is one of the names AppIcon.qml understands, not a glyph.
    property string iconName: ""

    // Set by a bar that wants its first (-1) and last (1) button to follow the
    // bar's own rounded ends (see FloatingToolbar.qml); 0 = an ordinary button.
    property int edge: 0
    readonly property real edgeRadius: themeManager.radiusMedium === 0 ? 0 : 26
    readonly property real leftCorner: edge === -1 ? edgeRadius : themeManager.radiusSmall
    readonly property real rightCorner: edge === 1 ? edgeRadius : themeManager.radiusSmall

    // Horizontal breathing room around the content; a bar that wants airier buttons raises it.
    property int hPad: 6
    // Extra room on the outer side of a bar's first / last button (see `edge`).
    property int outerPad: 10
    leftPadding: hPad + (edge === -1 ? outerPad : 0)
    rightPadding: hPad + (edge === 1 ? outerPad : 0)

    // Icon only: the text is not drawn but becomes the hover tooltip.
    property bool iconOnly: false
    // Tint of the icon (the label always uses the primary text colour).
    property color iconColor: themeManager.textPrimary

    // Measures the label text independently of any Item's width/layout.
    // Needed because contentItem's own implicit width can't be used for
    // this: once the label below has `elide` set, its implicitWidth becomes
    // a function of its OWN current width, so if control.implicitWidth is
    // left to derive from contentItem as usual, and that label's width in
    // turn reads control.availableWidth, the whole thing is circular and
    // collapses every button down toward zero width. Overriding implicitWidth
    // directly from this independent measurement (bypassing contentItem's
    // sizing entirely) breaks that loop.
    //
    // The +4 exists because TextMetrics measures ~1-2px narrower than the
    // same Label's own natural (pre-elide) width - close enough that without
    // it, the label below ends up very slightly under its actual needed
    // width and silently ellipsizes text that should fit.
    implicitWidth: (iconOnly ? 22 : labelMetrics.width + 4) + leftPadding + rightPadding

    AppToolTip {
        visible: control.iconOnly && control.hovered
        text: control.text
    }

    TextMetrics {
        id: labelMetrics
        font: control.font
        text: control.text
    }

    // Hover/press tint with the text colour rather than controlFace: in the dark
    // theme controlFace is the very colour of the bars these buttons sit on, so
    // a hover pill in it was invisible.
    readonly property real fillOpacity: control.checked ? 0.35
        : control.pressed ? 0.24
        : control.hovered ? 0.14
        : 0.0
    readonly property color fillColor: control.checked ? themeManager.accent : themeManager.textPrimary

    background: Loader {
        sourceComponent: themeManager.skin === "win98" ? win98Bg : modernBg

        Component {
            id: modernBg
            Rectangle {
                topLeftRadius: control.leftCorner
                bottomLeftRadius: control.leftCorner
                topRightRadius: control.rightCorner
                bottomRightRadius: control.rightCorner
                color: control.fillColor
                opacity: control.fillOpacity
                Behavior on opacity { NumberAnimation { duration: themeManager.animFast } }
            }
        }

        // Classic "coolbar" behavior: flat at rest, a raised bevel on
        // hover, sunken when pressed or checked (toggled on).
        Component {
            id: win98Bg
            Win98Bevel {
                visible: control.hovered || control.pressed || control.checked
                sunken: control.pressed || control.checked
                contentInset: 0
            }
        }
    }

    contentItem: Column {
        spacing: 2

        AppIcon {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: control.iconName.length > 0
            name: control.iconName
            size: 22
            color: control.iconColor
            opacity: control.enabled ? 1.0 : 0.5
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: !control.iconOnly
            // Capped at control.availableWidth so a button squeezed by its
            // container (e.g. a TabBar dividing space between tabs) elides
            // instead of spilling into its neighbor. Safe now that
            // control.implicitWidth above no longer derives from this label.
            width: Math.min(labelMetrics.width + 4, control.availableWidth)
            text: control.text
            font: control.font
            color: themeManager.textPrimary
            opacity: control.enabled ? 1.0 : 0.5
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
}
