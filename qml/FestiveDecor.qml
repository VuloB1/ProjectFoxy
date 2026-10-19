import QtQuick

// The holiday decorations over the window (resources/festive/*.svg, drawn by tools/make_festive_art.js):
// a garland along the top edge of the picture area, a piece in the bottom corner of the thumbnail bar and, for
// Halloween, a cobweb in the corner of the picture area. Nothing here takes the mouse, and only the modern themes
// get them. FestiveManager (the "festive" context property) says which holiday it is, if any.
Item {
    id: root

    // What it hangs from; set by Main.qml.
    property Item titleBar
    property Item sidebar
    property Item statusBar
    property Item canvas
    property real rightInset: 0       // the width of the edit panel when it is open
    property bool suppressed: false   // studios and the reader take the whole window

    readonly property string key: festive.festival
    readonly property bool shown: key !== "" && themeManager.skin !== "win98" && !suppressed

    // width / height of each corner piece, so that the room it needs is known before it is drawn
    readonly property var aspect: ({
        halloween: 1.106, muertos: 0.653, navidad: 1.238, nuevoano: 1.0,
        reyes: 0.677, sanvalentin: 0.88, sanpatricio: 1.55, pascua: 1.25
    })
    readonly property bool heroRoom: shown && !!sidebar && sidebar.visible && sidebar.width >= 70
    readonly property real heroWidth: sidebar ? sidebar.width - 8 : 0
    readonly property real heroHeight: Math.min(128, heroWidth / (aspect[key] || 1))
    // The space the thumbnail bar leaves free for the corner piece (it never covers a thumbnail).
    readonly property real heroReserve: heroRoom ? heroHeight + 8 : 0

    visible: shown
    enabled: false // not even hover: the picture below stays fully usable

    Image {
        id: garland
        visible: root.shown
        x: root.canvas ? root.canvas.x : 0
        y: root.titleBar ? root.titleBar.height : 0
        width: root.canvas ? root.canvas.width - root.rightInset : 0
        height: 48
        source: root.shown ? "qrc:/festive/" + root.key + "-garland.svg" : ""
        fillMode: Image.TileHorizontally
        verticalAlignment: Image.AlignTop
        sourceSize.height: height * Screen.devicePixelRatio
        asynchronous: true
        smooth: true
        opacity: 0.95
    }

    Image {
        id: web
        visible: root.shown && root.key === "halloween" && !!root.canvas
        x: root.canvas ? root.canvas.x + root.canvas.width - root.rightInset - width : 0
        y: root.titleBar ? root.titleBar.height : 0
        width: 96
        height: 96
        source: visible ? "qrc:/festive/halloween-web.svg" : ""
        sourceSize.height: height * Screen.devicePixelRatio
        asynchronous: true
        smooth: true
        opacity: 0.8
    }

    Image {
        id: hero
        visible: root.heroRoom
        x: root.sidebar ? 4 : 0
        width: root.heroWidth
        height: root.heroHeight
        y: (root.statusBar ? root.statusBar.y : root.height) - height - 4
        source: visible ? "qrc:/festive/" + root.key + "-hero.svg" : ""
        fillMode: Image.PreserveAspectFit
        horizontalAlignment: Image.AlignHCenter
        verticalAlignment: Image.AlignBottom
        sourceSize.height: height * Screen.devicePixelRatio
        asynchronous: true
        smooth: true
    }
}
