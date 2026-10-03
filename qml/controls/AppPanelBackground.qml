import QtQuick
import ImageViewerApp

// Shared flat-panel-surface look, reused as the root background for
// EditPanel/ThumbnailStrip/Toolbar and for AppMenu/infoPopup-style elevated
// surfaces. Keeping this in one place is what makes a future theme's panel
// look a one-file change instead of touching every panel individually.
Rectangle {
    id: root
    color: themeManager.surface
    border.color: themeManager.border
}
