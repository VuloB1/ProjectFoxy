import QtQuick

// Mouse-wheel scrolling at one comfortable, fixed speed for every scrollable
// list/panel. Qt's own Flickable wheel handling scrolls only a few pixels per
// notch, which feels sluggish in the tall edit panels. Declare it inside the
// Flickable/ListView it should drive: `AppWheelScroll { view: theFlickable }`.
// Touchpads keep their native smooth scrolling (they report pixel deltas).
// A list that runs sideways (the frame strip of the GIF studio) sets `horizontal: true`: the wheel then moves it along.
WheelHandler {
    id: root

    required property Flickable view
    // Pixels moved per wheel notch.
    property real step: 110
    property bool horizontal: false

    acceptedDevices: PointerDevice.Mouse

    property real targetY: 0
    property NumberAnimation slide: NumberAnimation {
        target: root.view
        property: root.horizontal ? "contentX" : "contentY"
        duration: 130
        easing.type: Easing.OutCubic
    }

    onWheel: function (event) {
        const v = root.view;
        const h = root.horizontal;
        const minY = h ? v.originX - v.leftMargin : v.originY - v.topMargin;
        const maxY = Math.max(minY, h ? v.originX + v.contentWidth + v.rightMargin - v.width
                                      : v.originY + v.contentHeight + v.bottomMargin - v.height);
        const current = h ? v.contentX : v.contentY;
        // Successive notches build on where the slide is heading, not on
        // where it currently is, so a fast spin covers the full distance.
        const from = root.slide.running ? root.targetY : current;
        const notches = event.angleDelta.y / 120 * (event.inverted ? -1 : 1);
        root.targetY = Math.max(minY, Math.min(maxY, from - notches * root.step));
        root.slide.stop();
        root.slide.from = current;
        root.slide.to = root.targetY;
        root.slide.start();
    }
}
