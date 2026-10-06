#pragma once

#include <QObject>
#include <QSettings>

// User-configurable defaults for the slideshow and batch operations,
// persisted to a plain .ini file (QSettings::IniFormat, under the app's
// standard config location) rather than the Windows registry - deliberately
// avoiding the registry entirely for this, unlike the Explorer context-menu
// integration (which the user runs themselves via a .reg file).
class AppSettings : public QObject {
    Q_OBJECT
    Q_PROPERTY(int slideshowIntervalMs READ slideshowIntervalMs WRITE setSlideshowIntervalMs NOTIFY slideshowIntervalMsChanged)
    Q_PROPERTY(bool slideshowRandom READ slideshowRandom WRITE setSlideshowRandom NOTIFY slideshowRandomChanged)
    Q_PROPERTY(bool slideshowLoop READ slideshowLoop WRITE setSlideshowLoop NOTIFY slideshowLoopChanged)
    // 1-100, applied only where it's meaningful (JPEG/WebP) - see
    // core::saveImage()'s quality parameter.
    Q_PROPERTY(int batchExportQuality READ batchExportQuality WRITE setBatchExportQuality NOTIFY batchExportQualityChanged)
    // Ask before "Guardar" replaces the original file.
    Q_PROPERTY(bool confirmOverwrite READ confirmOverwrite WRITE setConfirmOverwrite NOTIFY confirmOverwriteChanged)
    // Digit count for the numbered suffix batch rename produces (e.g. 3 ->
    // "imagen_001").
    Q_PROPERTY(int batchRenamePadding READ batchRenamePadding WRITE setBatchRenamePadding NOTIFY batchRenamePaddingChanged)
    // The thumbnail bar on the left. Size of one thumbnail in pixels (also changed by dragging the
    // bar's edge), how many columns it has, and when it is shown: "manual" (the tab opens and
    // closes it), "open" (always), "closed" (never) or "auto" (closes while editing, during the
    // slideshow and in narrow windows).
    Q_PROPERTY(int stripThumbSize READ stripThumbSize WRITE setStripThumbSize NOTIFY stripThumbSizeChanged)
    Q_PROPERTY(int stripColumns READ stripColumns WRITE setStripColumns NOTIFY stripColumnsChanged)
    Q_PROPERTY(QString stripMode READ stripMode WRITE setStripMode NOTIFY stripModeChanged)
    // "Modo pixel" (Configuración > Apariencia). `pixelMode` is the switch itself: pictures stay
    // sharp (no smoothing) once zoomed in, up to 64x. The rest are extras of that mode, each one
    // on its own switch, and only count while the mode is on.
    // Wheel zoom: in fixed steps (false) or glided smoothly towards the new size (true).
    Q_PROPERTY(bool smoothZoom READ smoothZoom WRITE setSmoothZoom NOTIFY smoothZoomChanged)
    Q_PROPERTY(bool pixelMode READ pixelMode WRITE setPixelMode NOTIFY pixelModeChanged)
    Q_PROPERTY(bool pixelIntegerZoom READ pixelIntegerZoom WRITE setPixelIntegerZoom NOTIFY pixelIntegerZoomChanged)
    Q_PROPERTY(bool pixelCheckerboard READ pixelCheckerboard WRITE setPixelCheckerboard NOTIFY pixelCheckerboardChanged)
    Q_PROPERTY(bool pixelGrid READ pixelGrid WRITE setPixelGrid NOTIFY pixelGridChanged)
    Q_PROPERTY(bool pixelReadout READ pixelReadout WRITE setPixelReadout NOTIFY pixelReadoutChanged)
    Q_PROPERTY(bool pixelSharpEdit READ pixelSharpEdit WRITE setPixelSharpEdit NOTIFY pixelSharpEditChanged)

public:
    explicit AppSettings(QObject *parent = nullptr);

    int slideshowIntervalMs() const { return m_slideshowIntervalMs; }
    void setSlideshowIntervalMs(int value);

    bool slideshowRandom() const { return m_slideshowRandom; }
    void setSlideshowRandom(bool value);

    bool slideshowLoop() const { return m_slideshowLoop; }
    void setSlideshowLoop(bool value);

    int batchExportQuality() const { return m_batchExportQuality; }
    void setBatchExportQuality(int value);

    int batchRenamePadding() const { return m_batchRenamePadding; }
    void setBatchRenamePadding(int value);

    int stripThumbSize() const { return m_stripThumbSize; }
    void setStripThumbSize(int value);
    int stripColumns() const { return m_stripColumns; }
    void setStripColumns(int value);
    QString stripMode() const { return m_stripMode; }
    void setStripMode(const QString &value);

    bool smoothZoom() const { return m_smoothZoom; }
    void setSmoothZoom(bool value);

    bool pixelMode() const { return m_pixelMode; }
    void setPixelMode(bool value);
    // Zoom in whole-number steps; a small picture opens enlarged to a whole number.
    bool pixelIntegerZoom() const { return m_pixelIntegerZoom; }
    void setPixelIntegerZoom(bool value);
    // A checkerboard behind the transparent parts.
    bool pixelCheckerboard() const { return m_pixelCheckerboard; }
    void setPixelCheckerboard(bool value);
    // A grid between the picture's own pixels from 8x.
    bool pixelGrid() const { return m_pixelGrid; }
    void setPixelGrid(bool value);
    // Position and colour of the pixel under the cursor.
    bool pixelReadout() const { return m_pixelReadout; }
    void setPixelReadout(bool value);
    // "Tamaño" and "Enderezar" resample by nearest neighbour (no new colours).
    bool pixelSharpEdit() const { return m_pixelSharpEdit; }
    void setPixelSharpEdit(bool value);

    bool confirmOverwrite() const { return m_confirmOverwrite; }
    void setConfirmOverwrite(bool value);

signals:
    void slideshowIntervalMsChanged();
    void slideshowRandomChanged();
    void slideshowLoopChanged();
    void batchExportQualityChanged();
    void batchRenamePaddingChanged();
    void confirmOverwriteChanged();
    void stripThumbSizeChanged();
    void stripColumnsChanged();
    void stripModeChanged();
    void smoothZoomChanged();
    void pixelModeChanged();
    void pixelIntegerZoomChanged();
    void pixelCheckerboardChanged();
    void pixelGridChanged();
    void pixelReadoutChanged();
    void pixelSharpEditChanged();

private:
    // Stores `value` under `key` and reports whether it was a change.
    bool storeFlag(bool &member, const QString &key, bool value);

    QSettings m_store;

    int m_slideshowIntervalMs;
    bool m_slideshowRandom;
    bool m_slideshowLoop;
    int m_batchExportQuality;
    int m_batchRenamePadding;
    bool m_confirmOverwrite;
    int m_stripThumbSize = 88;
    int m_stripColumns = 1;
    QString m_stripMode = QStringLiteral("manual");
    bool m_smoothZoom = false;
    bool m_pixelMode = false;
    bool m_pixelIntegerZoom = false;
    bool m_pixelCheckerboard = false;
    bool m_pixelGrid = false;
    bool m_pixelReadout = false;
    bool m_pixelSharpEdit = false;
};
