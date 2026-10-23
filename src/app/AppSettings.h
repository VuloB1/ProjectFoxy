#pragma once

#include <QObject>
#include <QRect>
#include <QSettings>

// User-configurable defaults for the batch operations and the interface,
// persisted to a plain .ini file (QSettings::IniFormat, under the app's
// standard config location) rather than the Windows registry - deliberately
// avoiding the registry entirely for this, unlike the Explorer context-menu
// integration (which the user runs themselves via a .reg file).
class AppSettings : public QObject {
    Q_OBJECT
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
    // closes it), "open" (always), "closed" (never) or "auto" (closes while editing and in
    // narrow windows).
    Q_PROPERTY(int stripThumbSize READ stripThumbSize WRITE setStripThumbSize NOTIFY stripThumbSizeChanged)
    Q_PROPERTY(int stripColumns READ stripColumns WRITE setStripColumns NOTIFY stripColumnsChanged)
    Q_PROPERTY(QString stripMode READ stripMode WRITE setStripMode NOTIFY stripModeChanged)
    // "Modo pixel" (Configuración > Apariencia). `pixelMode` is the switch itself: pictures stay
    // sharp (no smoothing) once zoomed in, up to 64x. The rest are extras of that mode, each one
    // on its own switch, and only count while the mode is on.
    // Wheel zoom: in fixed steps (false) or glided smoothly towards the new size (true).
    // The language of the interface: "system" (follows Windows) or es / en / pt / ko / zh / ja.
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(bool smoothZoom READ smoothZoom WRITE setSmoothZoom NOTIFY smoothZoomChanged)
    // The comic reader's sharpening (the button at the bottom right of the window while reading): FidelityFX CAS
    // applied to what is on screen, 0..100 %, 0 = off. It changes how the page looks, never the file.
    Q_PROPERTY(int viewSharpen READ viewSharpen WRITE setViewSharpen NOTIFY viewSharpenChanged)
    Q_PROPERTY(bool pixelMode READ pixelMode WRITE setPixelMode NOTIFY pixelModeChanged)
    Q_PROPERTY(bool pixelIntegerZoom READ pixelIntegerZoom WRITE setPixelIntegerZoom NOTIFY pixelIntegerZoomChanged)
    Q_PROPERTY(bool pixelCheckerboard READ pixelCheckerboard WRITE setPixelCheckerboard NOTIFY pixelCheckerboardChanged)
    Q_PROPERTY(bool pixelGrid READ pixelGrid WRITE setPixelGrid NOTIFY pixelGridChanged)
    Q_PROPERTY(bool pixelSharpEdit READ pixelSharpEdit WRITE setPixelSharpEdit NOTIFY pixelSharpEditChanged)
    // Opening a picture while the program is running uses that window instead of starting another one.
    // Read when the program starts: a change applies the next time it is opened.
    Q_PROPERTY(bool singleInstance READ singleInstance WRITE setSingleInstance NOTIFY singleInstanceChanged)
    // Presenting each frame in step with the screen's refresh (on by default). Some AMD cards stall for hundreds of
    // milliseconds in Direct3D 11 while the window is resized with it on; off avoids that and may tear on screens
    // without FreeSync / G-Sync. Read when the program starts: a change applies the next time it is opened.
    // the text-recognition languages in use (Tesseract codes), Linux
    Q_PROPERTY(QStringList ocrLanguages READ ocrLanguages WRITE setOcrLanguages NOTIFY ocrLanguagesChanged)
    Q_PROPERTY(bool startWithWindows READ startWithWindows WRITE setStartWithWindows NOTIFY startWithWindowsChanged)
    Q_PROPERTY(bool startMinimized READ startMinimized WRITE setStartMinimized NOTIFY startMinimizedChanged)
    Q_PROPERTY(bool keepInTray READ keepInTray WRITE setKeepInTray NOTIFY keepInTrayChanged)
    Q_PROPERTY(bool verticalSync READ verticalSync WRITE setVerticalSync NOTIFY verticalSyncChanged)
    // Look on GitHub for a newer version when the program starts (at most once a day; on by default).
    Q_PROPERTY(bool updateCheck READ updateCheck WRITE setUpdateCheck NOTIFY updateCheckChanged)
    // Decorations for the holidays (qml/FestiveDecor.qml; on by default).
    Q_PROPERTY(bool festive READ festive WRITE setFestive NOTIFY festiveChanged)
    // The holidays (keys of core/Festivity) the person turned off, and whether the big piece in the corner of the
    // thumbnail bar is shown (the garland always is, while its holiday is on).
    Q_PROPERTY(QStringList festiveOff READ festiveOff NOTIFY festiveOffChanged)
    Q_PROPERTY(bool festiveCorner READ festiveCorner WRITE setFestiveCorner NOTIFY festiveCornerChanged)

public:
    explicit AppSettings(QObject *parent = nullptr);

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

    QString language() const { return m_language; }
    void setLanguage(const QString &value);

    bool smoothZoom() const { return m_smoothZoom; }
    int viewSharpen() const { return m_viewSharpen; }
    void setViewSharpen(int percent);
    void setSmoothZoom(bool value);

    bool pixelMode() const { return m_pixelMode; }
    void setPixelMode(bool value);
    // Zoom in whole-number steps; a small picture opens enlarged to a whole number.
    bool pixelIntegerZoom() const { return m_pixelIntegerZoom; }
    void setPixelIntegerZoom(bool value);
    // A checkerboard behind the transparent parts.
    bool pixelCheckerboard() const { return m_pixelCheckerboard; }
    void setPixelCheckerboard(bool value);
    bool updateCheck() const { return m_updateCheck; }
    void setUpdateCheck(bool value);
    bool festive() const { return m_festive; }
    void setFestive(bool value);
    QStringList festiveOff() const { return m_festiveOff; }
    Q_INVOKABLE void setFestiveOn(const QString &key, bool on);
    bool festiveCorner() const { return m_festiveCorner; }
    void setFestiveCorner(bool value);
    // When the last check succeeded (seconds since the epoch; 0 = never) and the version the person chose to skip.
    qint64 lastUpdateCheck() const { return m_lastUpdateCheck; }
    void setLastUpdateCheck(qint64 secs);
    QString skippedUpdate() const { return m_skippedUpdate; }
    void setSkippedUpdate(const QString &version);
    bool singleInstance() const { return m_singleInstance; }
    void setSingleInstance(bool value);
    // Where the window was the last time (its normal, not maximised, rectangle; null when never saved) and whether it
    // was maximised. Read once at startup and written when the program closes (main.cpp).
    QRect windowRect() const;
    bool windowMaximized() const { return m_windowMaximized; }
    void saveWindowState(const QRect &normalRect, bool maximized);

    QStringList ocrLanguages() const { return m_ocrLanguages; }
    void setOcrLanguages(const QStringList &value);
    bool startWithWindows() const { return m_startWithWindows; }
    void setStartWithWindows(bool value);
    bool startMinimized() const { return m_startMinimized; }
    void setStartMinimized(bool value);
    // At sign-in: no window, only the tray icon (that needs the tray, or there would be no way back to it).
    bool startsHidden() const { return m_startMinimized && m_keepInTray; }
    bool keepInTray() const { return m_keepInTray; }
    void setKeepInTray(bool value);
    bool verticalSync() const { return m_verticalSync; }
    void setVerticalSync(bool value);
    // A grid between the picture's own pixels from 8x.
    bool pixelGrid() const { return m_pixelGrid; }
    void setPixelGrid(bool value);
    // Position and colour of the pixel under the cursor.
    // "Tamaño" and "Enderezar" resample by nearest neighbour (no new colours).
    bool pixelSharpEdit() const { return m_pixelSharpEdit; }
    void setPixelSharpEdit(bool value);

    bool confirmOverwrite() const { return m_confirmOverwrite; }
    void setConfirmOverwrite(bool value);

signals:
    void batchExportQualityChanged();
    void batchRenamePaddingChanged();
    void confirmOverwriteChanged();
    void stripThumbSizeChanged();
    void stripColumnsChanged();
    void stripModeChanged();
    void languageChanged();
    void smoothZoomChanged();
    void viewSharpenChanged();
    void pixelModeChanged();
    void pixelIntegerZoomChanged();
    void pixelCheckerboardChanged();
    void pixelGridChanged();
    void pixelSharpEditChanged();
    void singleInstanceChanged();
    void ocrLanguagesChanged();
    void startWithWindowsChanged();
    void startMinimizedChanged();
    void keepInTrayChanged();
    void verticalSyncChanged();
    void updateCheckChanged();
    void festiveChanged();
    void festiveOffChanged();
    void festiveCornerChanged();

private:
    // Stores `value` under `key` and reports whether it was a change.
    bool storeFlag(bool &member, const QString &key, bool value);

    QSettings m_store;

    int m_batchExportQuality;
    int m_batchRenamePadding;
    bool m_windowMaximized = false;
    bool m_confirmOverwrite;
    int m_stripThumbSize = 88;
    int m_stripColumns = 1;
    QString m_stripMode = QStringLiteral("manual");
    QString m_language = QStringLiteral("system");
    bool m_smoothZoom = false;
    int m_viewSharpen = 0;
    bool m_pixelMode = false;
    bool m_pixelIntegerZoom = false;
    bool m_pixelCheckerboard = false;
    bool m_pixelGrid = false;
    bool m_pixelSharpEdit = false;
    bool m_singleInstance = true;
    bool m_keepInTray = false;
    QStringList m_ocrLanguages{QStringLiteral("spa"), QStringLiteral("eng")};
    bool m_startWithWindows = false;
    bool m_startMinimized = true;
    bool m_verticalSync = true;
    bool m_updateCheck = true;
    bool m_festive = true;
    QStringList m_festiveOff;
    bool m_festiveCorner = true;
    qint64 m_lastUpdateCheck = 0;
    QString m_skippedUpdate;
};
