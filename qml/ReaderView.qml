import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ImageViewerApp

// "Leer": the comic / manga reader. A book (a folder of pictures or a .cbz) is read one spread at a time, right to
// left (the manga way) or left to right, with one page or two side by side. What is on screen and where the book is
// comes from `mangaReader` (MangaReader.cpp); this file is the view: the pages, the keys, the click zones; the controls (page
// slider and switches) are in the window's dock.
//
//  - Left / right (and the click zones at both sides of the screen) turn the page in the direction of the reading,
//    so with right-to-left reading the left side goes FORWARD. Space, Page Down and the wheel go forward as well;
//    Page Up, Backspace and the wheel the other way go back. Home and End jump to the first and last page.
//  - D: double page, R: reading direction, C: the cover stands alone, 1 / 2 / 3: fit the screen / the height / the
//    width. Esc closes the reader.
//  - When the spread is larger than the view (fit to height / width) the wheel and the arrow keys scroll it, and
//    only turn the page once the edge is reached.
FocusScope {
    id: root

    signal closeRequested()

    // The dock of the bottom bar rises over the page: the spread is fitted (and centred) above it, and a taller one
    // scrolls up past it, as the picture does in the viewer.
    property int bottomInset: 0
    readonly property real viewH: Math.max(0, height - bottomInset)
    readonly property bool rtl: mangaReader.rightToLeft
    readonly property string fit: mangaReader.fit

    // ---- size of the spread on screen -----------------------------------------------------------------------
    property real totalRatio: 0.7 // width / height of everything on screen together
    readonly property real spreadH: fit === "width" ? width / totalRatio
                                  : fit === "height" ? viewH
                                  : Math.min(viewH, width / totalRatio)
    readonly property real spreadW: spreadH * totalRatio

    function recalc() {
        let total = 0;
        for (let i = 0; i < pagesRepeater.count; ++i)
            total += pagesRepeater.itemAt(i) ? pagesRepeater.itemAt(i).ratio : 0.7;
        totalRatio = pagesRepeater.count > 0 && total > 0 ? total : 0.7;
    }

    // Decoded as large as the view needs, and the reader prefetches the next pages at that same size.
    Binding {
        target: mangaReader
        property: "pageSide"
        value: Math.ceil(Math.max(root.spreadH, root.spreadW) * Screen.devicePixelRatio / 256) * 256
    }

    function resetScroll() {
        flick.contentY = 0;
        flick.contentX = root.rtl ? Math.max(0, flick.contentWidth - flick.width) : 0;
    }
    Connections {
        target: mangaReader
        function onPositionChanged() { Qt.callLater(root.resetScroll) }
    }
    onFitChanged: Qt.callLater(resetScroll)

    // ---- turning pages --------------------------------------------------------------------------------------
    function forward() { mangaReader.next() }
    function backward() { mangaReader.previous() }
    // Toward the left / right of the screen: which of the two it is depends on the direction of reading.
    function toLeft() { rtl ? forward() : backward() }
    function toRight() { rtl ? backward() : forward() }

    // Scrolls a spread that is larger than the view; false when there was nothing (more) to scroll that way.
    function scrollBy(dx, dy) {
        const maxX = Math.max(0, flick.contentWidth - flick.width),
              maxY = Math.max(0, flick.contentHeight + flick.bottomMargin - flick.height);
        const x = Math.max(0, Math.min(maxX, flick.contentX + dx)), y = Math.max(0, Math.min(maxY, flick.contentY + dy));
        const moved = x !== flick.contentX || y !== flick.contentY;
        flick.contentX = x;
        flick.contentY = y;
        return moved;
    }
    // One step in the reading direction: scrolls first when the spread is taller than the view, then turns the page.
    function step(forwardDir) {
        if (forwardDir ? !scrollBy(0, flick.height * 0.85) : !scrollBy(0, -flick.height * 0.85))
            forwardDir ? forward() : backward();
    }

    Timer { id: wheelGate; interval: 140 }

    // ---- the pages ------------------------------------------------------------------------------------------
    Rectangle { anchors.fill: parent; color: themeManager.background }

    Flickable {
        id: flick
        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: Math.max(root.spreadW, width)
        bottomMargin: root.bottomInset
        contentHeight: Math.max(root.spreadH, root.viewH)
        // The sharpening (the button in the bottom bar, shown while reading): on the pages as they are drawn.
        layer.enabled: appSettings.viewSharpen > 0
        layer.effect: CasEffect {}

        Row {
            id: spread
            x: (flick.contentWidth - width) / 2
            y: (flick.contentHeight - height) / 2
            height: root.spreadH
            Repeater {
                id: pagesRepeater
                model: mangaReader.pages
                onItemAdded: root.recalc()
                onItemRemoved: root.recalc()
                delegate: Item {
                    id: slot
                    required property var modelData
                    // The real shape when it is known (the page was read in the background); otherwise what the
                    // picture says once it has loaded.
                    readonly property real ratio: modelData.width > 0 && modelData.height > 0
                                                  ? modelData.width / modelData.height
                                                  : (img.implicitHeight > 0 ? img.implicitWidth / img.implicitHeight : 0.7)
                    onRatioChanged: root.recalc()
                    height: root.spreadH
                    width: root.spreadH * ratio

                    Rectangle { anchors.fill: parent; color: themeManager.surface; visible: img.status !== Image.Ready }
                    Image {
                        id: img
                        anchors.fill: parent
                        source: slot.modelData.path !== "" ? "image://pane/" + encodeURIComponent(slot.modelData.path) : ""
                        sourceSize: Qt.size(mangaReader.pageSide, mangaReader.pageSide)
                        asynchronous: true
                        cache: false
                        smooth: true
                        mipmap: true
                        fillMode: Image.PreserveAspectFit
                    }
                }
            }
        }
    }

    // The wheel and the click zones sit ABOVE the Flickable: it is a child of this item and so gets every press first,
    // and it would keep them from a handler placed beside it. Handlers here take their share (a passive grab for the
    // tap) and the Flickable still drags.
    Item {
        anchors.fill: parent

        // Wheel: scrolls what does not fit, otherwise turns the page.
        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: function (event) {
                const dy = event.angleDelta.y;
                if (dy === 0)
                    return;
                if (root.scrollBy(0, -dy * 0.8))
                    return;
                if (wheelGate.running)
                    return;
                wheelGate.start();
                dy < 0 ? root.forward() : root.backward();
            }
        }

        // Click zones: the outer thirds turn the page (the middle one does nothing, so a click there is safe).
        TapHandler {
            onTapped: function (eventPoint) {
                const x = eventPoint.position.x;
                if (x < root.width * 0.3)
                    root.toLeft();
                else if (x > root.width * 0.7)
                    root.toRight();
            }
        }
    }

    // Keys.
    Keys.onPressed: function (event) {
        event.accepted = true;
        switch (event.key) {
        case Qt.Key_Left: root.toLeft(); break;
        case Qt.Key_Right: root.toRight(); break;
        case Qt.Key_Space: case Qt.Key_PageDown: case Qt.Key_Down: root.step(true); break;
        case Qt.Key_Backspace: case Qt.Key_PageUp: case Qt.Key_Up: root.step(false); break;
        case Qt.Key_Home: mangaReader.first(); break;
        case Qt.Key_End: mangaReader.last(); break;
        case Qt.Key_Escape: root.closeRequested(); break;
        case Qt.Key_D: mangaReader.doublePage = !mangaReader.doublePage; break;
        case Qt.Key_R: mangaReader.rightToLeft = !mangaReader.rightToLeft; break;
        case Qt.Key_C: mangaReader.coverAlone = !mangaReader.coverAlone; break;
        case Qt.Key_1: mangaReader.fit = "screen"; break;
        case Qt.Key_2: mangaReader.fit = "height"; break;
        case Qt.Key_3: mangaReader.fit = "width"; break;
        default: event.accepted = false;
        }
    }
    Component.onCompleted: forceActiveFocus()

    // ---- the controls ---------------------------------------------------------------------------------------
    // They are shown in the dock of the window's bottom bar (StatusBar.qml) while the reader is open, as the comparer's
    // are. The buttons never take the keyboard focus (focusPolicy), so the keys above keep working after a click.
    readonly property Component dockContent: Component {
        Row {
            spacing: 4

            AppToolButton {
                hPad: 10; iconSize: 22; iconOnly: true; focusPolicy: Qt.NoFocus
                iconName: "prev"; text: qsTr("Hacia la izquierda")
                onClicked: root.toLeft()
            }
            AppSlider {
                id: slider
                anchors.verticalCenter: parent.verticalCenter
                width: 260
                focusPolicy: Qt.NoFocus
                from: 0
                to: Math.max(1, mangaReader.pageCount - 1)
                stepSize: 1
                value: mangaReader.currentPage
                // Right to left reading: the end of the book is on the left, so the slider runs backwards too.
                LayoutMirroring.enabled: root.rtl
                LayoutMirroring.childrenInherit: true
                onMoved: mangaReader.goToPage(Math.round(value))
            }
            AppToolButton {
                hPad: 10; iconSize: 22; iconOnly: true; focusPolicy: Qt.NoFocus
                iconName: "next"; text: qsTr("Hacia la derecha")
                onClicked: root.toRight()
            }

            Rectangle { width: 1; height: 24; color: themeManager.border; anchors.verticalCenter: parent.verticalCenter }

            AppToolButton {
                hPad: 10; iconSize: 22; iconOnly: true; focusPolicy: Qt.NoFocus
                iconName: root.rtl ? "dir-rtl" : "dir-ltr"
                text: root.rtl ? qsTr("Lectura de derecha a izquierda (R)") : qsTr("Lectura de izquierda a derecha (R)")
                onClicked: mangaReader.rightToLeft = !mangaReader.rightToLeft
            }
            AppToolButton {
                hPad: 10; iconSize: 22; iconOnly: true; focusPolicy: Qt.NoFocus
                iconName: "multi"
                checkable: true
                checked: mangaReader.doublePage
                text: qsTr("Página doble (D)")
                onToggled: mangaReader.doublePage = checked
            }
            AppToolButton {
                hPad: 10; iconSize: 22; iconOnly: true; focusPolicy: Qt.NoFocus
                iconName: "page"
                visible: mangaReader.doublePage
                checkable: true
                checked: mangaReader.coverAlone
                text: qsTr("La portada va sola (C)")
                onToggled: mangaReader.coverAlone = checked
            }
            AppToolButton {
                hPad: 10; iconSize: 22; iconOnly: true; focusPolicy: Qt.NoFocus
                iconName: root.fit === "height" ? "fit-height" : root.fit === "width" ? "fit-width" : "fit"
                text: root.fit === "height" ? qsTr("Ajustar al alto (1, 2, 3)")
                      : root.fit === "width" ? qsTr("Ajustar al ancho (1, 2, 3)") : qsTr("Ajustar a la pantalla (1, 2, 3)")
                onClicked: mangaReader.fit = root.fit === "screen" ? "height" : root.fit === "height" ? "width" : "screen"
            }
            AppToolButton {
                hPad: 10; iconSize: 22; iconOnly: true; focusPolicy: Qt.NoFocus
                iconName: "close"; text: qsTr("Cerrar el lector (Esc)")
                onClicked: root.closeRequested()
            }
        }
    }
}
