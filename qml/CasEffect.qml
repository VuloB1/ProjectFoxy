import QtQuick

// The reader's sharpening (StatusBar.qml's button, shown while a book is open): FidelityFX CAS (Cas.frag) used as the `layer.effect` of whatever
// shows the picture, so it works on the pixels that are really on the screen. `strength` is the 0..100 % setting.
// Sits at 0 (and costs nothing: the layer is not even made) while the setting is off.
ShaderEffect {
    property real strength: appSettings.viewSharpen / 100
    // one texel of the layer, in texture coordinates: the layer has the item's size in device pixels
    property vector2d texel: Qt.vector2d(1 / Math.max(1, width * Screen.devicePixelRatio), 1 / Math.max(1, height * Screen.devicePixelRatio))
    fragmentShader: "qrc:/shaders/Cas.frag.qsb"
}
