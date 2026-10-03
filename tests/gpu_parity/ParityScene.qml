import QtQuick

// The live-preview chain of qml/ImageCanvas.qml, minus everything about zoom, pan
// and the editor panels, drawn 1:1 so that one pixel of the window is one pixel of
// the picture:
//
//   image -> Grade (the Filtros look) -> Grade (the Ajustes sliders) -> Detail
//
// A Grade stage that changes nothing is switched off (it frees its texture), so
// Detail reads the newest stage that runs.
//
// It reads the very same AppController properties and the very same shaders, so
// what the test grabs from this window is what the canvas would show. KEEP IT IN
// STEP with ImageCanvas.qml: if the chain there changes, change it here too.
Item {
    id: root

    Image {
        id: image
        source: appController.currentSource
        visible: false
        smooth: true
        mipmap: true
        asynchronous: false
        cache: false
        width: sourceSize.width > 0 ? sourceSize.width : root.width
        height: sourceSize.height > 0 ? sourceSize.height : root.height
    }

    Image {
        id: lookLut
        source: appController.lookLutSource
        visible: false
        smooth: false
        mipmap: false
        asynchronous: false
        cache: false
    }

    ShaderEffect {
        id: lookEffect
        visible: appController.lookActive
        width: image.width
        height: image.height
        property variant source: image
        property variant lut: lookLut
        property real shadows: appController.look.shadows
        property real highlights: appController.look.highlights
        property real saturation: appController.look.saturation
        property real vibrance: appController.look.vibrance
        property real hue: appController.look.hue
        property real negative: 0
        property real amount: appController.look.amount
        property real gradientOn: appController.look.gradientOn
        property real vignette: appController.look.vignette
        property real grain: appController.look.grain
        property real grainType: appController.look.grainType
        property real grainMono: appController.look.grainMono
        property size resolution: Qt.size(image.width, image.height)
        property vector3d gradient0: appController.look.gradient0
        property vector3d gradient1: appController.look.gradient1
        property vector3d gradient2: appController.look.gradient2
        property vector3d shadowOffset: appController.look.shadowOffset
        property vector3d highlightOffset: appController.look.highlightOffset
        fragmentShader: "qrc:/shaders/Grade.frag.qsb"
    }
    ShaderEffectSource {
        id: lookSource
        sourceItem: appController.lookActive ? lookEffect : null
        hideSource: true
        live: true
        mipmap: !appController.gradeActive
    }

    Image {
        id: adjustLut
        source: appController.adjustLutSource
        visible: false
        smooth: false
        mipmap: false
        asynchronous: false
        cache: false
    }

    ShaderEffect {
        id: adjustEffect
        visible: appController.gradeActive
        width: image.width
        height: image.height
        property variant source: appController.lookActive ? lookSource : image
        property variant lut: adjustLut
        property real shadows: appController.adjust.shadows
        property real highlights: appController.adjust.highlights
        property real saturation: appController.adjust.saturation
        property real vibrance: appController.adjust.vibrance
        property real hue: appController.adjust.hue
        property real negative: appController.adjust.negative ? 1.0 : 0.0
        property real amount: 1.0
        property real gradientOn: 0
        property real vignette: 0
        property real grain: 0
        property real grainType: 1
        property real grainMono: 0
        property size resolution: Qt.size(image.width, image.height)
        property vector3d gradient0: Qt.vector3d(0, 0, 0)
        property vector3d gradient1: Qt.vector3d(0, 0, 0)
        property vector3d gradient2: Qt.vector3d(0, 0, 0)
        property vector3d shadowOffset: Qt.vector3d(0, 0, 0)
        property vector3d highlightOffset: Qt.vector3d(0, 0, 0)
        fragmentShader: "qrc:/shaders/Grade.frag.qsb"
    }
    ShaderEffectSource {
        id: adjustSource
        sourceItem: appController.gradeActive ? adjustEffect : null
        hideSource: true
        live: true
        mipmap: true
    }

    ShaderEffect {
        id: detailEffect
        x: 0
        y: 0
        width: image.width
        height: image.height
        property variant source: appController.gradeActive ? adjustSource
            : appController.lookActive ? lookSource : image
        property real amount: appController.adjust.sharpness
        property real clarity: appController.adjust.clarity
        property real vignette: appController.adjust.vignette
        property real grain: appController.adjust.grain
        property real grainType: appController.adjust.grainType
        property real grainMono: appController.adjust.grainMono ? 1.0 : 0.0
        property size resolution: Qt.size(image.width, image.height)
        property size texelSize: Qt.size(1.0 / Math.max(1, image.width), 1.0 / Math.max(1, image.height))
        fragmentShader: "qrc:/shaders/Detail.frag.qsb"
    }
}
