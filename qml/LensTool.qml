import QtQuick
import QtQuick.Controls
import ImageViewerApp

// Corrección de lente: removes the distortion, the colour fringes and the dark corners of a lens,
// from the calibration of the camera + lens used (the Lensfun database, read in the background the
// first time) or, when there is no profile, with hand-set sliders. The correction is the hidden
// "lens" effect: this page only works out its numbers and puts them in.
Item {
    id: root

    property var canvas: null

    readonly property bool canApply: appController.effectId === "lens" && !appController.effectBusy
    function apply() { appController.commitEffect(); }
    function cancel() { appController.cancelEffect(); }

    // "profile" or "manual"
    property string mode: "profile"
    property string maker: ""
    property string model: ""
    property int lensIndex: -1
    property var info: ({})
    property real focal: 35
    property real aperture: 5.6
    property bool useDistortion: true
    property bool useTca: true
    property bool useVignette: true
    property bool autoScale: true
    property real strength: 100
    property real mDistortion: 0
    property real mFringes: 0
    property real mVignette: 0
    property var profile: ({ distortion: false, tca: false, vignetting: false, note: "" })
    property bool guessedFromPhoto: false

    property var makerList: []
    property var modelList: []
    property var lensList: []         // [{index, name}]
    readonly property var lensNames: lensList.map(function (l) { return l.name; })

    function reloadMakers() { makerList = lensController.makers(); }
    function reloadModels() { modelList = maker !== "" ? lensController.models(maker) : []; }
    function reloadLenses() {
        lensList = lensController.lenses(maker, model, lensFilter.text);
        // keep the chosen lens in the list even when the search hides it
        const at = lensList.findIndex(function (l) { return l.index === lensIndex; });
        lensCombo.currentIndex = at;
    }
    function selectLens(index) {
        lensIndex = index;
        info = lensController.lensInfo(index);
        if (info.focalMin !== undefined) {
            focal = Math.min(info.focalMax, Math.max(info.focalMin, focal));
            focalSlider.value = focal;
        }
        recompute();
    }
    function init() {
        reloadMakers();
        const hints = appController.lensHints;
        const guess = lensController.guess(hints);
        if (guess.maker) {
            maker = guess.maker;
            model = guess.model;
            guessedFromPhoto = true;
        }
        if (hints.focal > 0) focal = hints.focal;
        if (hints.aperture > 0) aperture = Math.min(32, Math.max(1, hints.aperture));
        apertureSlider.value = aperture;
        reloadModels();
        reloadLenses();
        if (guess.lensIndex >= 0)
            selectLens(guess.lensIndex);
        else
            recompute();
    }
    function putValues(values) {
        if (appController.effectId !== "lens")
            appController.selectEffect("lens");
        appController.setEffectValues(values);
    }
    function recompute() {
        if (mode === "manual") {
            putValues(lensController.manual(mDistortion, mFringes, mVignette, autoScale));
            return;
        }
        if (lensIndex < 0) {
            appController.cancelEffect();
            return;
        }
        const r = lensController.compute(maker, model, lensIndex, focal, aperture, 1000,
                                         useDistortion, useTca, useVignette, autoScale, strength);
        profile = { distortion: r.distortion, tca: r.tca, vignetting: r.vignetting, note: r.note };
        putValues(r.values);
    }

    Component.onCompleted: {
        lensController.load();
        if (lensController.ready)
            init();
    }
    Connections {
        target: lensController
        function onReadyChanged() { if (lensController.ready) root.init(); }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight + 8
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: AppScrollBar { id: bar }
        AppWheelScroll { view: flick }

        Column {
            id: column
            width: flick.width - bar.implicitWidth - 4
            spacing: 10

            Label {
                width: parent.width
                text: qsTr("Corrige la distorsión, las franjas de color y el oscurecimiento de las esquinas con el perfil de tu cámara y tu objetivo.")
                color: themeManager.textSecondary
                wrapMode: Text.WordWrap
            }

            Row {
                spacing: 4
                AppToolButton { text: qsTr("Con perfil"); checked: root.mode === "profile"; onClicked: { root.mode = "profile"; root.recompute(); } }
                AppToolButton { text: qsTr("A mano"); checked: root.mode === "manual"; onClicked: { root.mode = "manual"; root.recompute(); } }
            }

            // ---------------------------------------------------------------- profile mode
            Column {
                width: parent.width
                spacing: 10
                visible: root.mode === "profile"

                Label {
                    width: parent.width
                    visible: !lensController.ready
                    text: qsTr("Cargando la base de cámaras y objetivos…")
                    color: themeManager.accent
                }

                Column {
                    width: parent.width
                    spacing: 6
                    enabled: lensController.ready

                    Label { text: qsTr("Cámara"); color: themeManager.textSecondary }
                    AppComboBox {
                        width: parent.width
                        model: root.makerList
                        currentIndex: root.makerList.indexOf(root.maker)
                        displayText: root.maker !== "" ? root.maker : qsTr("Marca…")
                        onActivated: function (i) {
                            root.maker = root.makerList[i];
                            root.model = "";
                            root.guessedFromPhoto = false;
                            root.reloadModels();
                            root.reloadLenses();
                        }
                    }
                    AppComboBox {
                        width: parent.width
                        model: root.modelList
                        currentIndex: root.modelList.indexOf(root.model)
                        displayText: root.model !== "" ? root.model : qsTr("Modelo…")
                        enabled: root.maker !== ""
                        onActivated: function (i) {
                            root.model = root.modelList[i];
                            root.guessedFromPhoto = false;
                            root.reloadLenses();
                            root.recompute();
                        }
                    }
                    Label {
                        width: parent.width
                        visible: root.guessedFromPhoto
                        text: qsTr("Cámara y objetivo reconocidos por los datos de la foto.")
                        color: themeManager.accent
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }

                Column {
                    width: parent.width
                    spacing: 6
                    enabled: lensController.ready

                    Label { text: qsTr("Objetivo"); color: themeManager.textSecondary }
                    AppTextField {
                        id: lensFilter
                        width: parent.width
                        height: 30
                        placeholderText: qsTr("Buscar (por ejemplo 18-55, 50mm, nikkor)…")
                        onTextChanged: root.reloadLenses()
                    }
                    AppComboBox {
                        id: lensCombo
                        width: parent.width
                        model: root.lensNames
                        displayText: root.lensIndex >= 0 && root.info.name ? root.info.name : qsTr("Elegí un objetivo…")
                        onActivated: function (i) { root.selectLens(root.lensList[i].index); }
                    }
                    Label {
                        width: parent.width
                        visible: root.lensList.length === 0 && lensController.ready
                        text: qsTr("No hay objetivos que coincidan.")
                        color: themeManager.textSecondary
                        font.pixelSize: 11
                    }
                }

                // The data the lens has.
                Label {
                    width: parent.width
                    visible: root.lensIndex >= 0
                    wrapMode: Text.WordWrap
                    color: themeManager.textSecondary
                    font.pixelSize: 11
                    text: root.lensIndex < 0 ? "" :
                          qsTr("Perfil: distorsión %1 · aberración %2 · viñeta %3")
                              .arg(root.info.distortion ? "✓" : "✗").arg(root.info.tca ? "✓" : "✗").arg(root.info.vignetting ? "✓" : "✗")
                          + (root.info.type && root.info.type !== "rectilinear" ? "\n" + qsTr("Objetivo de tipo %1: solo se corrige la forma, no se cambia la proyección.").arg(root.info.type) : "")
                          + (root.profile.note ? "\n" + root.profile.note : "")
                }

                Column {
                    width: parent.width
                    spacing: 0
                    visible: root.lensIndex >= 0

                    Row {
                        width: parent.width
                        Label { text: qsTr("Distancia focal"); color: themeManager.textSecondary; width: parent.width - focalValue.width }
                        Label { id: focalValue; text: Math.round(focalSlider.value * 10) / 10 + " mm"; color: themeManager.textPrimary; font.bold: true }
                    }
                    AppSlider {
                        id: focalSlider
                        width: parent.width
                        from: root.info.focalMin !== undefined ? root.info.focalMin : 10
                        to: root.info.focalMax !== undefined && root.info.focalMax > from ? root.info.focalMax : from + 1
                        value: root.focal
                        onMoved: { root.focal = value; root.recompute(); }
                    }
                    Item { width: 1; height: 8 }
                    Row {
                        width: parent.width
                        Label { text: qsTr("Apertura"); color: themeManager.textSecondary; width: parent.width - apertureValue.width }
                        Label { id: apertureValue; text: "f/" + (Math.round(apertureSlider.value * 10) / 10); color: themeManager.textPrimary; font.bold: true }
                    }
                    AppSlider {
                        id: apertureSlider
                        width: parent.width
                        from: 1; to: 32
                        value: root.aperture
                        onMoved: { root.aperture = value; root.recompute(); }
                    }
                }

                Column {
                    width: parent.width
                    spacing: 2
                    visible: root.lensIndex >= 0
                    AppCheckBox { text: qsTr("Corregir la distorsión"); checked: root.useDistortion; enabled: root.info.distortion === true; onToggled: { root.useDistortion = checked; root.recompute(); } }
                    AppCheckBox { text: qsTr("Quitar las franjas de color"); checked: root.useTca; enabled: root.info.tca === true; onToggled: { root.useTca = checked; root.recompute(); } }
                    AppCheckBox { text: qsTr("Corregir las esquinas oscuras"); checked: root.useVignette; enabled: root.info.vignetting === true; onToggled: { root.useVignette = checked; root.recompute(); } }
                    AppCheckBox { text: qsTr("Quitar los bordes vacíos"); checked: root.autoScale; onToggled: { root.autoScale = checked; root.recompute(); } }
                }

                Column {
                    width: parent.width
                    visible: root.lensIndex >= 0
                    Row {
                        width: parent.width
                        Label { text: qsTr("Fuerza"); color: themeManager.textSecondary; width: parent.width - strengthValue.width }
                        Label { id: strengthValue; text: Math.round(strengthSlider.value) + "%"; color: strengthSlider.value < 99.5 ? themeManager.accent : themeManager.textPrimary; font.bold: true }
                    }
                    AppSlider {
                        id: strengthSlider
                        width: parent.width
                        from: 0; to: 100
                        value: root.strength
                        snapThreshold: 3
                        snapValue: 100
                        onMoved: { root.strength = value; root.recompute(); }
                    }
                }
            }

            // ---------------------------------------------------------------- manual mode
            Column {
                width: parent.width
                spacing: 8
                visible: root.mode === "manual"

                Label {
                    width: parent.width
                    text: qsTr("Para objetivos que no están en la base: ajustá cada cosa mirando la imagen.")
                    color: themeManager.textSecondary
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
                Column {
                    width: parent.width
                    Row {
                        width: parent.width
                        Label { text: qsTr("Distorsión"); color: themeManager.textSecondary; width: parent.width - dValue.width }
                        Label { id: dValue; text: (dSlider.value > 0 ? "+" : "") + Math.round(dSlider.value); color: dSlider.value !== 0 ? themeManager.accent : themeManager.textPrimary; font.bold: true }
                    }
                    AppSlider {
                        id: dSlider
                        width: parent.width
                        from: -100; to: 100; value: root.mDistortion
                        snapThreshold: 3; snapValue: 0; neutralMark: true; fillFrom: 0
                        onMoved: { root.mDistortion = value; root.recompute(); }
                    }
                    Label { text: qsTr("Positivo corrige un barril (líneas curvadas hacia afuera)."); color: themeManager.textSecondary; font.pixelSize: 11; width: parent.width; wrapMode: Text.WordWrap }
                }
                Column {
                    width: parent.width
                    Row {
                        width: parent.width
                        Label { text: qsTr("Franjas de color"); color: themeManager.textSecondary; width: parent.width - fValue.width }
                        Label { id: fValue; text: (fSlider.value > 0 ? "+" : "") + Math.round(fSlider.value); color: fSlider.value !== 0 ? themeManager.accent : themeManager.textPrimary; font.bold: true }
                    }
                    AppSlider {
                        id: fSlider
                        width: parent.width
                        from: -100; to: 100; value: root.mFringes
                        snapThreshold: 3; snapValue: 0; neutralMark: true; fillFrom: 0
                        onMoved: { root.mFringes = value; root.recompute(); }
                    }
                }
                Column {
                    width: parent.width
                    Row {
                        width: parent.width
                        Label { text: qsTr("Esquinas"); color: themeManager.textSecondary; width: parent.width - vValue.width }
                        Label { id: vValue; text: (vSlider.value > 0 ? "+" : "") + Math.round(vSlider.value); color: vSlider.value !== 0 ? themeManager.accent : themeManager.textPrimary; font.bold: true }
                    }
                    AppSlider {
                        id: vSlider
                        width: parent.width
                        from: -100; to: 100; value: root.mVignette
                        snapThreshold: 3; snapValue: 0; neutralMark: true; fillFrom: 0
                        onMoved: { root.mVignette = value; root.recompute(); }
                    }
                    Label { text: qsTr("Positivo aclara las esquinas, negativo las oscurece."); color: themeManager.textSecondary; font.pixelSize: 11; width: parent.width; wrapMode: Text.WordWrap }
                }
                AppCheckBox { text: qsTr("Quitar los bordes vacíos"); checked: root.autoScale; onToggled: { root.autoScale = checked; root.recompute(); } }
            }
        }
    }
}
