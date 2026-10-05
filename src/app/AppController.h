#pragma once

#include "ImageLoader.h"
#include "Histogram.h"
#include "edit/EditStack.h"
#include "edit/Operations.h"
#include <QObject>
#include <QUrl>
#include <QSize>
#include <QStringList>
#include <QColor>
#include <QVariantMap>
#include <QVariantList>
#include <QThreadPool>
#include <QTimer>
#include <array>
#include <atomic>
#include <memory>

class ImageProvider;
class FolderModel;

// Single QML-facing entry point for Fase 1. Owns the core::ImageLoader,
// republishes its async results as Qt properties/signals QML can bind to,
// and routes decoded pixels into ImageProvider so ImageCanvas can just bind
// Image.source to currentSource. Also keeps FolderModel in sync whenever a
// file is opened directly (dialog/drag&drop/file association) so the
// filmstrip always reflects the folder of the image currently on screen.
class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentSource READ currentSource NOTIFY currentSourceChanged)
    // The as-opened, never-edited image, for ImageCanvas.qml's before/after
    // compare toggle - unlike currentSource, this never changes across
    // crop/resize/rotate/flip/adjust/filter, only when a genuinely new
    // image is loaded.
    Q_PROPERTY(QString originalSource READ originalSource NOTIFY currentSourceChanged)
    Q_PROPERTY(QSize currentImageSize READ currentImageSize NOTIFY currentImageSizeChanged)
    // A "Guardar como..." dialog's suggested starting name/location: the
    // current file's own folder and base name (no extension), so the user
    // doesn't have to type one from scratch just to export a quick copy.
    Q_PROPERTY(QUrl suggestedSaveUrl READ suggestedSaveUrl NOTIFY currentSourceChanged)
    // Display-ready EXIF rows for the Info popup - {"Cámara": "Canon EOS R5",
    // "ISO": "400", ...}. Empty when the file has no readable metadata.
    Q_PROPERTY(QVariantMap metadata READ metadata NOTIFY metadataChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(QStringList supportedExtensions READ supportedExtensions CONSTANT)

    // Editing (Fase 2). Crop/resize/rotate/flip go through m_editStack (real
    // undo history); brightness/contrast/saturation and the filter preset are
    // a "live" overlay recomputed on top of that instead of being pushed to
    // the stack per slider tick, so dragging a slider doesn't fill undo with
    // dozens of near-identical steps.
    Q_PROPERTY(bool canUndoEdit READ canUndoEdit NOTIFY editStackChanged)
    Q_PROPERTY(bool canRedoEdit READ canRedoEdit NOTIFY editStackChanged)
    Q_PROPERTY(bool hasEdits READ hasEdits NOTIFY editStackChanged)
    Q_PROPERTY(bool toolSessionActive READ toolSessionActive NOTIFY editStackChanged)
    Q_PROPERTY(bool toolSessionDirty READ toolSessionDirty NOTIFY editStackChanged)
    // Changes that exist only in memory: what is on screen is not what was last opened
    // or saved (compared as content - see core::edit::EditRecipe - so Crop, Save, Undo
    // IS unsaved, while edits that cancel each other out are not). An effect that is
    // merely being previewed does not count. Closing the window or opening another
    // picture asks about them first.
    Q_PROPERTY(bool isDirty READ isDirty NOTIFY editStackChanged)
    // True when "Guardar" can write straight over the open file: it came from disk
    // and its format can be written (not a RAW/HEIC, not a pasted picture), and no
    // other picture is being loaded and nothing is being saved right now.
    Q_PROPERTY(bool canSaveInPlace READ canSaveInPlace NOTIFY canSaveInPlaceChanged)
    // True from the moment a save starts until its file is on disk (or it failed). The
    // work happens on a worker thread; the window stays alive meanwhile.
    Q_PROPERTY(bool isSaving READ isSaving NOTIFY isSavingChanged)

    // The Filtros look and the Ajustes sliders are ALL applied live entirely on
    // the GPU, chained in ImageCanvas.qml as: hidden Image (structural pixels
    // only) -> Grade ShaderEffect (the look) -> Grade ShaderEffect (Ajustes)
    // -> Detail ShaderEffect -> visible.
    // These properties just mirror the current live-overlay state for those
    // QML bindings; nothing here triggers a CPU re-bake or image republish.
    //
    // `adjust` is one flat map: every slider name -> its current value
    // (exposure, brightness, ..., sharpness), "negative" (bool), "levels"
    // (4 x {inBlack, gamma, inWhite}: RGB, R, G, B), "outBlack"/"outWhite" and
    // "curves" (4 lists of {x, y} points, same channel order).
    Q_PROPERTY(QVariantMap adjust READ adjustMap NOTIFY liveAdjustChanged)
    // "image://viewer/lut?rev=N": the 256x1 table stage A of the adjustment
    // reads (see core::edit::buildAdjustLut). The revision changes on every
    // adjustment so the Image element re-requests it.
    Q_PROPERTY(QString adjustLutSource READ adjustLutSource NOTIFY liveAdjustChanged)
    // The active Filtros look: its id ("" = none) and how strongly (0..1) it is
    // applied. `look` is the same thing resolved into the numbers Grade.frag
    // takes as uniforms (shadows, highlights, saturation, vibrance, hue,
    // gradientOn, gradient0..2, shadowOffset, highlightOffset, vignette, grain,
    // amount) so the shader binds straight to it; `lookLutSource` is its tone
    // table, like adjustLutSource is for the sliders.
    Q_PROPERTY(QString liveFilterId READ liveFilterId NOTIFY liveFilterChanged)
    Q_PROPERTY(qreal liveFilterIntensity READ liveFilterIntensity NOTIFY liveFilterChanged)
    Q_PROPERTY(QVariantMap look READ lookMap NOTIFY liveFilterChanged)
    // Whether the look / the tone-and-colour stage of Ajustes change anything at
    // all. The canvas drops a stage's GPU texture while it is a no-op (a 37 MP
    // photo costs ~200 MB per stage), drawing the previous stage's output instead.
    Q_PROPERTY(bool lookActive READ lookActive NOTIFY liveFilterChanged)
    Q_PROPERTY(bool gradeActive READ gradeActive NOTIFY liveAdjustChanged)
    Q_PROPERTY(QString lookLutSource READ lookLutSource NOTIFY liveFilterChanged)
    // The catalogue: [{id, name, group}] and [{id, name}] for the groups.
    Q_PROPERTY(QVariantList lookList READ lookList CONSTANT)
    Q_PROPERTY(QVariantList lookGroups READ lookGroups CONSTANT)
    // Bumped each time the preview thumbnails are re-rendered for a new photo;
    // put it in the thumbnail URL: "image://viewer/lookthumb/<id>?rev=N".
    Q_PROPERTY(int lookThumbRevision READ lookThumbRevision NOTIFY lookThumbsChanged)

    // Efectos: one-shot effects (blurs, stylizing, distortions - see
    // core/edit/Effects.h). Picking one shows a PREVIEW of the whole photo with
    // it applied (calculated off the UI thread, never touching the undo
    // history) until commitEffect() bakes it in as an undoable edit, or
    // cancelEffect() throws it away.
    // [{id, name, group}] and [{id, name}] for the groups.
    Q_PROPERTY(QVariantList effectList READ effectList CONSTANT)
    Q_PROPERTY(QVariantList effectGroups READ effectGroups CONSTANT)
    // The effect being previewed ("" = none).
    Q_PROPERTY(QString effectId READ effectId NOTIFY effectChanged)
    // Its sliders, [{label, min, max, def, suffix, integer, toggle}] - changes
    // only when another effect is picked.
    Q_PROPERTY(QVariantList effectParams READ effectParams NOTIFY effectChanged)
    // The names of the picked effect's presets (starting points that set several sliders at once),
    // and the handles it wants drawn over the picture: [{kind: "point"|"vline"|"hline", x, y, label,
    // xmin, xmax, ymin, ymax}] where x / y are slider indices (see EffectOverlay).
    Q_PROPERTY(QStringList effectPresets READ effectPresets NOTIFY effectChanged)
    Q_PROPERTY(QVariantList effectOverlays READ effectOverlays NOTIFY effectChanged)
    // The slider values (as many as effectParams) and the Mezcla amount (0..1);
    // these change on every drag tick.
    Q_PROPERTY(QVariantList effectValues READ effectValues NOTIFY effectValuesChanged)
    Q_PROPERTY(qreal effectMix READ effectMix NOTIFY effectValuesChanged)
    // True while a preview is still being calculated.
    Q_PROPERTY(bool effectBusy READ effectBusy NOTIFY effectBusyChanged)

    // 256 normalized (0..1) luma bucket heights for the Info popup's
    // histogram graph. Reflects the structural bake only (crop/resize/
    // rotate/flip) - recomputing it on every brightness/filter GPU tick
    // would need reading the adjustment back off the GPU, not worth it for
    // what's meant as a rough exposure/contrast reference.
    Q_PROPERTY(QVariantList histogram READ histogram NOTIFY histogramChanged)
    // Per-channel histograms for the Ajustes editors, each a list of 256
    // heights normalized to ~1: {"luma": [...], "r": [...], "g": [...],
    // "b": [...]}. `histogramChannels` is the image going INTO the
    // adjustments (what Levels/Curves are edited against);
    // `adjustedHistogram` is what comes OUT of them (crop/resize/rotate/flip,
    // then the filter preset, then every Ajustes control), measured on a small
    // proxy copy through the same CPU pipeline that writes the file.
    Q_PROPERTY(QVariantMap histogramChannels READ histogramChannels NOTIFY histogramChanged)
    Q_PROPERTY(QVariantMap adjustedHistogram READ adjustedHistogram NOTIFY adjustedHistogramChanged)

    // Animated GIF/APNG playback (see core::AnimatedDecoder). isAnimated
    // only changes alongside a genuinely new file finishing loading, so it
    // reuses that signal instead of a dedicated one.
    Q_PROPERTY(bool isAnimated READ isAnimated NOTIFY newImageLoaded)
    Q_PROPERTY(bool animationPlaying READ animationPlaying NOTIFY animationPlayingChanged)

    // Just the file's display name (e.g. "photo.jpg"), for the toolbar's
    // repurposed left side - currentSource is an "image://..." provider URL,
    // useless for display, and suggestedSaveUrl is shaped for a save dialog
    // (folder + basename, no extension), not a plain file name.
    Q_PROPERTY(QString currentFileName READ currentFileName NOTIFY currentSourceChanged)

public:
    explicit AppController(ImageProvider *provider, FolderModel *folderModel, QObject *parent = nullptr);
    ~AppController() override;

    QString currentSource() const { return m_currentSource; }
    QString originalSource() const;
    QString currentFileName() const;
    QSize currentImageSize() const { return m_currentImageSize; }
    QUrl suggestedSaveUrl() const;
    QVariantMap metadata() const { return m_metadata; }
    bool isLoading() const { return m_isLoading; }
    QString errorString() const { return m_errorString; }
    QStringList supportedExtensions() const;

    // A previewed (not yet applied) effect counts: "undo" then discards it.
    // Inside a tool session (see beginToolSession) undo stops at the point the tool was
    // opened at: the tool is left with Aplicar or Cancelar, not by undoing past its start.
    bool canUndoEdit() const
    {
        const bool stack = m_editStack.canUndo() && (!m_toolSession || m_editStack.undoPosition() > m_toolSessionStart);
        return stack || !m_effectId.isEmpty();
    }
    bool toolSessionActive() const { return m_toolSession; }
    // Something was done since the tool was opened (an edit, or an effect being previewed).
    bool toolSessionDirty() const
    {
        return m_toolSession && (m_editStack.undoPosition() > m_toolSessionStart || !m_effectId.isEmpty());
    }
    bool canRedoEdit() const { return m_editStack.canRedo(); }
    bool hasEdits() const;
    bool isDirty() const;
    bool canSaveInPlace() const;
    bool isSaving() const { return m_saving; }

    // The pixel of the picture on screen at (x, y) - { valid, r, g, b, a } - for the
    // pixel-mode read-out. Not valid outside the picture.
    Q_INVOKABLE QVariantMap pixelAt(int x, int y) const;

    QVariantMap adjustMap() const;
    QString adjustLutSource() const;
    QString liveFilterId() const { return m_liveFilterPreset; }
    qreal liveFilterIntensity() const { return m_liveFilterIntensity; }
    QVariantMap lookMap() const;
    bool lookActive() const;
    bool gradeActive() const;
    QString lookLutSource() const;
    QVariantList lookList() const;
    QVariantList lookGroups() const;
    int lookThumbRevision() const { return m_lookThumbRevision; }
    QVariantList histogram() const;
    QVariantMap histogramChannels() const;
    QVariantMap adjustedHistogram() const;

    QVariantList effectList() const;
    QVariantList effectGroups() const;
    QString effectId() const { return m_effectId; }
    QVariantList effectParams() const;
    QStringList effectPresets() const;
    QVariantList effectOverlays() const;
    QVariantList effectValues() const;
    qreal effectMix() const { return m_effectMix; }
    bool effectBusy() const { return m_effectJobsRunning > 0 || m_effectDebounce.isActive(); }

    bool isAnimated() const { return m_animationFrames.size() > 1; }
    bool animationPlaying() const { return m_animationPlaying; }

public slots:
    // No-op when the current image isn't animated (or already in the
    // requested state). Play/pause only - there's no scrubbing to a
    // specific frame yet, matching the scope of the feature (view/play,
    // not frame-by-frame editing).
    void setAnimationPlaying(bool playing);
    void openFile(const QUrl &fileUrl);
    void openPath(const QString &filePath);

    // Structural edits - go through m_editStack, so undo/redo covers them.
    void cropNormalized(qreal x, qreal y, qreal w, qreal h);
    // `filter` is "lanczos3" (default) or "nearest" (modo pixel: hard-edged, no new colours).
    void resizeImage(int width, int height, bool keepAspectRatio, bool enhanceDetail = true,
                     const QString &filter = QStringLiteral("lanczos3"));
    void rotateEdit90();
    void rotateEditMinus90(); // a quarter turn the other way (the Recortar tool's left button)
    // Fine-grained rotation (any angle, not just 90-degree steps) - the
    // "Enderezar" slider, for leveling a tilted horizon. Committed as one
    // RotateOp on release, same as rotateEdit90() - the slider's live
    // preview while dragging is a cheap view-only transform in
    // ImageCanvas.qml, not a per-tick call into this.
    void straighten(qreal degrees, bool smooth = true);
    void flipEditHorizontal();
    void flipEditVertical();
    void undoEdit();
    // An edit tool (Recortar, Ajustes, ...) was opened / is being left. begin remembers
    // where the history stands; end(keep = true) keeps what the tool did, end(false)
    // undoes all of it and drops the redo tail, so Cancelar leaves no trace.
    void beginToolSession();
    void endToolSession(bool keep);
    void redoEdit();

    // Live overlay - applied on top of the edit stack's result, not part of
    // undo/redo. presetId is a look id from lookList; an empty or unknown id
    // clears the active look. intensity is clamped to 0..1.
    void applyFilterPreset(const QString &presetId, qreal intensity);
    // Renders the look preview thumbnails for the current photo if they are
    // out of date (a new image, or a crop/resize/rotate/flip). Cheap when
    // nothing changed; call it when the Filtros tab is shown.
    void ensureLookThumbnails();

    // Ajustes. `name` is one of the slider names in the `adjust` map
    // (exposure, brightness, contrast, highlights, shadows, whites, blacks,
    // gamma, temperature, tint, saturation, vibrance, hue, clarity,
    // sharpness, vignette, grain); the value is clamped to that slider's range.
    void setAdjustParam(const QString &name, qreal value);
    void setNegative(bool on);
    // The noise/grain control's distribution (0 uniform, 1 gaussian, 2 impulse,
    // 3 laplacian) and whether it is brightness-only (monochrome).
    void setGrainType(int type);
    void setGrainMono(bool mono);
    // channel: 0 = combined RGB, 1..3 = red, green, blue.
    void setLevels(int channel, qreal inBlack, qreal gamma, qreal inWhite);
    void setOutputLevels(qreal black, qreal white);
    // points: a list of {x, y} (0..1) - an empty list or the two corner
    // points clears that channel's curve.
    void setCurvePoints(int channel, const QVariantList &points);
    // The 256 output heights of the curve through `points` - what the curve
    // editor draws, computed by the same code that builds the export.
    QVariantList curveSamples(const QVariantList &points) const;
    // "contrast", "levels" or "color": levels computed from the image's own
    // histogram (replaces the current levels; other sliders are untouched).
    void autoAdjust(const QString &kind);
    // Efectos. selectEffect() starts previewing `id` with its default values
    // (an empty or unknown id cancels); setEffectValue() moves slider `index`
    // (clamped to its range) and setEffectMix() the 0..1 blend with the
    // original - both refresh the preview. commitEffect() makes it permanent
    // (one undo step); cancelEffect() brings the untouched picture back.
    void selectEffect(const QString &id);
    void setEffectValue(int index, qreal value);
    void setEffectMix(qreal mix);
    void resetEffectValues();
    void applyEffectPreset(int index);
    void commitEffect();
    void cancelEffect();

    // Puts every Ajustes control (sliders, levels, curves, negative) back to
    // neutral. Structural edits and the filter preset are untouched.
    void resetAdjust();
    // Just the levels (all four channels plus the output range).
    void resetLevels();

    // Clears everything: history and the live overlay.
    void resetEdits();

    // The answer to the "unsaved changes" question raised by unsavedChangesBlocked():
    // "save" (write the file, then carry on), "discard" (carry on, losing the
    // changes) or "cancel" (stay on this picture).
    void resolveUnsaved(const QString &choice);

    // Overwrites the currently open file (or `targetUrl` if given) with the edited
    // image - ASYNCHRONOUSLY. What the file will hold is captured the moment this is
    // called (an immutable snapshot of the original, the history and the live overlay);
    // a worker thread then renders it, encodes it, carries the metadata over and writes
    // it atomically, while the interface stays responsive. The return value says only
    // whether the save STARTED: false (with errorString set) when it was refused - no
    // picture, another picture is loading, a save is already running, the format cannot
    // be written in place. The outcome arrives as saveFinished(); a failure also sets
    // errorString. "Saved" means what the snapshot held: edits made while the file is
    // being written stay unsaved afterwards.
    bool saveEdited();
    bool saveEditedAs(const QUrl &targetUrl);

    // Copies the current (baked) image to the system clipboard, or replaces
    // the currently open image with whatever image is on the clipboard (if
    // any) - the pasted image has no file path of its own, so "Guardar"
    // stays disabled for it until "Guardar como..." gives it one.
    bool copyToClipboard();
    bool pasteFromClipboard();

    // Sends the current file to the Recycle Bin (not a permanent delete -
    // Windows' own undo-delete still applies) and advances to the next
    // remaining file in the folder, if any.
    bool deleteCurrentFile();

    // Bakes the current image out to a temp PNG and sets it as the Windows
    // desktop wallpaper (SPI_SETDESKWALLPAPER). Returns false if there's no
    // image open or the Windows API call fails.
    bool setAsWallpaper();

signals:
    // Fired alongside currentSourceChanged() whenever the image's actual
    // identity/dimensions changed (a new file, or a structural edit baked)
    // - NOT on every animation frame tick, which reuses currentSourceChanged
    // alone to swap pixels without disturbing the view. ImageCanvas.qml
    // hooks its "reset zoom-to-fit/rotation" logic to this instead of
    // Image.onStatusChanged for exactly that reason.
    void structuralImageChanged();
    void currentSourceChanged();
    void canSaveInPlaceChanged();
    void currentImageSizeChanged();
    void isLoadingChanged();
    void errorStringChanged();
    void isSavingChanged();
    // A save started with saveEdited()/saveEditedAs() has ended: `ok`, and the file it
    // wrote (or tried to).
    void saveFinished(bool ok, const QString &path);
    // A save that WORKED but could not keep something (the original's metadata, say):
    // a short message for a non-blocking notice, not an error.
    void saveNotice(const QString &message);
    void editStackChanged();
    void metadataChanged();
    void liveAdjustChanged();
    void liveFilterChanged();
    void lookThumbsChanged();
    // openPath()/pasteFromClipboard() was asked to replace a picture that has
    // unsaved changes and did nothing yet: the UI must ask, then call resolveUnsaved().
    void unsavedChangesBlocked();
    void effectChanged();
    void effectValuesChanged();
    void effectBusyChanged();
    void histogramChanged();
    void adjustedHistogramChanged();
    // Fired only when a genuinely new file finishes loading - not on every
    // re-bake from an edit - so QML can reset transient UI state (sliders)
    // without fighting edits that also change currentSource.
    void newImageLoaded();
    void animationPlayingChanged();

private:
    void loadPath(const QString &path);
    static QVariantMap buildMetadataDisplay(const core::ImageMetadata &meta);
    // structuralBaked() + the filter preset + m_liveAdjust, all via CPU -
    // used ONLY by saveEdited()/saveEditedAs(), which need one full-quality
    // QImage to write to disk, not a GPU-composited preview.
    QImage currentBaked() const;
    // What the picture would be made of right now (see core::edit::EditRecipe).
    core::edit::EditRecipe currentRecipe() const;
    // Refuses a save (and says why) while another picture is being loaded - until it
    // lands, the pixels and the file they belong to are about to be replaced - or while
    // another save is still running.
    bool saveBlocked();
    // The part of saving that needs the live document (the UI thread): validates,
    // commits a pending effect, captures the snapshot and hands it to the worker.
    bool startSave(const QString &path);
    // What the worker reports back (see finishSave).
    struct SaveOutcome {
        quint64 serial = 0;               // the document the save was started for
        core::edit::EditRecipe recipe;    // what the file holds, if it worked
        QString path;
        bool ok = false;
        QString error;
        QString warning;
        QImage structural;                // the structural bake the worker had to compute, if it did...
        std::vector<core::edit::Operation> ops; // ...and the pixel operations it belongs to
    };
    void finishSave(const SaveOutcome &outcome);
    // Makes a previewed effect a real history step. `republish` false skips rebuilding
    // the picture (the safe-save worker will compute it anyway, off the UI thread).
    void commitPendingEffect(bool republish);
    // Publishes structuralBaked() as-is: the filter preset/brightness/
    // contrast/saturation/sharpness are all applied live by GPU shaders in
    // ImageCanvas.qml on top of whatever this publishes, so baking any of
    // them in here too would apply them twice.
    // `resetView` false skips structuralImageChanged: for a same-size change
    // (an applied effect) the canvas keeps the user's zoom and position.
    void republishBaked(bool resetView = true);
    // Shows `image` as the current picture without touching the edit state:
    // effect previews, and putting the real picture back afterwards.
    void publishPicture(const QImage &image);

    // --- Efectos plumbing -------------------------------------------------
    // Forgets the effect being previewed (cancelling any calculation in flight)
    // WITHOUT restoring the canvas - callers that replace the picture anyway
    // (every structural edit, a new image) use this; cancelEffect() adds the
    // restore.
    void dropEffectPreview();
    // Called after any parameter change: starts the quick preview and
    // (re)arms the debounce timer that launches the exact full-size one.
    void scheduleEffectPreview();
    // stage 1 = preview computed on a shrunken copy and stretched back up;
    // stage 2 = the effect on the real pixels.
    void startEffectJob(int stage);
    void finishEffectJob(int generation, int stage, const QImage &image, bool wasCancelled);
    // structuralBaked() shrunk for the quick preview, cached.
    QImage effectProxy() const;

    // The result of replaying m_editStack (crop/resize/rotate/flip) alone,
    // cached and only recomputed when the structural stack actually changes
    // - not on every brightness/contrast/filter tick, which would otherwise
    // silently re-run a potentially expensive resize/crop on every single
    // slider move. Mutable because it's a cache invalidated by a dirty flag,
    // read from the const currentBaked().
    QImage structuralBaked() const;
    void markStructuralDirty()
    {
        dropEffectPreview(); // an uncommitted effect was computed from the old picture
        m_liveCoalesce.stop(); // the next slider move starts a new history entry
        m_effectProxy = QImage();
        m_structuralDirty = true;
        m_histogramDirty = true;
        m_proxyDirty = true;
        m_adjustedDirty = true;
        m_lookProxyDirty = true;
    }
    // Assigns the Ajustes state, refreshes the GPU lookup table and tells QML, and
    // records the change in the undo history. `coalesceKey` names the control that
    // changed ("p:exposure", "levels:2"...): ticks of the SAME control within a
    // short time are merged into one history entry, so a whole slider drag is one
    // undo step (empty key = always a new entry).
    void setLiveAdjust(const core::edit::AdjustOp &op, const QString &coalesceKey = QString());
    // Puts the current Ajustes + Filtros state into the history (see LiveOp).
    void recordLiveChange(const QString &coalesceKey);
    // Sets the live overlay to what the undo history says it is at the current
    // position (after undo/redo), refreshing the GPU tables and telling QML.
    void syncLiveFromStack();
    // Rebuilds and republishes the tone LUT for m_liveAdjust (no signals).
    void refreshAdjustLut();
    // Same for the active look's tone table.
    void refreshLookLut();
    // structuralBaked() shrunk to a histogram-friendly size, cached.
    QImage histogramProxy() const;
    // Replaces the current image with an already-decoded document (used by
    // both the async loader's `loaded` handler and pasteFromClipboard(),
    // which has no file to hand the loader in the first place).
    void applyNewDocument(const core::ImageDocument &doc);
    // m_animationTimer's timeout handler: publishes the next frame through
    // ImageProvider exactly like republishBaked() does for an edit, then
    // reschedules itself for that new frame's own delay.
    void advanceAnimationFrame();

    core::ImageLoader m_loader;
    ImageProvider *m_provider = nullptr;
    FolderModel *m_folderModel = nullptr;

    QString m_currentSource;
    // The file the pixels in m_originalImage came from. Changes only when a new
    // picture has actually ARRIVED (applyNewDocument), never when one is merely asked
    // for: while B decodes - or if it fails - the document is still A, whole.
    QString m_currentFilePath;
    QSize m_currentImageSize;
    QVariantMap m_metadata;
    bool m_isLoading = false;
    QString m_errorString;
    int m_revision = 0;
    int m_originalRevision = 0;

    QImage m_originalImage; // full-resolution decode of m_currentFilePath
    // The ICC profile still describing m_originalImage's pixels - only when they are NOT
    // sRGB (a profile that could not be applied at decode time). Empty normally.
    QByteArray m_iccProfile;
    core::edit::EditStack m_editStack;
    core::edit::AdjustOp m_liveAdjust;
    QString m_liveFilterPreset;
    qreal m_liveFilterIntensity = 1.0;

    mutable QImage m_structuralBaked;
    mutable bool m_structuralDirty = true;
    mutable core::Histogram m_histogramCache;
    mutable bool m_histogramDirty = true;
    mutable QImage m_proxyCache;
    mutable bool m_proxyDirty = true;
    mutable QVariantMap m_adjustedCache;
    mutable bool m_adjustedDirty = true;
    int m_lutRevision = 0;
    int m_lookLutRevision = 0;
    int m_lookThumbRevision = 0;
    bool m_lookProxyDirty = true;

    // The undo history entry the last Ajustes/Filtros change went into, and for how
    // long further changes of the same control keep merging into it.
    QString m_liveCoalesceKey;
    QTimer m_liveCoalesce;
    // The recipe of the picture as it was opened or last written to disk; isDirty() is
    // "the current recipe is not this one".
    core::edit::EditRecipe m_savedRecipe;
    // The recipe when the load in flight started: if it differs when the picture
    // lands, the user kept editing the old one meanwhile.
    core::edit::EditRecipe m_loadStartRecipe;
    // What openPath()/pasteFromClipboard() would have done if it had not been
    // blocked by unsaved changes; resolveUnsaved() carries it out.
    enum class PendingAction { None, OpenPath, Paste };
    PendingAction m_pendingAction = PendingAction::None;
    QString m_pendingPath;
    bool m_discardApproved = false;

    // Saving. One at a time; the work runs on m_savePool (which the destructor waits
    // for: the worker posts its result back to `this`).
    QThreadPool m_savePool;
    bool m_saving = false;
    // Which document the running save belongs to: bumped by every applyNewDocument().
    quint64 m_docSerial = 0;
    // What to do once the running save has succeeded: opening another picture or
    // pasting was asked for while it ran (or the "save" answer to the unsaved-changes
    // question). Dropped if the save fails.
    PendingAction m_afterSaveAction = PendingAction::None;
    QString m_afterSavePath;

    // Efectos preview state. A "generation" is one set of parameters: any
    // change starts a new one, which cancels the previous one's work (its
    // `m_effectCancel` flag) so stale results are never shown.
    QString m_effectId;
    bool m_toolSession = false;
    size_t m_toolSessionStart = 0;
    core::edit::EffectValues m_effectValues{};
    double m_effectMix = 1.0;
    int m_effectGeneration = 0;
    int m_effectJobsRunning = 0;
    std::shared_ptr<std::atomic<bool>> m_effectCancel;
    // The effect calculations run here (not on the application-wide pool), so the
    // destructor can wait for exactly these - they hold `this` - and nothing else.
    QThreadPool m_effectPool;
    QTimer m_effectDebounce; // single-shot: starts the full-size calculation once the sliders rest
    QImage m_effectFull;     // the exact full-size result for the current generation...
    bool m_effectFullValid = false; // ...once it has been calculated
    bool m_effectShown = false;     // a preview (not the real picture) is on the canvas
    mutable QImage m_effectProxy;

    // Animated GIF/APNG playback state. Empty/size-1 frames means the
    // current image is a plain static one - editing/crop/filters keep
    // working exactly as before in that case, untouched by any of this.
    QVector<QImage> m_animationFrames;
    QVector<int> m_animationDelaysMs;
    int m_animationFrameIndex = 0;
    bool m_animationPlaying = false;
    QTimer m_animationTimer; // single-shot, restarted per-frame with that frame's own delay

    static constexpr auto kCurrentRequestId = "current";
};
