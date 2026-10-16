import QtQuick
import QtQuick.Controls
import ImageViewerApp

// "Acerca de" page of the settings: the program, who made it, how to support it, thanks, and the licences.
// Loaded by SettingsDialog.qml only while that page is open.
Item {
    id: root
    width: parent ? parent.width : 0
    implicitHeight: column.implicitHeight

    // Where the support button leads (Ko-fi). Empty hides that card.
    readonly property string supportUrl: "https://ko-fi.com/vulito"
    readonly property string repoUrl: "https://github.com/VuloB1/ProjectFoxy"

    // A rounded panel, like the Card of the settings pages but with its content free to lay out.
    component Panel: Rectangle {
        id: panel
        default property alias content: inner.data
        property string heading: ""
        // A tint that fades out downwards from the top edge (the program card).
        property color glow: "transparent"
        property real glowY: 52 // where the glow is centred, from the card's top
        width: parent ? parent.width : 0
        implicitHeight: inner.implicitHeight + (heading.length > 0 ? 54 : 32)
        radius: themeManager.radiusMedium === 0 ? 0 : 12
        color: themeManager.surface
        border.color: themeManager.border
        border.width: 1
        // (a radial glow is not available as an item, so it is painted)
        Canvas {
            visible: panel.glow.a > 0
            anchors.fill: parent
            canvasSize: Qt.size(width, height)
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            Connections { target: panel; function onGlowChanged() { requestPaint(); } }
            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                const w = width, h = height, r = panel.radius;
                const cx = w / 2, cy = panel.glowY; // the middle of the icon
                ctx.beginPath();
                ctx.moveTo(r, 0); ctx.lineTo(w - r, 0); ctx.arcTo(w, 0, w, r, r);
                ctx.lineTo(w, h - r); ctx.arcTo(w, h, w - r, h, r);
                ctx.lineTo(r, h); ctx.arcTo(0, h, 0, h - r, r);
                ctx.lineTo(0, r); ctx.arcTo(0, 0, r, 0, r);
                ctx.closePath();
                ctx.clip();
                const g = ctx.createRadialGradient(cx, cy, 0, cx, cy, w * 0.5);
                const c = panel.glow;
                g.addColorStop(0, Qt.rgba(c.r, c.g, c.b, 0.34));
                g.addColorStop(0.5, Qt.rgba(c.r, c.g, c.b, 0.12));
                g.addColorStop(1, Qt.rgba(c.r, c.g, c.b, 0));
                ctx.fillStyle = g;
                ctx.fillRect(0, 0, w, h);
            }
        }
        Label {
            visible: parent.heading.length > 0
            x: 18; y: 14
            text: parent.heading
            color: themeManager.textSecondary
            font.bold: true
            font.pixelSize: 11
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 0.8
        }
        Column {
            id: inner
            x: 18
            y: parent.heading.length > 0 ? 40 : 16
            width: parent.width - 36
            spacing: 10
        }
    }

    // A button that opens a web page.
    component LinkButton: Rectangle {
        id: link
        property string label
        property string url
        property string iconName: "link"
        property bool wide: false
        width: wide ? parent.width : (parent.width - 10) / 2
        height: 38
        radius: themeManager.radiusMedium === 0 ? 0 : 10
        color: hover.hovered ? themeManager.controlFace : "transparent"
        border.color: hover.hovered ? themeManager.accent : themeManager.border
        border.width: 1
        Row {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 12
            spacing: 10
            AppIcon { anchors.verticalCenter: parent.verticalCenter; name: link.iconName; size: 16; color: themeManager.accent }
            Label { anchors.verticalCenter: parent.verticalCenter; text: link.label; color: themeManager.textPrimary }
        }
        HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: Qt.openUrlExternally(link.url) }
        Accessible.role: Accessible.Link
        Accessible.name: link.label
        AppToolTip { visible: hover.hovered; text: link.url }
    }

    // A round badge with a letter, standing in for a picture.
    component Badge: Rectangle {
        property string letter
        width: 44; height: 44; radius: 22
        color: Qt.rgba(themeManager.accent.r, themeManager.accent.g, themeManager.accent.b, 0.2)
        border.color: themeManager.accent
        Label { anchors.centerIn: parent; text: parent.letter; color: themeManager.accent; font.bold: true; font.pixelSize: 18 }
    }

    Column {
        id: column
        width: parent.width
        spacing: 14

        // ---- the program
        Panel {
            glow: themeManager.accent
            Column {
                width: parent.width
                spacing: 8
                Image {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 72; height: 72
                    source: "qrc:/qt/qml/ImageViewerApp/resources/icons/icon.png"
                    sourceSize: Qt.size(144, 144)
                    smooth: true
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Project Foxy"
                    color: themeManager.textPrimary
                    font.bold: true
                    font.pixelSize: 22
                }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: versionLabel.implicitWidth + 24
                    height: 24
                    radius: 12
                    color: "transparent"
                    border.color: themeManager.border
                    Label {
                        id: versionLabel
                        anchors.centerIn: parent
                        text: qsTr("Versión %1").arg(Qt.application.version)
                        color: themeManager.textSecondary
                        font.pixelSize: 12
                    }
                }
                Label {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: qsTr("Un visor y editor de imágenes rápido, con tus archivos siempre contigo.")
                    color: themeManager.textSecondary
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    topPadding: 6
                    text: qsTr("Creado con")
                    color: themeManager.textSecondary
                    font.bold: true
                    font.pixelSize: 11
                    font.capitalization: Font.AllUppercase
                    font.letterSpacing: 0.8
                }
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 8
                    Repeater {
                        model: ["Qt 6", "QML", "C++20"]
                        delegate: Rectangle {
                            required property string modelData
                            width: chip.implicitWidth + 20
                            height: 24
                            radius: 12
                            color: "transparent"
                            border.color: themeManager.border
                            Label { id: chip; anchors.centerIn: parent; text: parent.modelData; color: themeManager.textPrimary; font.pixelSize: 12 }
                        }
                    }
                }
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 6
                    topPadding: 4
                    AppIcon { anchors.verticalCenter: parent.verticalCenter; name: "effects"; size: 14; color: themeManager.accent }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Desarrollado con la ayuda de Claude, de Anthropic")
                        color: themeManager.textSecondary
                        font.pixelSize: 12
                    }
                }
            }
        }

        // ---- updates
        Panel {
            heading: qsTr("Actualizaciones")
            Label {
                width: parent.width
                wrapMode: Text.WordWrap
                color: themeManager.textPrimary
                text: {
                    switch (updater.state) {
                    case "checking": return qsTr("Buscando una versión nueva…")
                    case "upToDate": return qsTr("Tienes la última versión.")
                    case "available": case "downloading": case "ready": case "installing":
                        return qsTr("Hay una versión nueva: %1").arg(updater.latestVersion)
                    case "failed": return qsTr("No se pudo comprobar la versión.")
                    default: return qsTr("Versión instalada: %1").arg(updater.currentVersion)
                    }
                }
            }
            Row {
                spacing: 10
                AppButton {
                    text: qsTr("Buscar ahora")
                    enabled: updater.state !== "checking" && updater.state !== "downloading" && updater.state !== "installing"
                    onClicked: updater.checkNow()
                }
                AppButton {
                    visible: updater.state === "available"
                    primary: true
                    text: updater.canInstall ? qsTr("Actualizar ahora") : qsTr("Descargar")
                    onClicked: updater.download()
                }
            }
            Item {
                width: parent.width
                height: 40
                Label {
                    anchors.left: parent.left
                    anchors.right: autoSwitch.left
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    wrapMode: Text.WordWrap
                    color: themeManager.textSecondary
                    font.pixelSize: 12
                    text: qsTr("Buscar actualizaciones al iniciar (una consulta a GitHub al día, nada más).")
                }
                AppSwitch {
                    id: autoSwitch
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    checked: appSettings.updateCheck
                    onToggled: appSettings.updateCheck = checked
                }
            }
        }

        // ---- the author
        Panel {
            Row {
                spacing: 14
                Image {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 56; height: 56
                    source: "qrc:/qt/qml/ImageViewerApp/resources/images/creator.png"
                    sourceSize: Qt.size(112, 112)
                    smooth: true
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Label {
                        text: qsTr("Creador y autor")
                        color: themeManager.textSecondary
                        font.bold: true
                        font.pixelSize: 11
                        font.capitalization: Font.AllUppercase
                        font.letterSpacing: 0.8
                    }
                    Label { text: "Vulito"; color: themeManager.textPrimary; font.bold: true; font.pixelSize: 17 }
                }
            }
            LinkButton { wide: true; label: qsTr("Sitio web") + " · vulito.carrd.co"; url: "https://vulito.carrd.co" }
            Flow {
                width: parent.width
                spacing: 10
                LinkButton { label: "@VulitoVT"; url: "https://x.com/VulitoVT" }
                LinkButton { label: "@VulitoVRC"; url: "https://x.com/VulitoVRC" }
                LinkButton { label: "@vulitovt"; url: "https://www.instagram.com/vulitovt" }
                LinkButton { label: "VuloB1"; url: "https://github.com/VuloB1" }
            }
            Rectangle {
                visible: root.supportUrl.length > 0
                width: parent.width
                height: supportColumn.implicitHeight + 24
                radius: themeManager.radiusMedium === 0 ? 0 : 10
                color: "transparent"
                border.color: themeManager.border
                Column {
                    id: supportColumn
                    x: 14; y: 12
                    width: parent.width - 28
                    spacing: 10
                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        text: qsTr("Si Project Foxy te gusta, puedes apoyar su desarrollo.")
                        color: themeManager.textSecondary
                        font.pixelSize: 12
                    }
                    AppButton {
                        width: parent.width
                        primary: true
                        text: qsTr("Invítame a un café en Ko-fi")
                        onClicked: Qt.openUrlExternally(root.supportUrl)
                    }
                }
            }
        }

        // ---- thanks. `url` is optional: when it is filled in, the name gets a link on its right.
        Panel {
            heading: qsTr("Agradecimientos")
            Flow {
                width: parent.width
                spacing: 10
                Repeater {
                    model: [
                        { name: "SymphonyVR", url: "" },
                        { name: "Emi~", url: "" },
                        { name: "Seita!", url: "" },
                        { name: "Ryonikku", url: "" },
                        { name: "Tofu", url: "" }
                    ]
                    delegate: Row {
                        id: person
                        required property var modelData
                        width: (parent.width - 10) / 2
                        spacing: 10
                        Badge { letter: person.modelData.name.charAt(0); width: 32; height: 32; radius: 16; anchors.verticalCenter: parent.verticalCenter }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 32 - 10 - (person.modelData.url !== "" ? 22 : 0)
                            text: person.modelData.name
                            color: themeManager.textPrimary
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        AppIcon {
                            visible: person.modelData.url !== ""
                            anchors.verticalCenter: parent.verticalCenter
                            name: "link"; size: 14; color: themeManager.accent
                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: Qt.openUrlExternally(person.modelData.url) }
                        }
                    }
                }
            }
        }

        // ---- legal
        Panel {
            heading: qsTr("Legal")
            Flow {
                width: parent.width
                spacing: 10
                LinkButton { label: qsTr("Licencia (GPL-3.0)"); url: root.repoUrl + "/blob/main/LICENSE" }
                LinkButton { label: qsTr("Licencias de terceros"); url: root.repoUrl + "/blob/main/docs/THIRD_PARTY_NOTICES.txt" }
            }
        }
    }
}
