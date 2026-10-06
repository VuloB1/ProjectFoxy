import QtQuick
import QtQuick.Controls
import ImageViewerApp

// The main viewport: GPU-composited pan/zoom over the current image.
// Defaults to appController.currentSource (set via Toolbar's Abrir dialog or
// a drag&drop onto Main.qml); the `source` alias lets a caller override it.
//
// Zoom model: `fitMode` tracks whether the image should always be rescaled
// to the viewport (default, capped at 100% so small images aren't blown up).
// Any explicit zoom action (buttons, wheel, double-click to 1:1) drops into
// manual mode; "Ajustar" or a double-click back returns to fit mode.
Rectangle {
    id: root
    color: themeManager.background

    property alias source: image.source
    property real minScale: 0.05
    property real maxScale: 16.0
    property bool fitMode: true
    // Set by Main.qml, same property-passing convention as `canvas` on
    // Toolbar/FloatingToolbar - needed so the right-click context menu below
    // can toggle the edit panel and reuse Toolbar's delete-confirmation
    // dialog instead of duplicating it here.
    property var toolbar: null
    // Edit mode lives in Main.qml; the context menu asks it to switch.
    property bool editMode: false
    // A tool is open in the edit panel (see Main.qml): no leaving edit mode, no deleting the file.
    property bool locked: false
    signal editModeRequested(bool on)
    // Before/after compare toggle (EditPanel's "Comparar" button, held down):
    // while true, the final visible stage shows the as-opened original
    // instead of the live-edited GPU chain output. Not pixel-aligned with
    // the edited view after a crop/resize (it's the full original image
    // dimensions), which is fine for a rough before/after glance.
    property bool compareOriginal: false
    readonly property int zoomPercent: Math.round(image.scale * 100)

    // --- Modo pixel (Configuración > Apariencia) ------------------------------------------
    // The mode itself: magnified pixels stay hard-edged (no smoothing from 100 % up) and the
    // zoom reaches 64x. Everything else is an extra with its own switch in the settings:
    //   integerZoom   - zoom moves in whole-number steps, a small picture opens enlarged
    //   checkerboard  - behind the transparent parts
    //   grid          - between the picture's own pixels, from 8x
    //   readout       - position and colour of the pixel under the cursor
    //   (and appSettings.pixelSharpEdit for Tamaño / Enderezar, which live outside the canvas)
    readonly property bool pixelMode: appSettings.pixelMode
    readonly property bool integerZoom: pixelMode && appSettings.pixelIntegerZoom
    readonly property bool nearest: pixelMode && image.scale >= 1
    readonly property real topScale: pixelMode ? 64 : maxScale
    readonly property var pixelSteps: [0.05, 0.1, 0.125, 0.25, 0.5, 1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 32, 40, 48, 64]
    // (later: topScale, which the fit scale reads, has to have followed the switch first)
    onPixelModeChanged: Qt.callLater(function () { if (fitMode) zoomToFit(); })
    onIntegerZoomChanged: Qt.callLater(function () { if (fitMode) zoomToFit(); })

    // The next zoom stop above (dir > 0) or below `current`.
    function stepScale(current, dir) {
        const steps = pixelSteps;
        if (dir > 0) {
            for (let i = 0; i < steps.length; ++i)
                if (steps[i] > current * 1.0001)
                    return steps[i];
            return current;
        }
        for (let i = steps.length - 1; i >= 0; --i)
            if (steps[i] < current / 1.0001)
                return steps[i];
        return current;
    }

    // While true, the rotation Behavior is disabled so an assignment to
    // image.rotation jumps instantly instead of animating - used when a new
    // image is loaded and rotation is reset to 0, so it doesn't visibly spin
    // back from whatever angle the previous image was left at.
    property bool suppressRotationAnimation: false

    // The Behavior on rotation animates image.rotation over time, so reading
    // it back mid-animation gives an interpolated (not the intended target)
    // value. Clicking "Rotar" fast enough would then accumulate off the
    // animated value instead of the real target, leaving the image a few
    // degrees off from 0/90/180/270. Track the target separately instead.
    property int targetRotation: 0

    function computeFitScale() {
        if (image.sourceSize.width <= 0 || image.sourceSize.height <= 0)
            return 1.0;
        const ratio = Math.min(flick.width / image.sourceSize.width,
                                flick.height / image.sourceSize.height);
        // With whole-number zoom, a small picture opens as big as a whole number allows.
        if (integerZoom && ratio >= 1.0)
            return Math.min(Math.floor(ratio), topScale);
        return Math.min(ratio, 1.0);
    }

    function centerContent() {
        flick.contentX = (flick.contentWidth - flick.width) / 2;
        flick.contentY = (flick.contentHeight - flick.height) / 2;
    }

    function zoomToFit() {
        fitMode = true;
        image.scale = computeFitScale();
        centerContent();
    }

    // zoomToFit() is also what window resizes and new images call behind the
    // scenes - those must not flash the zoom readout. This is the variant for
    // the user explicitly asking to fit the image ("Ajustar", double-click).
    function fitToWindow() {
        zoomToFit();
        showHud("zoom");
    }

    function setZoom(scale) {
        fitMode = false;
        image.scale = Math.min(topScale, Math.max(minScale, scale));
        centerContent();
        showHud("zoom");
    }

    function zoomIn() { setZoom(integerZoom ? stepScale(image.scale, 1) : image.scale * 1.25); }
    function zoomOut() { setZoom(integerZoom ? stepScale(image.scale, -1) : image.scale / 1.25); }

    function toggleActualSize() {
        if (!fitMode && Math.abs(image.scale - 1.0) < 0.001)
            fitToWindow();
        else
            setZoom(1.0);
    }

    // --- Center readout (zoom % / image position in the folder) ------------
    // Appears instantly in the middle of the viewport and fades out a moment
    // later. `hudKind` picks what it says; the text itself is bound live, so a
    // burst of wheel-zoom ticks keeps the number current instead of stale.
    property string hudKind: "zoom"

    function showHud(kind) {
        hudFade.stop();
        hudKind = kind;
        hudBox.opacity = 1;
        hudFade.start();
    }

    function announceImageIndex() {
        // A one-image folder has no "position" worth announcing.
        if (folderModel.count > 1)
            showHud("index");
    }

    function rotateClockwise() {
        // Deliberately unbounded (not wrapped with % 360): the Behind on
        // rotation animates by linear interpolation from the current value
        // to the target, so wrapping 270 -> 0 made it spin backwards instead
        // of continuing forward those last 90 degrees. Letting it grow keeps
        // every rotation a consistent forward +90 turn; the numeric range is
        // effectively unbounded for any realistic amount of clicking.
        targetRotation += 90;
        image.rotation = targetRotation;
        centerContent();
    }

    // Mirror of rotateClockwise() - same unbounded-accumulator reasoning,
    // just turning the other way.
    function rotateCounterClockwise() {
        targetRotation -= 90;
        image.rotation = targetRotation;
        centerContent();
    }

    // --- Animated rotate / flip from the edit panel -------------------------
    // These edits are baked into the image on the C++ side, which swaps the
    // pixels instantly. To give the action a visible motion, the current view
    // is first animated to exactly what the baked result will look like (a
    // quarter turn, or a mirror), and only THEN is the edit applied - the
    // swap lands on an identical picture, so there is no visible jump.
    property bool transforming: false
    property string transformKind: ""
    // True only while our own bake is running, so the structuralImageChanged
    // handler can tell it apart from an unrelated image change (which must
    // cancel a half-played animation instead of baking onto the wrong image).
    property bool bakingTransform: false
    // Crop selection as fractions of the image, taken before the transform so
    // it can follow the image afterwards (null when there isn't one).
    property var transformSelection: null

    function playEditTransform(kind) {
        if (transforming || image.status !== Image.Ready)
            return;
        transforming = true;
        transformKind = kind;

        transformSelection = null;
        if (cropActive && cropHasSelection && cropBounds.width > 0 && cropBounds.height > 0) {
            transformSelection = {
                x: (selectionRect.x - cropBounds.x) / cropBounds.width,
                y: (selectionRect.y - cropBounds.y) / cropBounds.height,
                w: selectionRect.width / cropBounds.width,
                h: selectionRect.height / cropBounds.height
            };
        }

        suppressRotationAnimation = true;
        if (kind === "rotate" || kind === "rotateL") {
            // A quarter turn swaps the image's width and height, so the fit
            // scale changes with it; animate that too, or the view would
            // snap to the new scale the moment the edit is baked.
            const swappedFit = Math.min(flick.width / image.sourceSize.height,
                                        flick.height / image.sourceSize.width, 1.0);
            rotateAngleAnim.to = image.rotation + (kind === "rotateL" ? -90 : 90);
            rotateScaleAnim.to = swappedFit;
            rotateAnim.start();
        } else if (kind === "flipH") {
            flipHAnim.start();
        } else {
            flipVAnim.start();
        }
    }

    function finishEditTransform() {
        const kind = transformKind;
        bakingTransform = true;
        if (kind === "rotate")
            appController.rotateEdit90();
        else if (kind === "rotateL")
            appController.rotateEditMinus90();
        else if (kind === "flipH")
            appController.flipEditHorizontal();
        else
            appController.flipEditVertical();
        bakingTransform = false;

        flipTransform.xScale = 1;
        flipTransform.yScale = 1;
        suppressRotationAnimation = false;
        // The overlay stays hidden (transforming is still true) until the
        // crop frame has been re-measured against the new image.
        Qt.callLater(settleAfterTransform);
    }

    function settleAfterTransform() {
        if (cropActive) {
            const bounds = imageScreenRect();
            cropBounds = bounds;
            const sel = transformSelection;
            if (sel) {
                let nx = sel.x, ny = sel.y, nw = sel.w, nh = sel.h;
                if (transformKind === "rotate") {
                    nx = 1 - (sel.y + sel.h);
                    ny = sel.x;
                    nw = sel.h;
                    nh = sel.w;
                } else if (transformKind === "rotateL") {
                    nx = sel.y;
                    ny = 1 - (sel.x + sel.w);
                    nw = sel.h;
                    nh = sel.w;
                } else if (transformKind === "flipH") {
                    nx = 1 - (sel.x + sel.w);
                } else {
                    ny = 1 - (sel.y + sel.h);
                }
                selectionRect.x = bounds.x + nx * bounds.width;
                selectionRect.y = bounds.y + ny * bounds.height;
                selectionRect.width = nw * bounds.width;
                selectionRect.height = nh * bounds.height;
            }
            // A 4:3 frame is 3:4 once the image is turned on its side.
            if ((transformKind === "rotate" || transformKind === "rotateL") && !cropFreeform) {
                const w = cropAspectW;
                cropAspectW = cropAspectH;
                cropAspectH = w;
            }
        }
        transformSelection = null;
        transforming = false;
    }

    // An image change that isn't ours (next/previous, opening a file, undo)
    // arrived mid-animation: drop the animation rather than let it finish
    // and bake a rotate/flip onto a different picture.
    function abortEditTransform() {
        rotateAnim.stop();
        flipHAnim.stop();
        flipVAnim.stop();
        flipTransform.xScale = 1;
        flipTransform.yScale = 1;
        transformSelection = null;
        transforming = false;
    }

    // --- Straighten (fine-angle rotation) ---------------------------------
    // Live preview reuses the existing view-rotation transform (cheap, GPU-
    // composited) instead of re-baking the structural image on every slider
    // tick; only releasing the slider ("Aplicar" in EditPanel) commits a
    // real RotateOp via appController.straighten().
    property bool straightenActive: false

    function previewStraighten(angle) {
        suppressRotationAnimation = true;
        image.rotation = targetRotation + angle;
        suppressRotationAnimation = false;
    }

    function commitStraighten(angle) {
        suppressRotationAnimation = true;
        image.rotation = targetRotation; // the new baked image will reset this to 0 anyway
        suppressRotationAnimation = false;
        appController.straighten(angle, !(pixelMode && appSettings.pixelSharpEdit));
    }

    function cancelStraighten() {
        suppressRotationAnimation = true;
        image.rotation = targetRotation;
        suppressRotationAnimation = false;
    }

    // Image-space x/y (unscaled pixel coordinates) of the content-item
    // top-left corner, i.e. where the anchors.centerIn placement puts the
    // image within the (possibly viewport-sized) content area.
    function contentImageX(scale) {
        const w = Math.max(flick.width, image.width * scale);
        return (w - image.width * scale) / 2;
    }
    function contentImageY(scale) {
        const h = Math.max(flick.height, image.height * scale);
        return (h - image.height * scale) / 2;
    }

    function clamp(v, lo, hi) { return Math.min(hi, Math.max(lo, v)); }

    // --- Interactive crop -------------------------------------------------
    // While cropping, the view is pinned to fit-mode with rotation reset to
    // 0 (restored afterward) so the on-screen image rect is simple to
    // compute: it's exactly `cropBounds`, in the same (viewport) coordinate
    // space as this overlay.
    property bool cropActive: false
    // Freeform (no aspect constraint) is the default when entering crop mode;
    // picking a ratio preset switches this off.
    property bool cropFreeform: true
    property real cropAspectW: 1
    property real cropAspectH: 1
    property rect cropBounds: Qt.rect(0, 0, 0, 0)
    property int savedRotationForCrop: 0
    // No selection is pre-drawn on entering crop mode - the user draws it by
    // dragging over the image. Until they do, the selection rectangle/handles
    // stay hidden and only a hint is shown.
    property bool cropHasSelection: false
    // True for the duration of the press-drag that draws a new selection, so
    // the rectangle is visible live as it's being drawn instead of only
    // appearing once the mouse is released.
    property bool cropDrawing: false

    function imageScreenRect() {
        const s = image.scale;
        const w = image.width * s;
        const h = image.height * s;
        return Qt.rect((flick.width - w) / 2, (flick.height - h) / 2, w, h);
    }

    // Re-syncs cropBounds to the image's actual current on-screen rect, and
    // remaps any existing selection proportionally so it keeps covering the
    // same portion of the image. Needed whenever the viewport size changes
    // while cropping is active - e.g. opening the Edit panel for the first
    // time shrinks the canvas (to make room for the panel) right around the
    // same time crop mode starts. Always called via Qt.callLater (never
    // directly from a width/height change handler): anchors can cascade
    // through several intermediate geometries before settling, and calling
    // this mid-cascade against a not-yet-final flick.width/image.scale is
    // what caused the crop area to end up too large/offscreen; deferring to
    // the next idle moment guarantees everything has already settled.
    function refreshCropBounds() {
        if (!cropActive)
            return;
        if (fitMode)
            zoomToFit();
        const oldBounds = cropBounds;
        const newBounds = imageScreenRect();
        if (cropHasSelection && oldBounds.width > 0 && oldBounds.height > 0) {
            const nx = (selectionRect.x - oldBounds.x) / oldBounds.width;
            const ny = (selectionRect.y - oldBounds.y) / oldBounds.height;
            const nw = selectionRect.width / oldBounds.width;
            const nh = selectionRect.height / oldBounds.height;
            selectionRect.x = newBounds.x + nx * newBounds.width;
            selectionRect.y = newBounds.y + ny * newBounds.height;
            selectionRect.width = nw * newBounds.width;
            selectionRect.height = nh * newBounds.height;
        }
        cropBounds = newBounds;
    }

    // aspectW/aspectH <= 0 (or omitted) means freeform. Only sets up the
    // crop bounds/ratio - the selection itself is drawn by the user
    // dragging over the image (see the draw MouseArea below).
    function startCrop(aspectW, aspectH) {
        savedRotationForCrop = targetRotation;
        suppressRotationAnimation = true;
        targetRotation = 0;
        image.rotation = 0;
        suppressRotationAnimation = false;

        cropFreeform = !(aspectW > 0 && aspectH > 0);
        cropAspectW = cropFreeform ? 1 : aspectW;
        cropAspectH = cropFreeform ? 1 : aspectH;
        cropHasSelection = false;
        selectionRect.width = 0;
        selectionRect.height = 0;
        cropActive = true;

        // See refreshCropBounds(): deferred so the viewport (e.g. the Edit
        // panel about to take its share of the width) has fully settled
        // before cropBounds is measured.
        Qt.callLater(refreshCropBounds);
    }

    // Switches the aspect constraint. If a selection is already drawn,
    // reshapes it in place (centered on its current center); otherwise just
    // remembers the ratio for the next drag the user makes.
    function setCropAspect(aspectW, aspectH) {
        if (!cropActive) {
            startCrop(aspectW, aspectH);
            return;
        }
        cropFreeform = !(aspectW > 0 && aspectH > 0);
        cropAspectW = cropFreeform ? 1 : aspectW;
        cropAspectH = cropFreeform ? 1 : aspectH;
        if (cropFreeform || !cropHasSelection)
            return;

        const cx = selectionRect.x + selectionRect.width / 2;
        const cy = selectionRect.y + selectionRect.height / 2;
        let w = selectionRect.width;
        let h = w * aspectH / aspectW;
        if (h > cropBounds.height) { h = cropBounds.height; w = h * aspectW / aspectH; }
        if (w > cropBounds.width) { w = cropBounds.width; h = w * aspectH / aspectW; }

        selectionRect.width = w;
        selectionRect.height = h;
        selectionRect.x = clamp(cx - w / 2, cropBounds.x, cropBounds.x + cropBounds.width - w);
        selectionRect.y = clamp(cy - h / 2, cropBounds.y, cropBounds.y + cropBounds.height - h);
    }

    // Draws a brand-new selection anchored at the press point (sx, sy),
    // dragging its opposite corner to the current mouse position - the same
    // simultaneous width/height-vs-ratio resolution used by updateCropEdge's
    // corner case, just anchored at a fixed press point instead of a fixed
    // selection edge.
    function updateDraftCrop(sx, sy, mx, my) {
        mx = clamp(mx, cropBounds.x, cropBounds.x + cropBounds.width);
        my = clamp(my, cropBounds.y, cropBounds.y + cropBounds.height);

        let left, top, w, h;
        if (cropFreeform) {
            left = Math.min(sx, mx);
            top = Math.min(sy, my);
            w = Math.abs(mx - sx);
            h = Math.abs(my - sy);
        } else {
            const ratio = cropAspectW / cropAspectH;
            const maxW = mx < sx ? (sx - cropBounds.x) : (cropBounds.x + cropBounds.width - sx);
            const maxH = my < sy ? (sy - cropBounds.y) : (cropBounds.y + cropBounds.height - sy);
            const wFromX = Math.abs(mx - sx);
            const wFromY = Math.abs(my - sy) * ratio;
            w = Math.min(Math.max(wFromX, wFromY), maxW);
            h = w / ratio;
            if (h > maxH) { h = maxH; w = h * ratio; }
            left = mx < sx ? sx - w : sx;
            top = my < sy ? sy - h : sy;
        }

        selectionRect.x = left;
        selectionRect.y = top;
        selectionRect.width = w;
        selectionRect.height = h;
    }

    // `edge` is one of "n","s","e","w","nw","ne","sw","se" - which side(s)
    // of the selection follow the drag. (mx, my) are cropOverlay-relative
    // (i.e. same space as cropBounds/selectionRect).
    function updateCropEdge(edge, mx, my) {
        const minSize = 24;
        const hasW = edge.indexOf("w") >= 0;
        const hasE = edge.indexOf("e") >= 0;
        const hasN = edge.indexOf("n") >= 0;
        const hasS = edge.indexOf("s") >= 0;

        let left = selectionRect.x;
        let top = selectionRect.y;
        let right = selectionRect.x + selectionRect.width;
        let bottom = selectionRect.y + selectionRect.height;

        if (cropFreeform) {
            if (hasW) left = clamp(mx, cropBounds.x, right - minSize);
            if (hasE) right = clamp(mx, left + minSize, cropBounds.x + cropBounds.width);
            if (hasN) top = clamp(my, cropBounds.y, bottom - minSize);
            if (hasS) bottom = clamp(my, top + minSize, cropBounds.y + cropBounds.height);
        } else {
            // Width and height must be resolved TOGETHER against both the
            // ratio and the image bounds at once - clamping each edge
            // independently after the fact (the previous approach) could
            // shrink just one dimension near a boundary, silently breaking
            // the ratio, or leave the other dimension "stuck" reading as
            // still growing past the visible image while its paired
            // dimension had already maxed out.
            const ratio = cropAspectW / cropAspectH;
            const isCorner = hasW !== hasE && hasN !== hasS && (hasW || hasE) && (hasN || hasS);

            if (isCorner) {
                const anchorX = hasW ? right : left;
                const anchorY = hasN ? bottom : top;
                const maxW = hasW ? (anchorX - cropBounds.x) : (cropBounds.x + cropBounds.width - anchorX);
                const maxH = hasN ? (anchorY - cropBounds.y) : (cropBounds.y + cropBounds.height - anchorY);
                // Follow whichever axis the mouse moved further along (in
                // equivalent width terms) - previously this only looked at
                // the horizontal delta, so dragging mostly up/down did
                // nothing until the mouse also moved sideways.
                const wFromX = Math.abs(mx - anchorX);
                const wFromY = Math.abs(my - anchorY) * ratio;
                let w = clamp(Math.max(wFromX, wFromY), minSize, maxW);
                let h = w / ratio;
                if (h > maxH) { h = maxH; w = h * ratio; }
                if (hasW) left = anchorX - w; else right = anchorX + w;
                if (hasN) top = anchorY - h; else bottom = anchorY + h;
            } else if (hasN || hasS) {
                const anchorY = hasN ? bottom : top;
                const maxH = hasN ? (anchorY - cropBounds.y) : (cropBounds.y + cropBounds.height - anchorY);
                const cx = selectionRect.x + selectionRect.width / 2;
                const maxWFromCenter = 2 * Math.min(cx - cropBounds.x, cropBounds.x + cropBounds.width - cx);
                let h = clamp(Math.abs(my - anchorY), minSize, maxH);
                let w = h * ratio;
                if (w > maxWFromCenter) { w = maxWFromCenter; h = w / ratio; }
                if (hasN) top = anchorY - h; else bottom = anchorY + h;
                left = cx - w / 2;
                right = cx + w / 2;
            } else if (hasE || hasW) {
                const anchorX = hasW ? right : left;
                const maxW = hasW ? (anchorX - cropBounds.x) : (cropBounds.x + cropBounds.width - anchorX);
                const cy = selectionRect.y + selectionRect.height / 2;
                const maxHFromCenter = 2 * Math.min(cy - cropBounds.y, cropBounds.y + cropBounds.height - cy);
                let w = clamp(Math.abs(mx - anchorX), minSize, maxW);
                let h = w / ratio;
                if (h > maxHFromCenter) { h = maxHFromCenter; w = h * ratio; }
                if (hasW) left = anchorX - w; else right = anchorX + w;
                top = cy - h / 2;
                bottom = cy + h / 2;
            }
        }

        selectionRect.x = left;
        selectionRect.y = top;
        selectionRect.width = Math.max(minSize, right - left);
        selectionRect.height = Math.max(minSize, bottom - top);
    }

    function cancelCrop() {
        cropActive = false;
        cropHasSelection = false;
        // Instant, not animated: an unbounded number of prior "Rotar" clicks
        // can leave savedRotationForCrop at e.g. 720 or 1080 degrees (see
        // rotateClockwise's comment on why it's never wrapped), and without
        // suppression the Behavior would spin through that many full turns
        // to get back there.
        suppressRotationAnimation = true;
        image.rotation = savedRotationForCrop;
        targetRotation = savedRotationForCrop;
        suppressRotationAnimation = false;
    }

    function confirmCrop() {
        if (!cropHasSelection)
            return;
        const nx = (selectionRect.x - cropBounds.x) / cropBounds.width;
        const ny = (selectionRect.y - cropBounds.y) / cropBounds.height;
        const nw = selectionRect.width / cropBounds.width;
        const nh = selectionRect.height / cropBounds.height;
        appController.cropNormalized(nx, ny, nw, nh);
        cropActive = false;
        cropHasSelection = false;
        // Deliberately NOT restoring savedRotationForCrop here: cropping
        // bakes a new image and reloads it just like every other edit
        // (resize/adjust/filter/rotate-bake), which already resets rotation
        // to 0 itself (Image.onStatusChanged below, also suppressed).
        // Restoring the old, possibly large accumulated angle on top of that
        // - and un-suppressed, since this call site never set the guard -
        // was fighting that reset and spinning through however many
        // multiples of 360 had piled up before crop mode was entered.
    }

    // Zooms in/out while keeping the image point under the cursor fixed on
    // screen, the way every other image/map viewer handles mouse-wheel zoom.
    //
    // (cx, cy) are CONTENT coordinates, not viewport ones: any item declared
    // as a direct child of a Flickable - our wheel-handling MouseArea
    // included - gets auto-parented to the Flickable's contentItem, so the
    // positions it reports are already offset by contentX/contentY. Treating
    // them as viewport-relative (i.e. adding contentX again) double-counted
    // that offset, an error that grows with contentX - which is exactly why
    // it only got worse the more you kept zooming in.
    function zoomAt(factor, cx, cy) {
        const oldScale = image.scale;
        if (appSettings.smoothZoom && !integerZoom && factor !== 1) {
            // smooth zoom: the wheel only moves the target; smoothTick() glides towards it, keeping the point
            // under the cursor (kept as a viewport position, because the content moves while it glides)
            const base = smoothActive ? smoothTarget : oldScale;
            smoothTarget = Math.min(topScale, Math.max(minScale, base * factor));
            smoothVx = cx - flick.contentX;
            smoothVy = cy - flick.contentY;
            smoothActive = true;
            return;
        }
        smoothActive = false;
        const wanted = integerZoom ? (factor > 1 ? stepScale(oldScale, 1) : factor < 1 ? stepScale(oldScale, -1) : oldScale)
                                 : oldScale * factor;
        applyZoomAt(Math.min(topScale, Math.max(minScale, wanted)), cx, cy);
    }

    property bool smoothActive: false
    property real smoothTarget: 1
    property real smoothVx: 0
    property real smoothVy: 0
    // Any other change of the view (fit, 100%, a new picture) ends the glide.
    onFitModeChanged: if (fitMode) smoothActive = false
    FrameAnimation {
        running: root.smoothActive
        onTriggered: {
            const cur = image.scale, target = root.smoothTarget;
            // closes ~a third of the remaining distance (in the ratio of the sizes) every 16 ms: quick to start
            // and settling softly
            const k = 1 - Math.exp(-frameTime / 0.06);
            let next = cur * Math.pow(target / cur, k);
            if (Math.abs(next - target) / target < 0.003) {
                next = target;
                root.smoothActive = false;
            }
            root.applyZoomAt(next, flick.contentX + root.smoothVx, flick.contentY + root.smoothVy);
        }
    }

    function applyZoomAt(newScale, cx, cy) {
        const oldScale = image.scale;
        if (Math.abs(newScale - oldScale) < 0.00001)
            return;

        const oldImgX = contentImageX(oldScale);
        const oldImgY = contentImageY(oldScale);
        const ux = (cx - oldImgX) / oldScale;
        const uy = (cy - oldImgY) / oldScale;

        // Where the cursor sits relative to the visible viewport - this is
        // what must stay constant after the rescale.
        const viewportX = cx - flick.contentX;
        const viewportY = cy - flick.contentY;

        fitMode = false;
        image.scale = newScale;
        showHud("zoom");

        const newImgX = contentImageX(newScale);
        const newImgY = contentImageY(newScale);
        const newCx = ux * newScale + newImgX;
        const newCy = uy * newScale + newImgY;

        const newContentWidth = Math.max(flick.width, image.width * newScale);
        const newContentHeight = Math.max(flick.height, image.height * newScale);
        flick.contentX = clamp(newCx - viewportX, 0, newContentWidth - flick.width);
        flick.contentY = clamp(newCy - viewportY, 0, newContentHeight - flick.height);
    }

    // The viewport was resized (window resize, panel toggled, etc.) - keep
    // following the viewport when in fit mode instead of leaving stale zoom.
    onWidthChanged: { if (fitMode) zoomToFit(); if (cropActive) Qt.callLater(refreshCropBounds); }
    onHeightChanged: { if (fitMode) zoomToFit(); if (cropActive) Qt.callLater(refreshCropBounds); }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: Math.max(width, image.width * image.scale)
        contentHeight: Math.max(height, image.height * image.scale)
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        // While the picture is fitted it must stay centred whatever the content size
        // does. Right after an edit swaps in a picture of another shape (a quarter
        // turn), the content is measured once with the old size and once with the
        // new one; centring only at the first moment left the view offset until some
        // later event re-centred it - the little jump the user saw.
        onContentWidthChanged: if (root.fitMode) root.centerContent()
        onContentHeightChanged: if (root.fitMode) root.centerContent()

        Image {
            id: image
            source: appController.currentSource
            anchors.centerIn: parent
            fillMode: Image.PreserveAspectFit
            // Until a picture is loaded it fills the viewport. (Not `parent`:
            // that is the Flickable's content item, whose size is contentWidth/
            // Height - and those depend on this width/height: a binding loop.)
            width: sourceSize.width > 0 ? sourceSize.width : flick.width
            height: sourceSize.height > 0 ? sourceSize.height : flick.height
            smooth: !root.nearest
            mipmap: true
            // The Ajustes sliders are applied live by the shader chain below
            // instead of baking a new image on the CPU on every slider tick - this Image just holds the plain
            // (structural-only) pixels as its GPU texture source, and is
            // never shown directly.
            visible: false
            // The provider already serves an in-memory QImage (no disk I/O,
            // no real decode work left to do), so async loading only added a
            // thread hand-off delay - during which Image shows nothing,
            // producing a visible blink to the background color on every
            // edit (crop/resize/adjust/filter all swap `source`).
            asynchronous: false
            cache: false // ImageLoader/ThumbnailCache own caching, not QML's Image

            // No Behavior on scale: zoomAt()'s cursor-anchored pan math sets
            // flick.contentX/contentY for the TARGET scale immediately: an
            // animated scale would render at an in-between value for ~120ms
            // while contentX/Y already assumed the final one, producing the
            // jump/flicker reported when zooming (wheel or buttons).
            Behavior on rotation {
                enabled: !root.suppressRotationAnimation
                NumberAnimation { duration: themeManager.animFast; easing.type: themeManager.easingCurve }
            }

            // Reset rotation and re-fit whenever the image's actual identity
            // changes (a new file opened, or a structural edit baked) - NOT
            // on every animated GIF/APNG frame tick, which also swaps
            // `source` (to force the reload) but keeps the same dimensions
            // and shouldn't yank the user's zoom/pan/rotation back on every
            // frame. See AppController::structuralImageChanged's doc comment.
            Connections {
                target: appController
                function onStructuralImageChanged() {
                    if (root.transforming && !root.bakingTransform)
                        root.abortEditTransform();
                    root.suppressRotationAnimation = true;
                    root.targetRotation = 0;
                    image.rotation = 0;
                    root.suppressRotationAnimation = false;
                    root.zoomToFit();
                }
            }
        }

        // The as-opened, unedited image - only ever shown when compareOriginal
        // is true (EditPanel's "Comparar" button held down), bypassing the
        // whole filter/adjust/detail chain below entirely.
        Image {
            id: originalImage
            // Loaded only while "Comparar" is held: kept resident it would cost a
            // second full-size texture (about 200 MB for a 37 MP photo) for nothing.
            source: root.compareOriginal ? appController.originalSource : ""
            visible: false
            x: image.x
            y: image.y
            width: image.width
            height: image.height
            rotation: image.rotation
            scale: image.scale
            transformOrigin: image.transformOrigin
            smooth: !root.nearest
            mipmap: true
            asynchronous: false
            cache: false
        }

        // Live GPU preview chain - `image` above holds only the structural
        // (crop/resize/rotate/flip) pixels, never shown directly. Everything
        // after it is a texture-processing stage feeding the next: filter
        // look (Grade.frag), then the Ajustes tone/color stages (Grade.frag
        // again), then clarity + sharpen + vignette + grain (Detail.frag).
        // Only the LAST stage (detailEffect) actually carries the on-screen
        // transform (position/rotation/scale) - the intermediate stages are
        // captured as plain, untransformed textures at `image`'s native size,
        // so mirroring geometry on each of them would be redundant.
        //
        // NOTE on hidden intermediate stages below: a custom ShaderEffect
        // (unlike a plain Image, and unlike a final visible item) simply
        // does not render at all - and so never produces a valid texture -
        // while its own `visible` is false; Qt Quick skips its paint pass
        // entirely rather than keeping its output around for something else
        // to sample. `visible: false` alone (which is all a plain Image ever
        // needs, since it is a texture holder that doesn't need an active
        // paint pass) silently breaks any INTERMEDIATE ShaderEffect stage.
        // ShaderEffectSource's `hideSource: true` is the mechanism actually
        // meant for this: it keeps sourceItem rendering into a texture every
        // frame while suppressing its normal on-screen appearance.
        // The Filtros look. Same shader as the Ajustes stage below, fed with the
        // look's own numbers and tone table: `amount` is the Cantidad slider, so
        // 0 passes the photo straight through.
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
            // A stage that changes nothing is switched off, which frees its GPU
            // texture (see lookSource / adjustSource and detailEffect.source).
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
            smooth: !root.nearest
            // Only the last stage before Detail.frag is shown scaled down, and it needs mips.
            mipmap: !appController.gradeActive
        }

        // The 256x1 tone table stage A of the adjustment reads (white balance,
        // exposure, levels, gamma, curves, brightness, contrast, ... already
        // folded together on the C++ side). Unfiltered on purpose: the shader
        // must read exact entries, never a blend of two neighbors.
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
            // The look-only extras must be declared (and zero) for this pass
            // too - the shader reads them from the same uniform block. The
            // vignette and grain SLIDERS are applied later, in Detail.frag.
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
            smooth: !root.nearest
            // Clarity compares each pixel with a heavily blurred copy of the
            // image, which Detail.frag reads from a high mip level.
            mipmap: true
        }

        // Modo pixel: a checkerboard where the picture is transparent (it follows the
        // picture's position, turn and zoom; the squares stay 12 screen pixels).
        ShaderEffect {
            id: checkerEffect
            visible: root.pixelMode && appSettings.pixelCheckerboard
            x: image.x
            y: image.y
            width: image.width
            height: image.height
            rotation: image.rotation
            scale: image.scale
            transformOrigin: image.transformOrigin
            property vector2d cells: Qt.vector2d(width * scale / 12, height * scale / 12)
            property color colorA: Qt.tint(themeManager.background, Qt.rgba(themeManager.textPrimary.r, themeManager.textPrimary.g, themeManager.textPrimary.b, 0.10))
            property color colorB: Qt.tint(themeManager.background, Qt.rgba(themeManager.textPrimary.r, themeManager.textPrimary.g, themeManager.textPrimary.b, 0.20))
            fragmentShader: "qrc:/shaders/Checker.frag.qsb"
        }

        ShaderEffect {
            id: detailEffect
            x: image.x
            y: image.y
            width: image.width
            height: image.height
            rotation: image.rotation
            scale: image.scale
            transformOrigin: image.transformOrigin
            // The newest stage that is actually running (the plain image when none is).
            property variant source: root.compareOriginal ? originalImage
                : appController.gradeActive ? adjustSource
                : appController.lookActive ? lookSource : image
            property real amount: root.compareOriginal ? 0.0 : appController.adjust.sharpness
            property real clarity: root.compareOriginal ? 0.0 : appController.adjust.clarity
            property real vignette: root.compareOriginal ? 0.0 : appController.adjust.vignette
            property real grain: root.compareOriginal ? 0.0 : appController.adjust.grain
            property real grainType: appController.adjust.grainType
            property real grainMono: appController.adjust.grainMono ? 1.0 : 0.0
            property size resolution: Qt.size(image.width, image.height)
            property size texelSize: Qt.size(1.0 / Math.max(1, image.width), 1.0 / Math.max(1, image.height))
            fragmentShader: "qrc:/shaders/Detail.frag.qsb"

            // Mirror used by the animated flip: slides xScale/yScale from 1
            // to -1 about the picture's center (applied after rotation and
            // scale, so it always mirrors along the screen's own axes).
            // Sits at 1 the rest of the time.
            transform: Scale {
                id: flipTransform
                origin.x: detailEffect.width / 2
                origin.y: detailEffect.height / 2
            }
        }

        // Modo pixel, from 8x: a thin grid between the picture's own pixels.
        ShaderEffect {
            id: gridEffect
            visible: root.pixelMode && appSettings.pixelGrid && image.scale >= 8 && !root.compareOriginal
            x: image.x
            y: image.y
            width: image.width
            height: image.height
            rotation: image.rotation
            scale: image.scale
            transformOrigin: image.transformOrigin
            property vector2d pixels: Qt.vector2d(image.sourceSize.width, image.sourceSize.height)
            property real lineWidth: 1.0 / Math.max(1, image.scale)
            property color lineColor: "#66808080"
            fragmentShader: "qrc:/shaders/PixelGrid.frag.qsb"
        }

        ParallelAnimation {
            id: rotateAnim
            NumberAnimation {
                id: rotateAngleAnim
                target: image
                property: "rotation"
                duration: themeManager.animMedium
                easing.type: themeManager.easingCurve
            }
            NumberAnimation {
                id: rotateScaleAnim
                target: image
                property: "scale"
                duration: themeManager.animMedium
                easing.type: themeManager.easingCurve
            }
            onFinished: root.finishEditTransform()
        }
        NumberAnimation {
            id: flipHAnim
            target: flipTransform
            property: "xScale"
            to: -1
            duration: themeManager.animMedium
            easing.type: themeManager.easingCurve
            onFinished: root.finishEditTransform()
        }
        NumberAnimation {
            id: flipVAnim
            target: flipTransform
            property: "yScale"
            to: -1
            duration: themeManager.animMedium
            easing.type: themeManager.easingCurve
            onFinished: root.finishEditTransform()
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            enabled: !root.pickerOn
            onDoubleTapped: root.toggleActualSize()
        }
        // with Alt held a click picks the colour under the cursor
        TapHandler {
            acceptedButtons: Qt.LeftButton
            enabled: root.pickerOn
            onTapped: root.pickColor()
        }

        // A plain MouseArea (not a WheelHandler on a parent item) is what
        // actually wins the wheel event ahead of Flickable's own built-in
        // wheel-to-scroll handling, since it's a direct child sitting on top
        // of the content and Qt Quick delivers wheel events to the topmost
        // child first. Without this, the mouse wheel panned the Flickable
        // instead of zooming.
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            onWheel: function (wheel) {
                const factor = wheel.angleDelta.y > 0 ? 1.1 : 1 / 1.1;
                root.zoomAt(factor, wheel.x, wheel.y);
                wheel.accepted = true;
            }
        }

        // Right-click context menu - only claims the right button, so it
        // doesn't interfere with the left-click/wheel handling above or
        // (while cropping) the crop overlay's own MouseAreas.
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            enabled: !root.cropActive
            onClicked: function (mouse) {
                if (mouse.button === Qt.RightButton)
                    imageContextMenu.popup();
            }
        }
    }

    AppMenu {
        id: imageContextMenu
        AppMenuItem {
            iconName: "copy"
            text: qsTr("Copiar imagen")
            enabled: appController.currentSource.length > 0
            onTriggered: appController.copyToClipboard()
        }
        AppMenuItem {
            iconName: "paste"
            text: qsTr("Pegar imagen")
            enabled: !root.editMode
            onTriggered: appController.pasteFromClipboard()
        }
        AppMenuItem {
            iconName: "wallpaper"
            text: qsTr("Poner como fondo de escritorio")
            enabled: appController.currentSource.length > 0
            onTriggered: appController.setAsWallpaper()
        }
        AppMenuSeparator {}
        AppMenuItem {
            iconName: "edit"
            text: qsTr("Editar")
            checkable: true
            enabled: !appController.isAnimated && !root.locked
            checked: root.editMode
            onTriggered: root.editModeRequested(!root.editMode)
        }
        AppMenuSeparator {}
        AppMenuItem {
            iconName: "trash"
            danger: true
            text: qsTr("Eliminar archivo…")
            enabled: appController.currentSource.length > 0 && !root.locked
            onTriggered: if (root.toolbar) root.toolbar.requestDelete()
        }
    }

    // The colour readout (Modo pixel > "mostrar posición y color") and the eyedropper: holding Alt shows the
    // readout whatever the setting says, turns the cursor into an eyedropper, and a click copies the colour
    // as #RRGGBB.
    readonly property bool readoutOn: pixelMode && appSettings.pixelReadout
    property bool altDown: false
    readonly property bool pickerOn: altDown && !cropActive && !editMode
    readonly property bool readoutVisible: readoutOn || pickerOn
    property string copiedHex: ""
    HoverHandler { id: pixelHover }
    // Alt is polled (pressing it alone sends no mouse event), only while the mouse is over the picture
    Timer {
        interval: 50
        repeat: true
        running: pixelHover.hovered
        onTriggered: root.altDown = colorPicker.altDown()
    }
    Connections {
        target: pixelHover
        function onHoveredChanged() { if (!pixelHover.hovered) root.altDown = false; }
    }
    onPickerOnChanged: colorPicker.setCursor(pickerOn && pixelHover.hovered)
    Connections {
        target: pixelHover
        function onHoveredChanged() { colorPicker.setCursor(root.pickerOn && pixelHover.hovered); }
    }
    Timer { id: copiedTimer; interval: 1600; onTriggered: root.copiedHex = "" }
    function pickColor() {
        const info = appController.pixelAt(pixelCell.x, pixelCell.y);
        if (info.valid !== true)
            return;
        const text = colorPicker.hex(info.r, info.g, info.b, info.a);
        colorPicker.copyText(text);
        copiedHex = text;
        copiedTimer.restart();
    }
    readonly property point pixelCell: {
        void (image.scale + flick.contentX + flick.contentY + image.rotation); // follow zoom / pan too
        const p = image.mapFromItem(root, pixelHover.point.position.x, pixelHover.point.position.y);
        return Qt.point(Math.floor(p.x), Math.floor(p.y));
    }
    readonly property var pixelInfo: root.readoutVisible && pixelHover.hovered
        ? appController.pixelAt(pixelCell.x, pixelCell.y) : ({ valid: false })

    Rectangle {
        visible: root.readoutVisible && root.pixelInfo.valid === true
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 12
        z: 25
        height: 30
        width: readout.implicitWidth + 50
        radius: themeManager.radiusMedium === 0 ? 0 : 8
        color: themeManager.surfaceElevated
        border.color: themeManager.border
        border.width: 1
        opacity: 0.94

        Rectangle {
            id: swatch
            x: 8
            anchors.verticalCenter: parent.verticalCenter
            width: 16
            height: 16
            radius: 3
            border.color: themeManager.border
            color: root.pixelInfo.valid === true
                   ? Qt.rgba(root.pixelInfo.r / 255, root.pixelInfo.g / 255, root.pixelInfo.b / 255, root.pixelInfo.a / 255)
                   : "transparent"
        }
        Label {
            id: readout
            anchors.left: swatch.right
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            color: themeManager.textPrimary
            font.pixelSize: 12
            text: root.pixelInfo.valid === true
                  ? (root.copiedHex !== ""
                     ? qsTr("X %1  Y %2   %3   ·   Copiado").arg(root.pixelCell.x).arg(root.pixelCell.y).arg(root.copiedHex)
                     : qsTr("X %1  Y %2   %3").arg(root.pixelCell.x).arg(root.pixelCell.y)
                           .arg(colorPicker.hex(root.pixelInfo.r, root.pixelInfo.g, root.pixelInfo.b, root.pixelInfo.a)))
                  : ""
        }
    }

    // A heavy picture takes a while to open: a pill that fills while it does.
    AppLoadingPill {
        anchors.centerIn: parent
        loading: appController.isLoading
        z: 30
    }

    // Error banner - anchored near the top edge instead of dead-center over
    // the image, and with a fixed (not theme-dependent) semantic error
    // color, so it can't end up low-contrast/easy to miss under any theme.
    property bool errorToastVisible: false

    Connections {
        target: appController
        function onErrorStringChanged() {
            root.errorToastVisible = appController.errorString.length > 0;
            errorToastHideTimer.restart();
        }
    }
    Timer { id: errorToastHideTimer; interval: 4000; onTriggered: root.errorToastVisible = false }

    Rectangle {
        id: errorToast
        anchors.top: parent.top
        anchors.topMargin: 12
        anchors.horizontalCenter: parent.horizontalCenter
        z: 20
        radius: themeManager.radiusMedium
        color: "#d5484d"
        opacity: root.errorToastVisible ? 1 : 0
        visible: opacity > 0
        width: errorToastLabel.implicitWidth + 28
        height: errorToastLabel.implicitHeight + 16

        Behavior on opacity {
            NumberAnimation { duration: themeManager.animFast }
        }

        Label {
            id: errorToastLabel
            anchors.centerIn: parent
            text: appController.errorString
            color: "#ffffff"
            wrapMode: Text.WordWrap
            width: Math.min(parent.parent.width - 80, 480)
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // A save that worked but could not keep something (the original's metadata): a
    // neutral note in the same slot, below the error toast when both are up.
    //
    // The same slot says "Guardando…" while a file is being written (the write runs in
    // the background) and "Guardado" for a moment once it has worked.
    property string noticeText: ""
    // Did the save that is running (or just ended) already say something?
    property bool saveWarned: false
    function showNotice(message, milliseconds) {
        root.noticeText = message;
        noticeHideTimer.interval = milliseconds;
        noticeHideTimer.restart();
    }
    Connections {
        target: appController
        function onIsSavingChanged() {
            if (appController.isSaving) { // a new save: whatever the last one said is old news
                root.saveWarned = false;
                root.noticeText = "";
            }
        }
        function onSaveNotice(message) {
            root.saveWarned = true;
            root.showNotice(message, 7000);
        }
        function onSaveFinished(ok, path) {
            if (ok && !root.saveWarned)
                root.showNotice(qsTr("Guardado"), 2500);
        }
    }
    Timer { id: noticeHideTimer; interval: 7000; onTriggered: root.noticeText = "" }
    AppHintBox {
        id: noticeBox
        anchors.top: parent.top
        anchors.topMargin: 12 + (errorToast.visible ? errorToast.height + 8 : 0)
        anchors.horizontalCenter: parent.horizontalCenter
        z: 20
        visible: appController.isSaving || root.noticeText.length > 0
        maxTextWidth: Math.min(root.width - 80, 480)
        text: appController.isSaving ? qsTr("Guardando…") : root.noticeText
    }

    // Crop instructional hint, in the same top-banner slot as the error
    // toast above (translucent instead of solid, so it doesn't read as an
    // error) - it used to be centered directly over the image, which made
    // it hard to read against busy/light images and, worse, let the image
    // paint over the middle of the text on some content. Anchoring here
    // sidesteps both: it's never over the image at all.
    AppHintBox {
        id: cropHint
        anchors.top: parent.top
        anchors.topMargin: 12
        anchors.horizontalCenter: parent.horizontalCenter
        z: 20
        visible: root.cropActive && !root.cropHasSelection && !root.cropDrawing && !root.transforming
        maxTextWidth: Math.min(root.width - 80, 420)
        text: qsTr("Arrastrá sobre la imagen para seleccionar el área a recortar")
    }

    // Zoom % / "3 / 12" readout: same box as the crop hint above and in the
    // same top-center slot (out of the way of the picture itself), shown by
    // showHud() and faded out by hudFade. It appears at full opacity at once
    // - only the exit animates. If the crop hint or an error toast is up in
    // that slot, it sits just below instead of on top of it.
    AppHintBox {
        id: hudBox
        anchors.top: parent.top
        anchors.topMargin: 12 + (cropHint.visible || errorToast.visible || noticeBox.visible
                                 ? Math.max(cropHint.visible ? cropHint.height : 0,
                                            errorToast.visible ? errorToast.height : 0,
                                            noticeBox.visible ? noticeBox.height + (errorToast.visible ? errorToast.height + 8 : 0) : 0) + 8
                                 : 0)
        anchors.horizontalCenter: parent.horizontalCenter
        z: 20
        opacity: 0
        visible: opacity > 0
        maxTextWidth: 240
        text: root.hudKind === "index"
              ? qsTr("%1 / %2").arg(folderModel.currentIndex + 1).arg(folderModel.count)
              : qsTr("%1%").arg(root.zoomPercent)
    }

    SequentialAnimation {
        id: hudFade
        PauseAnimation { duration: 700 }
        NumberAnimation {
            target: hudBox
            property: "opacity"
            to: 0
            duration: themeManager.animMedium
            easing.type: Easing.OutQuad
        }
    }

    Connections {
        target: folderModel
        // Deferred so folderModel.count has settled: opening a file rescans
        // its folder and sets the index in one go.
        function onCurrentIndexChanged() { Qt.callLater(root.announceImageIndex); }
    }

    Label {
        anchors.centerIn: parent
        visible: !appController.isLoading && appController.errorString.length === 0
                 && image.status !== Image.Ready
        text: qsTr("Arrastrá una imagen o usá Abrir")
        color: themeManager.textSecondary
    }

    Item {
        id: cropOverlay
        anchors.fill: parent
        visible: root.cropActive && !root.transforming
        z: 10

        // Eats every click/wheel in the dimmed area so it can't reach the
        // Flickable underneath (panning/zooming while cropping would be
        // confusing) - declared before the dim rectangles/selection so both
        // still render on top of it. Also doubles as the "draw a new
        // selection" surface: pressing anywhere here (including over the
        // dimmed area of an existing selection, to redraw it) and dragging
        // defines a fresh rect anchored at the press point; the selection's
        // own MouseArea and the resize handles are declared later/on top, so
        // a press that actually lands on them is consumed there first and
        // never reaches this one.
        MouseArea {
            id: drawArea
            anchors.fill: parent
            property real startX: 0
            property real startY: 0
            onWheel: function (wheel) { wheel.accepted = true; }
            onPressed: function (mouse) {
                startX = root.clamp(mouse.x, root.cropBounds.x, root.cropBounds.x + root.cropBounds.width);
                startY = root.clamp(mouse.y, root.cropBounds.y, root.cropBounds.y + root.cropBounds.height);
                root.cropHasSelection = false;
                root.cropDrawing = true;
                root.updateDraftCrop(startX, startY, startX, startY);
            }
            onPositionChanged: function (mouse) {
                if (pressed)
                    root.updateDraftCrop(startX, startY, mouse.x, mouse.y);
            }
            onReleased: function () {
                root.cropDrawing = false;
                if (selectionRect.width >= 24 && selectionRect.height >= 24)
                    root.cropHasSelection = true;
            }
        }

        Rectangle { visible: root.cropHasSelection || root.cropDrawing; color: "#99000000"; x: 0; y: 0; width: parent.width; height: selectionRect.y }
        Rectangle { visible: root.cropHasSelection || root.cropDrawing; color: "#99000000"; x: 0; y: selectionRect.y + selectionRect.height; width: parent.width; height: parent.height - selectionRect.y - selectionRect.height }
        Rectangle { visible: root.cropHasSelection || root.cropDrawing; color: "#99000000"; x: 0; y: selectionRect.y; width: selectionRect.x; height: selectionRect.height }
        Rectangle { visible: root.cropHasSelection || root.cropDrawing; color: "#99000000"; x: selectionRect.x + selectionRect.width; y: selectionRect.y; width: parent.width - selectionRect.x - selectionRect.width; height: selectionRect.height }

        Rectangle {
            id: selectionRect
            visible: root.cropHasSelection || root.cropDrawing
            color: "#22ffffff"
            border.color: "white"
            border.width: 2

            MouseArea {
                anchors.fill: parent
                drag.target: selectionRect
                drag.axis: Drag.XAndYAxis
                drag.minimumX: root.cropBounds.x
                drag.maximumX: root.cropBounds.x + root.cropBounds.width - selectionRect.width
                drag.minimumY: root.cropBounds.y
                drag.maximumY: root.cropBounds.y + root.cropBounds.height - selectionRect.height
            }

            // 4 corner handles + 4 edge handles - the `edge` string tells
            // updateCropEdge() which side(s) of the selection to move.
            Repeater {
                model: ["nw", "n", "ne", "e", "se", "s", "sw", "w"]
                delegate: Rectangle {
                    id: handle
                    required property string modelData
                    readonly property string edge: modelData
                    readonly property bool isCorner: edge.length === 2
                    readonly property real smallSide: Math.min(selectionRect.width, selectionRect.height)
                    // The corner dots are always there, so a tiny selection
                    // can still be grabbed and grown; they just shrink a bit
                    // (16 -> 10px) so four of them don't bury a 24px box.
                    // The edge bars only appear once their side is long
                    // enough to hold the bar (32px) plus a gap on both ends
                    // to the corner dots - on a small crop they'd sit on top
                    // of the corners. Fading is `opacity` + a disabled hit
                    // area (not `visible: false`, which would also kill the
                    // MouseArea while it fades back in mid-drag).
                    // (Only while a brand-new box is still being dragged out
                    // below the 24px minimum are the dots held back - that
                    // draft isn't a selection yet.)
                    readonly property bool shown: isCorner
                        ? !(root.cropDrawing && smallSide < 24)
                        : (edge === "n" || edge === "s" ? selectionRect.width : selectionRect.height) >= 56
                    opacity: shown ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: 120 } }
                    readonly property real dot: Math.max(10, Math.min(16, smallSide / 2.5))
                    width: isCorner ? dot : (edge === "n" || edge === "s" ? 32 : 10)
                    height: isCorner ? dot : (edge === "e" || edge === "w" ? 32 : 10)
                    radius: isCorner ? dot / 2 : 3
                    color: "white"
                    border.color: "#55000000"
                    x: edge.indexOf("w") >= 0 ? -width / 2
                       : edge.indexOf("e") >= 0 ? parent.width - width / 2
                       : parent.width / 2 - width / 2
                    y: edge.indexOf("n") >= 0 ? -height / 2
                       : edge.indexOf("s") >= 0 ? parent.height - height / 2
                       : parent.height / 2 - height / 2

                    MouseArea {
                        anchors.fill: parent
                        // Bigger touch/click target than the visible handle -
                        // but on a tiny selection the four corner targets
                        // would swallow the whole box and it could no longer
                        // be dragged, so the margin shrinks to nothing at the
                        // 24px minimum (always leaving a strip in the middle).
                        anchors.margins: -Math.max(0, Math.min(8, (handle.smallSide - 24) / 2))
                        enabled: handle.shown
                        onPositionChanged: function (mouse) {
                            if (pressed) {
                                const p = mapToItem(cropOverlay, mouse.x, mouse.y);
                                root.updateCropEdge(handle.edge, p.x, p.y);
                            }
                        }
                    }
                }
            }
        }
    }

    // The handles an effect asks for (the centre of a vignette, where to stretch...).
    EffectHandles {
        anchors.fill: parent
        z: 9
        imageItem: image
        flickItem: flick
        visible: root.editMode && appController.effectId !== "" && appController.effectOverlays.length > 0
                 && !root.cropActive && !root.transforming
    }

    // Rule-of-thirds reference grid, shown only while adjusting "Enderezar"
    // - purely visual, no interaction, so it's a flat overlay (not part of
    // the Flickable's content) that just sits over the whole viewport.
    Item {
        anchors.fill: parent
        visible: root.straightenActive && root.editMode
        z: 9
        Repeater {
            model: 2
            delegate: Rectangle {
                required property int index
                x: parent.width * (index + 1) / 3
                y: 0
                width: 1
                height: parent.height
                color: "#aaffffff"
            }
        }
        Repeater {
            model: 2
            delegate: Rectangle {
                required property int index
                x: 0
                y: parent.height * (index + 1) / 3
                width: parent.width
                height: 1
                color: "#aaffffff"
            }
        }
    }
}
