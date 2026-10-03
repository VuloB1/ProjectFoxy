import QtQuick

// Mouse-wheel scrolling at one comfortable, fixed speed for every scrollable
// list/panel. Qt's own Flickable wheel handling scrolls only a few pixels per
// notch, which feels sluggish in the tall edit panels. Declare it inside the
// Flickable/ListView it should drive: `AppWheelScroll { view: theFlickable }`.
// Touchpads keep their native smooth scrolling (they report pixel deltas).
WheelHandler {
    id: root

    required property Flickable view
    // Pixels moved per wheel notch.
    property real step: 110

    acceptedDevices: PointerDevice.Mouse

    property real targetY: 0
    property NumberAnimation slide: NumberAnimation {
        target: root.view
        property: "contentY"
        duration: 130
        easing.type: Easing.OutCubic
    }

    onWheel: function (event) {
        const v = root.view;
        const minY = v.originY - v.topMargin;
        const maxY = Math.max(minY, v.originY + v.contentHeight + v.bottomMargin - v.height);
        // Successive notches build on where the slide is heading, not on
        // where it currently is, so a fast spin covers the full distance.
        const from = root.slide.running ? root.targetY : v.contentY;
        const notches = event.angleDelta.y / 120 * (event.inverted ? -1 : 1);
        root.targetY = Math.max(minY, Math.min(maxY, from - notches * root.step));
        root.slide.stop();
        root.slide.from = v.contentY;
        root.slide.to = root.targetY;
        root.slide.start();
    }
}
