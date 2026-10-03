import QtQuick
import QtQuick.Controls
import ImageViewerApp

// One entry of the Filtros grid: a small preview of the CURRENT photo with the
// look already applied (rendered by the image provider), and the look's name
// underneath. An empty lookId is the "Ninguno" cell - the untouched photo.
Item {
    id: root

    property string lookId: ""
    // "look" (the Filtros grid) or "effect" (the Efectos grid): which family of
    // previews the image provider renders for this cell. lookId then holds the
    // effect's id.
    property string kind: "look"
    property string label: ""
    property bool selected: false
    signal picked()

    implicitWidth: 80
    implicitHeight: width + 18

    Rectangle {
        id: frame
        width: parent.width
        height: width
        radius: themeManager.radiusSmall
        color: themeManager.surfaceElevated
        border.width: root.selected || hover.hovered ? 2 : 1
        border.color: root.selected ? themeManager.accent : (hover.hovered ? Qt.rgba(themeManager.accent.r, themeManager.accent.g, themeManager.accent.b, 0.55) : themeManager.border)

        Image {
            anchors.fill: parent
            anchors.margins: 2
            fillMode: Image.PreserveAspectCrop
            smooth: true
            asynchronous: true
            // Nothing to show until the C++ side has rendered the previews
            // for this photo (revision 0 = not yet).
            source: appController.lookThumbRevision > 0
                ? "image://viewer/" + (root.kind === "effect" ? "effectthumb/" : "lookthumb/")
                  + (root.lookId === "" ? "none" : root.lookId)
                  + "?rev=" + appController.lookThumbRevision
                : ""
        }

        HoverHandler { id: hover }
        TapHandler { onTapped: root.picked() }
    }

    Label {
        anchors.top: frame.bottom
        anchors.topMargin: 2
        width: parent.width
        text: root.label
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: 11
        color: root.selected || hover.hovered ? themeManager.textPrimary : themeManager.textSecondary
        font.bold: root.selected
    }
}
