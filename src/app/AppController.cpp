#include "AppController.h"
#include "ImageProvider.h"
#include "FolderModel.h"
#include "DecoderRegistry.h"
#include "ImageWriter.h"
#include "Histogram.h"
#include "edit/AdjustMath.h"
#include "edit/Effects.h"
#include "edit/Looks.h"
#include <algorithm>
#include <array>
#include <QVector3D>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QGuiApplication>
#include <QClipboard>
#include <QMimeData>
#include <QRunnable>
#include <QThreadPool>

// NOMINMAX before Windows.h - otherwise its min/max macros shadow std::min/
// std::max used throughout this file (and everywhere that includes
// AppController.h afterward).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shellapi.h>

namespace {
// Further moves of the same Ajustes/Filtros control within this long of the last
// one belong to the same gesture (one undo step).
constexpr int kLiveCoalesceMs = 900;
} // namespace

using core::edit::AdjustOp;
using core::edit::CropOp;
using core::edit::FilterPresetOp;
using core::edit::FlipOp;
using core::edit::ResizeOp;
using core::edit::RotateOp;

AppController::AppController(ImageProvider *provider, FolderModel *folderModel, QObject *parent)
    : QObject(parent)
    , m_provider(provider)
    , m_folderModel(folderModel)
{
    m_effectPool.setMaxThreadCount(3); // the quick preview, the exact one, and one being cancelled
    m_savePool.setMaxThreadCount(1);   // one save at a time
    m_animationTimer.setSingleShot(true);
    connect(&m_animationTimer, &QTimer::timeout, this, &AppController::advanceAnimationFrame);
    m_liveCoalesce.setSingleShot(true);
    m_liveCoalesce.setInterval(kLiveCoalesceMs);
    m_effectDebounce.setSingleShot(true);
    connect(&m_effectDebounce, &QTimer::timeout, this, [this]() { startEffectJob(2); });
    refreshAdjustLut();
    refreshLookLut();

    // "Guardar" depends on both of these (see canSaveInPlace).
    connect(this, &AppController::currentSourceChanged, this, &AppController::canSaveInPlaceChanged);
    connect(this, &AppController::isLoadingChanged, this, &AppController::canSaveInPlaceChanged);
    connect(this, &AppController::isSavingChanged, this, &AppController::canSaveInPlaceChanged);

    connect(&m_loader, &core::ImageLoader::loaded, this,
            [this](const QString &requestId, core::ImageDocument doc) {
                if (requestId != QLatin1String(kCurrentRequestId))
                    return; // stale/cancelled request - ignore

                // The user kept editing the OLD picture while this one was decoding:
                // replacing it now would silently throw those edits away. Ask the same
                // question as for any other replacement (the picture is decoded again
                // from disk if they answer "save" or "discard").
                if (currentRecipe() != m_loadStartRecipe && isDirty()) {
                    m_isLoading = false;
                    m_pendingAction = PendingAction::OpenPath;
                    m_pendingPath = doc.filePath;
                    if (!m_currentFilePath.isEmpty())
                        m_folderModel->openFolderForFile(m_currentFilePath);
                    emit isLoadingChanged();
                    emit unsavedChangesBlocked();
                    return;
                }
                applyNewDocument(doc);
            });

    connect(&m_loader, &core::ImageLoader::failed, this,
            [this](const QString &requestId, QString error) {
                if (requestId != QLatin1String(kCurrentRequestId))
                    return;

                m_isLoading = false;
                m_errorString = error;
                emit isLoadingChanged();
                emit errorStringChanged();
            });
}

AppController::~AppController()
{
    // Stop any effect still being calculated and let the workers finish, so none can
    // post its result to an object that no longer exists. The kernels poll the
    // cancel flag between rows, so this is a short wait - and it is a complete one:
    // a worker that outlived a timeout would be holding a dangling `this`.
    m_effectDebounce.stop();
    if (m_effectCancel)
        m_effectCancel->store(true);
    m_effectPool.waitForDone();
    // A save that is still writing is allowed to finish: closing the program must not
    // leave a half-done file (the write is atomic, but the user asked for it).
    m_savePool.waitForDone();
}

QStringList AppController::supportedExtensions() const
{
    QStringList filters;
    const auto extensions = core::DecoderRegistry::instance().allSupportedExtensions();
    filters.reserve(extensions.size());
    for (const auto &ext : extensions)
        filters << QStringLiteral("*.%1").arg(ext);
    return filters;
}

void AppController::openFile(const QUrl &fileUrl)
{
    loadPath(fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString());
}

void AppController::openPath(const QString &filePath)
{
    loadPath(filePath);
}

void AppController::loadPath(const QString &path)
{
    // A save is still writing this picture's file: wait for it, then go (the picture is
    // clean by then unless it was edited meanwhile, and the usual question follows).
    if (m_saving) {
        m_afterSaveAction = PendingAction::OpenPath;
        m_afterSavePath = path;
        return;
    }

    // Replacing the picture would throw away edits nobody has saved: do nothing yet
    // and let the UI ask (it calls resolveUnsaved() with the answer).
    if (isDirty() && !m_discardApproved) {
        m_pendingAction = PendingAction::OpenPath;
        m_pendingPath = path;
        // The folder strip already moved to the requested file; put it back on the
        // one that is still showing.
        if (!m_currentFilePath.isEmpty())
            m_folderModel->openFolderForFile(m_currentFilePath);
        emit unsavedChangesBlocked();
        return;
    }

    // m_currentFilePath is NOT touched here: the document stays A, in full, until B has
    // actually arrived (applyNewDocument) - and if B fails it still is.
    m_isLoading = true;
    m_loadStartRecipe = currentRecipe();
    m_errorString.clear();
    emit isLoadingChanged();
    emit errorStringChanged();

    m_loader.requestLoad(QLatin1String(kCurrentRequestId), path);

    // Silently syncs the filmstrip to this file's folder/position - does
    // NOT emit currentFilePathChanged, so this never loops back into us.
    m_folderModel->openFolderForFile(path);
}

void AppController::applyNewDocument(const core::ImageDocument &doc)
{
    // The one place the document changes: pixels and the file they came from move
    // together (an empty path = a pasted picture that no file backs yet).
    ++m_docSerial; // a save started for the previous picture must not touch this one's state
    m_currentFilePath = doc.filePath;
    m_iccProfile = doc.iccProfile;
    m_originalImage = doc.pixels;
    m_editStack.clear();
    m_toolSession = false;
    m_toolSessionStart = 0;
    {
        // The history remembers finished full-size results so undo/redo of a slow
        // edit is instant. Give it a quarter of the memory that is free right now
        // (between 256 MB and 1.5 GB) rather than a number that ignores the machine.
        MEMORYSTATUSEX mem{};
        mem.dwLength = sizeof(mem);
        qint64 budget = 600LL * 1024 * 1024;
        if (GlobalMemoryStatusEx(&mem))
            budget = std::clamp<qint64>(qint64(mem.ullAvailPhys / 4), 256LL * 1024 * 1024, 1536LL * 1024 * 1024);
        m_editStack.setCheckpointBudget(budget);
    }
    m_liveAdjust = AdjustOp();
    refreshAdjustLut();
    m_liveFilterPreset.clear();
    m_liveFilterIntensity = 1.0;
    refreshLookLut();
    markStructuralDirty();
    m_savedRecipe = currentRecipe(); // a freshly opened picture has nothing to lose
    m_liveCoalesceKey.clear();

    m_provider->setImage(doc.pixels);
    m_provider->setOriginalImage(doc.pixels);
    m_currentImageSize = doc.sourceSize;
    m_metadata = buildMetadataDisplay(doc.metadata);
    m_isLoading = false;
    m_errorString.clear();

    // stop() first: a pending timer from the PREVIOUS animation must never
    // fire after this point, since it would publish a stale frame from the
    // image being replaced right now.
    m_animationTimer.stop();
    m_animationFrames = doc.animationFrames;
    m_animationDelaysMs = doc.animationDelaysMs;
    m_animationFrameIndex = 0;
    m_animationPlaying = m_animationFrames.size() > 1;
    if (m_animationPlaying)
        m_animationTimer.start(qBound(20, m_animationDelaysMs.value(0, 100), 10000));

    // Bump the revision so the "image://viewer/current?rev=N" URL changes
    // and QML's Image actually re-requests the provider instead of serving
    // its own cached pixmap.
    ++m_revision;
    ++m_originalRevision;
    m_currentSource = QStringLiteral("image://viewer/current?rev=%1").arg(m_revision);

    emit currentSourceChanged();
    emit structuralImageChanged();
    emit currentImageSizeChanged();
    emit metadataChanged();
    emit isLoadingChanged();
    emit errorStringChanged();
    emit editStackChanged();
    emit liveAdjustChanged();
    emit liveFilterChanged();
    emit histogramChanged();
    emit adjustedHistogramChanged();
    emit animationPlayingChanged();
    emit newImageLoaded();
}

void AppController::advanceAnimationFrame()
{
    if (m_animationFrames.size() <= 1)
        return;

    m_animationFrameIndex = (m_animationFrameIndex + 1) % m_animationFrames.size();
    m_provider->setImage(m_animationFrames.at(m_animationFrameIndex));
    ++m_revision;
    m_currentSource = QStringLiteral("image://viewer/current?rev=%1").arg(m_revision);
    emit currentSourceChanged();

    if (m_animationPlaying) {
        const int delay = qBound(20, m_animationDelaysMs.value(m_animationFrameIndex, 100), 10000);
        m_animationTimer.start(delay);
    }
}

void AppController::setAnimationPlaying(bool playing)
{
    if (m_animationFrames.size() <= 1 || playing == m_animationPlaying)
        return;

    m_animationPlaying = playing;
    if (playing) {
        const int delay = qBound(20, m_animationDelaysMs.value(m_animationFrameIndex, 100), 10000);
        m_animationTimer.start(delay);
    } else {
        m_animationTimer.stop();
    }
    emit animationPlayingChanged();
}

QString AppController::originalSource() const
{
    if (m_originalImage.isNull())
        return {};
    return QStringLiteral("image://viewer/original?rev=%1").arg(m_originalRevision);
}

QString AppController::currentFileName() const
{
    if (m_currentFilePath.isEmpty())
        return {};
    return QFileInfo(m_currentFilePath).fileName();
}

QUrl AppController::suggestedSaveUrl() const
{
    if (m_currentFilePath.isEmpty())
        return {};
    const QFileInfo info(m_currentFilePath);
    return QUrl::fromLocalFile(info.absolutePath() + QLatin1Char('/') + info.completeBaseName() + QStringLiteral(".png"));
}

QVariantMap AppController::buildMetadataDisplay(const core::ImageMetadata &meta)
{
    QVariantMap display;

    const QString camera = (meta.cameraMake + QLatin1Char(' ') + meta.cameraModel).trimmed();
    if (!camera.isEmpty())
        display.insert(QStringLiteral("Cámara"), camera);

    if (meta.dateTaken.isValid())
        display.insert(QStringLiteral("Fecha"), meta.dateTaken.toString(QStringLiteral("dd/MM/yyyy HH:mm")));

    if (meta.focalLengthMm > 0.0)
        display.insert(QStringLiteral("Distancia focal"), QStringLiteral("%1 mm").arg(meta.focalLengthMm, 0, 'g', 3));

    if (meta.apertureF > 0.0)
        display.insert(QStringLiteral("Apertura"), QStringLiteral("f/%1").arg(meta.apertureF, 0, 'g', 2));

    if (meta.exposureSeconds > 0.0) {
        const QString shutter = meta.exposureSeconds < 1.0
            ? QStringLiteral("1/%1 s").arg(qRound(1.0 / meta.exposureSeconds))
            : QStringLiteral("%1 s").arg(meta.exposureSeconds, 0, 'g', 3);
        display.insert(QStringLiteral("Velocidad"), shutter);
    }

    if (meta.isoSpeed > 0)
        display.insert(QStringLiteral("ISO"), QString::number(meta.isoSpeed));

    if (meta.hasGps) {
        display.insert(QStringLiteral("GPS"),
                        QStringLiteral("%1, %2").arg(meta.gpsLatitude, 0, 'f', 5).arg(meta.gpsLongitude, 0, 'f', 5));
    }

    return display;
}

bool AppController::hasEdits() const
{
    // LiveOps in the history do not count by themselves: what matters is whether
    // the picture would come out different from the original.
    return m_editStack.hasActivePixelOps() || !m_liveAdjust.isIdentity() || !m_liveFilterPreset.isEmpty()
        || !m_effectId.isEmpty();
}

core::edit::EditRecipe AppController::currentRecipe() const
{
    core::edit::EditRecipe recipe;
    recipe.pixelOps = m_editStack.activePixelOps();
    recipe.adjust = m_liveAdjust.isIdentity() ? AdjustOp() : m_liveAdjust;
    recipe.lookId = m_liveFilterPreset;
    recipe.lookAmount = m_liveFilterPreset.isEmpty() ? 1.0 : m_liveFilterIntensity;
    return recipe; // (an effect that is only being previewed is not part of it yet)
}

bool AppController::isDirty() const
{
    return currentRecipe() != m_savedRecipe;
}

bool AppController::canSaveInPlace() const
{
    if (m_isLoading || m_saving || m_currentFilePath.isEmpty())
        return false;
    return core::supportedSaveExtensions().contains(QFileInfo(m_currentFilePath).suffix().toLower());
}

bool AppController::saveBlocked()
{
    if (!m_isLoading && !m_saving)
        return false;
    m_errorString = m_isLoading
        ? QStringLiteral("Espera a que termine de cargarse la otra imagen antes de guardar.")
        : QStringLiteral("Ya se está guardando: espera a que termine.");
    emit errorStringChanged();
    return true;
}

QImage AppController::structuralBaked() const
{
    if (m_structuralDirty) {
        m_structuralBaked = m_editStack.bake(m_originalImage);
        m_structuralDirty = false;
    }
    return m_structuralBaked;
}

namespace {

// 256 heights normalized against the tallest bucket that is not one of the two
// end ones: a photo with clipped shadows/highlights has a giant spike at 0
// and/or 255 that would otherwise flatten the rest of the graph to a line.
QVariantList normalizedBins(const std::array<int, 256> &bins)
{
    int peak = 0;
    for (int i = 1; i < 255; ++i)
        peak = std::max(peak, bins[i]);
    if (peak == 0)
        peak = std::max({1, bins[0], bins[255]});

    QVariantList list;
    list.reserve(256);
    for (int n : bins)
        list.append(std::min(1.0, double(n) / double(peak)));
    return list;
}

QVariantMap channelHistogramMap(const core::Histogram &h)
{
    QVariantMap map;
    map.insert(QStringLiteral("luma"), normalizedBins(h.luma));
    map.insert(QStringLiteral("r"), normalizedBins(h.r));
    map.insert(QStringLiteral("g"), normalizedBins(h.g));
    map.insert(QStringLiteral("b"), normalizedBins(h.b));
    return map;
}

} // namespace

QVariantList AppController::histogram() const
{
    if (m_histogramDirty) {
        m_histogramCache = core::computeHistogram(structuralBaked());
        m_histogramDirty = false;
    }
    QVariantList list;
    list.reserve(256);
    for (int count : m_histogramCache.luma)
        list.append(double(count) / double(m_histogramCache.maxCount));
    return list;
}

QVariantMap AppController::histogramChannels() const
{
    if (m_histogramDirty) {
        m_histogramCache = core::computeHistogram(structuralBaked());
        m_histogramDirty = false;
    }
    return channelHistogramMap(m_histogramCache);
}

QImage AppController::histogramProxy() const
{
    if (m_proxyDirty) {
        const QImage baked = structuralBaked();
        constexpr int kLongSide = 384;
        if (baked.isNull())
            m_proxyCache = QImage();
        else if (std::max(baked.width(), baked.height()) > kLongSide)
            m_proxyCache = baked.scaled(kLongSide, kLongSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        else
            m_proxyCache = baked;
        m_proxyDirty = false;
    }
    return m_proxyCache;
}

QVariantMap AppController::adjustedHistogram() const
{
    if (m_adjustedDirty) {
        QImage img = histogramProxy();
        if (!img.isNull()) {
            if (!m_liveFilterPreset.isEmpty())
                img = core::edit::applyOperation(img, FilterPresetOp{m_liveFilterPreset, m_liveFilterIntensity});
            img = core::edit::applyOperation(img, m_liveAdjust);
        }
        m_adjustedCache = channelHistogramMap(core::computeHistogram(img));
        m_adjustedDirty = false;
    }
    return m_adjustedCache;
}

QImage AppController::currentBaked() const
{
    if (m_originalImage.isNull())
        return {};

    QImage img = structuralBaked();
    // An effect that is only being previewed is part of what is on screen, so it is
    // part of what gets saved / copied - but it is NOT committed: the history is
    // not touched and the preview stays pending (the user can still cancel it).
    if (!m_effectId.isEmpty() && m_effectMix > 0.0) {
        img = (m_effectFullValid && !m_effectFull.isNull())
                  ? m_effectFull // the exact full-size result, already calculated
                  : core::edit::applyEffect(img, m_effectId, m_effectValues, m_effectMix);
    }
    return core::edit::renderLiveOverlay(img, m_liveFilterPreset, m_liveFilterIntensity, m_liveAdjust);
}

void AppController::republishBaked(bool resetView)
{
    const QImage baked = structuralBaked();
    m_provider->setImage(baked);
    m_currentImageSize = baked.size();
    ++m_revision;
    m_currentSource = QStringLiteral("image://viewer/current?rev=%1").arg(m_revision);
    emit currentSourceChanged();
    if (resetView)
        emit structuralImageChanged();
    emit currentImageSizeChanged();
    emit editStackChanged();
    emit histogramChanged();
    emit adjustedHistogramChanged();
}

void AppController::cropNormalized(qreal x, qreal y, qreal w, qreal h)
{
    if (m_originalImage.isNull())
        return;
    m_editStack.push(CropOp{QRectF(x, y, w, h)});
    markStructuralDirty();
    republishBaked();
}

void AppController::resizeImage(int width, int height, bool keepAspectRatio, bool enhanceDetail, const QString &filter)
{
    if (m_originalImage.isNull() || width <= 0 || height <= 0)
        return;
    const QString name = filter == QLatin1String("nearest") ? filter : QStringLiteral("lanczos3");
    m_editStack.push(ResizeOp{QSize(width, height), keepAspectRatio, name, enhanceDetail});
    markStructuralDirty();
    republishBaked();
}

void AppController::rotateEdit90()
{
    if (m_originalImage.isNull())
        return;
    m_editStack.push(RotateOp{90});
    markStructuralDirty();
    republishBaked();
}

void AppController::rotateEditMinus90()
{
    if (m_originalImage.isNull())
        return;
    m_editStack.push(RotateOp{270});
    markStructuralDirty();
    republishBaked();
}

void AppController::straighten(qreal degrees, bool smooth)
{
    if (m_originalImage.isNull() || qFuzzyIsNull(degrees))
        return;
    m_editStack.push(RotateOp{degrees, smooth});
    markStructuralDirty();
    republishBaked();
}

void AppController::flipEditHorizontal()
{
    if (m_originalImage.isNull())
        return;
    m_editStack.push(FlipOp{true, false});
    markStructuralDirty();
    republishBaked();
}

void AppController::flipEditVertical()
{
    if (m_originalImage.isNull())
        return;
    m_editStack.push(FlipOp{false, true});
    markStructuralDirty();
    republishBaked();
}

void AppController::beginToolSession()
{
    cancelEffect();
    m_liveCoalesce.stop(); // the first change made in the tool must be an entry of its own
    m_liveCoalesceKey.clear();
    m_toolSession = true;
    m_toolSessionStart = m_editStack.undoPosition();
    emit editStackChanged();
}

void AppController::endToolSession(bool keep)
{
    if (!m_toolSession)
        return;
    cancelEffect();
    m_liveCoalesce.stop();
    m_liveCoalesceKey.clear();
    if (!keep) {
        bool pixelsChanged = false;
        while (m_editStack.undoPosition() > m_toolSessionStart) {
            if (!m_editStack.undoTargetIsLive())
                pixelsChanged = true;
            m_editStack.undo();
        }
        m_editStack.truncateRedo();
        if (pixelsChanged) {
            markStructuralDirty();
            republishBaked();
        }
        syncLiveFromStack(); // the Ajustes / Filtros overlay is whatever the history says now
    }
    m_toolSession = false;
    emit editStackChanged();
}

void AppController::undoEdit()
{
    // With an effect being previewed, "undo" means "never mind that effect".
    if (!m_effectId.isEmpty()) {
        cancelEffect();
        return;
    }
    if (m_toolSession && m_editStack.undoPosition() <= m_toolSessionStart)
        return; // the history from before the tool was opened is not this tool's to undo
    const bool liveStep = m_editStack.undoTargetIsLive();
    if (!m_editStack.undo())
        return;
    if (liveStep) {
        // Only the Ajustes/Filtros overlay changes: no pixels to re-bake, no
        // reset of the view.
        m_liveCoalesce.stop();
        syncLiveFromStack();
    } else {
        markStructuralDirty();
        republishBaked();
    }
}

void AppController::redoEdit()
{
    const bool liveStep = m_editStack.redoTargetIsLive();
    if (!m_editStack.redo())
        return;
    if (liveStep) {
        m_liveCoalesce.stop();
        syncLiveFromStack();
    } else {
        markStructuralDirty();
        republishBaked();
    }
}

void AppController::syncLiveFromStack()
{
    // Copy first: the pointer is into the history, which must not be read after
    // anything below could change it.
    const core::edit::LiveOp *state = m_editStack.activeLiveState();
    const AdjustOp adjust = state ? state->adjust : AdjustOp();
    const QString look = state ? state->lookId : QString();
    const double amount = state ? state->lookAmount : 1.0;

    m_liveAdjust = adjust;
    refreshAdjustLut();
    m_liveFilterPreset = look;
    m_liveFilterIntensity = amount;
    refreshLookLut();
    m_adjustedDirty = true;
    emit liveAdjustChanged();
    emit liveFilterChanged();
    emit adjustedHistogramChanged();
    emit editStackChanged();
}

void AppController::recordLiveChange(const QString &coalesceKey)
{
    const core::edit::LiveOp snapshot{m_liveAdjust, m_liveFilterPreset, m_liveFilterIntensity};
    const bool merged = !coalesceKey.isEmpty() && coalesceKey == m_liveCoalesceKey && m_liveCoalesce.isActive()
        && m_editStack.replaceLastLive(snapshot);
    if (!merged)
        m_editStack.push(snapshot);
    m_liveCoalesceKey = coalesceKey;
    m_liveCoalesce.start();
}

QVariantMap AppController::pixelAt(int x, int y) const
{
    QVariantMap map;
    const QColor c = m_provider->pixelAt(x, y);
    map.insert(QStringLiteral("valid"), c.isValid());
    if (c.isValid()) {
        map.insert(QStringLiteral("r"), c.red());
        map.insert(QStringLiteral("g"), c.green());
        map.insert(QStringLiteral("b"), c.blue());
        map.insert(QStringLiteral("a"), c.alpha());
    }
    return map;
}

QVariantMap AppController::adjustMap() const
{
    const AdjustOp &op = m_liveAdjust;
    QVariantMap map;
    map.insert(QStringLiteral("exposure"), op.exposure);
    map.insert(QStringLiteral("brightness"), op.brightness);
    map.insert(QStringLiteral("contrast"), op.contrast);
    map.insert(QStringLiteral("highlights"), op.highlights);
    map.insert(QStringLiteral("shadows"), op.shadows);
    map.insert(QStringLiteral("whites"), op.whites);
    map.insert(QStringLiteral("blacks"), op.blacks);
    map.insert(QStringLiteral("gamma"), op.gamma);
    map.insert(QStringLiteral("temperature"), op.temperature);
    map.insert(QStringLiteral("tint"), op.tint);
    map.insert(QStringLiteral("saturation"), op.saturation);
    map.insert(QStringLiteral("vibrance"), op.vibrance);
    map.insert(QStringLiteral("hue"), op.hue);
    map.insert(QStringLiteral("clarity"), op.clarity);
    map.insert(QStringLiteral("sharpness"), op.sharpness);
    map.insert(QStringLiteral("vignette"), op.vignette);
    map.insert(QStringLiteral("grain"), op.grain);
    map.insert(QStringLiteral("grainType"), op.grainType);
    map.insert(QStringLiteral("grainMono"), op.grainMono);
    map.insert(QStringLiteral("negative"), op.negative);
    map.insert(QStringLiteral("outBlack"), op.outBlack);
    map.insert(QStringLiteral("outWhite"), op.outWhite);

    QVariantList levels;
    for (const auto &lv : op.levels) {
        QVariantMap m;
        m.insert(QStringLiteral("inBlack"), lv.inBlack);
        m.insert(QStringLiteral("gamma"), lv.gamma);
        m.insert(QStringLiteral("inWhite"), lv.inWhite);
        levels.append(m);
    }
    map.insert(QStringLiteral("levels"), levels);

    QVariantList curves;
    for (const auto &curve : op.curves) {
        QVariantList pts;
        for (const QPointF &p : curve) {
            QVariantMap m;
            m.insert(QStringLiteral("x"), p.x());
            m.insert(QStringLiteral("y"), p.y());
            pts.append(m);
        }
        // fromValue on purpose: QVariantList::append(QVariantList) would
        // CONCATENATE the points into `curves` instead of nesting one list
        // per channel, and the editor would read a single point back.
        curves.append(QVariant::fromValue(pts));
    }
    map.insert(QStringLiteral("curves"), curves);
    return map;
}

QString AppController::adjustLutSource() const
{
    return QStringLiteral("image://viewer/lut?rev=%1").arg(m_lutRevision);
}

void AppController::refreshAdjustLut()
{
    m_provider->setLut(core::edit::lutToImage(core::edit::buildAdjustLut(m_liveAdjust)));
    ++m_lutRevision;
}

void AppController::setLiveAdjust(const AdjustOp &op, const QString &coalesceKey)
{
    m_liveAdjust = op;
    refreshAdjustLut();
    m_adjustedDirty = true;
    recordLiveChange(coalesceKey);
    // No CPU re-bake here: ImageCanvas.qml's Adjust/Detail ShaderEffects read
    // these straight off this signal and apply them live on the GPU.
    emit liveAdjustChanged();
    emit adjustedHistogramChanged();
    emit editStackChanged();
}

void AppController::setAdjustParam(const QString &name, qreal value)
{
    if (m_originalImage.isNull())
        return;

    struct Param {
        const char *name;
        double AdjustOp::*field;
        double lo;
        double hi;
    };
    static const Param kParams[] = {
        {"exposure", &AdjustOp::exposure, -1.0, 1.0},
        {"brightness", &AdjustOp::brightness, -1.0, 1.0},
        {"contrast", &AdjustOp::contrast, -1.0, 1.0},
        {"highlights", &AdjustOp::highlights, -1.0, 1.0},
        {"shadows", &AdjustOp::shadows, -1.0, 1.0},
        {"whites", &AdjustOp::whites, -1.0, 1.0},
        {"blacks", &AdjustOp::blacks, -1.0, 1.0},
        {"gamma", &AdjustOp::gamma, -1.0, 1.0},
        {"temperature", &AdjustOp::temperature, -1.0, 1.0},
        {"tint", &AdjustOp::tint, -1.0, 1.0},
        {"saturation", &AdjustOp::saturation, -1.0, 1.0},
        {"vibrance", &AdjustOp::vibrance, -1.0, 1.0},
        {"hue", &AdjustOp::hue, -1.0, 1.0},
        {"clarity", &AdjustOp::clarity, -1.0, 1.0},
        {"sharpness", &AdjustOp::sharpness, 0.0, 1.0},
        {"vignette", &AdjustOp::vignette, -1.0, 1.0},
        {"grain", &AdjustOp::grain, 0.0, 1.0},
    };
    for (const Param &p : kParams) {
        if (name != QLatin1String(p.name))
            continue;
        const double clamped = std::clamp(double(value), p.lo, p.hi);
        if (m_liveAdjust.*(p.field) == clamped)
            return;
        AdjustOp op = m_liveAdjust;
        op.*(p.field) = clamped;
        setLiveAdjust(op, QStringLiteral("p:") + name);
        return;
    }
}

void AppController::setGrainType(int type)
{
    type = std::clamp(type, 0, 3);
    if (m_originalImage.isNull() || m_liveAdjust.grainType == type)
        return;
    AdjustOp op = m_liveAdjust;
    op.grainType = type;
    setLiveAdjust(op, QStringLiteral("grainType"));
}

void AppController::setGrainMono(bool mono)
{
    if (m_originalImage.isNull() || m_liveAdjust.grainMono == mono)
        return;
    AdjustOp op = m_liveAdjust;
    op.grainMono = mono;
    setLiveAdjust(op, QStringLiteral("grainMono"));
}

void AppController::setNegative(bool on)
{
    if (m_originalImage.isNull() || m_liveAdjust.negative == on)
        return;
    AdjustOp op = m_liveAdjust;
    op.negative = on;
    setLiveAdjust(op, QStringLiteral("negative"));
}

void AppController::setLevels(int channel, qreal inBlack, qreal gamma, qreal inWhite)
{
    if (m_originalImage.isNull() || channel < 0 || channel > 3)
        return;
    core::edit::LevelsChannel lv;
    lv.inBlack = std::clamp(double(inBlack), 0.0, 0.98);
    lv.inWhite = std::clamp(double(inWhite), lv.inBlack + 0.02, 1.0);
    lv.gamma = std::clamp(double(gamma), 0.1, 9.99);
    const auto &cur = m_liveAdjust.levels[channel];
    if (cur.inBlack == lv.inBlack && cur.gamma == lv.gamma && cur.inWhite == lv.inWhite)
        return;
    AdjustOp op = m_liveAdjust;
    op.levels[channel] = lv;
    setLiveAdjust(op, QStringLiteral("levels:%1").arg(channel));
}

void AppController::setOutputLevels(qreal black, qreal white)
{
    if (m_originalImage.isNull())
        return;
    const double b = std::clamp(double(black), 0.0, 0.98);
    const double w = std::clamp(double(white), b + 0.02, 1.0);
    if (m_liveAdjust.outBlack == b && m_liveAdjust.outWhite == w)
        return;
    AdjustOp op = m_liveAdjust;
    op.outBlack = b;
    op.outWhite = w;
    setLiveAdjust(op, QStringLiteral("outputLevels"));
}

namespace {

// Accepts what QML hands over for a point: a {x, y} object or a Qt.point().
bool pointFromVariant(const QVariant &v, QPointF *out)
{
    if (v.typeId() == QMetaType::QVariantMap) {
        const QVariantMap m = v.toMap();
        *out = QPointF(m.value(QStringLiteral("x")).toDouble(), m.value(QStringLiteral("y")).toDouble());
        return true;
    }
    if (v.canConvert<QPointF>()) {
        *out = v.toPointF();
        return true;
    }
    return false;
}

} // namespace

void AppController::setCurvePoints(int channel, const QVariantList &points)
{
    if (m_originalImage.isNull() || channel < 0 || channel > 3)
        return;

    core::edit::CurvePoints curve;
    for (const QVariant &v : points) {
        QPointF p;
        if (!pointFromVariant(v, &p))
            continue;
        curve.emplace_back(std::clamp(p.x(), 0.0, 1.0), std::clamp(p.y(), 0.0, 1.0));
        if (curve.size() >= 16)
            break;
    }
    std::sort(curve.begin(), curve.end(), [](const QPointF &a, const QPointF &b) { return a.x() < b.x(); });
    if (core::edit::isIdentityCurve(curve))
        curve.clear();

    if (m_liveAdjust.curves[channel] == curve)
        return;
    AdjustOp op = m_liveAdjust;
    op.curves[channel] = curve;
    setLiveAdjust(op, QStringLiteral("curves:%1").arg(channel));
}

QVariantList AppController::curveSamples(const QVariantList &points) const
{
    core::edit::CurvePoints curve;
    for (const QVariant &v : points) {
        QPointF p;
        if (pointFromVariant(v, &p))
            curve.push_back(p);
    }
    const auto samples = core::edit::sampleCurve(curve);
    QVariantList list;
    list.reserve(256);
    for (double s : samples)
        list.append(s);
    return list;
}

void AppController::autoAdjust(const QString &kind)
{
    if (m_originalImage.isNull())
        return;
    core::edit::AutoAdjustKind k = core::edit::AutoAdjustKind::Contrast;
    if (kind == QLatin1String("levels"))
        k = core::edit::AutoAdjustKind::Levels;
    else if (kind == QLatin1String("color"))
        k = core::edit::AutoAdjustKind::Color;
    else if (kind != QLatin1String("contrast"))
        return;
    setLiveAdjust(core::edit::autoAdjusted(m_liveAdjust, structuralBaked(), k));
}

void AppController::resetAdjust()
{
    if (m_originalImage.isNull() || m_liveAdjust.isIdentity())
        return;
    setLiveAdjust(AdjustOp());
}

void AppController::resetLevels()
{
    if (m_originalImage.isNull())
        return;
    AdjustOp op = m_liveAdjust;
    op.levels = {};
    op.outBlack = 0.0;
    op.outWhite = 1.0;
    setLiveAdjust(op);
}

void AppController::refreshLookLut()
{
    const core::edit::LookSpec *spec = core::edit::findLook(m_liveFilterPreset);
    m_provider->setLookLut(core::edit::lutToImage(core::edit::buildAdjustLut(spec ? spec->tone : AdjustOp())));
    ++m_lookLutRevision;
}

QString AppController::lookLutSource() const
{
    return QStringLiteral("image://viewer/looklut?rev=%1").arg(m_lookLutRevision);
}

bool AppController::lookActive() const
{
    return core::edit::findLook(m_liveFilterPreset) != nullptr && m_liveFilterIntensity > 0.0;
}

bool AppController::gradeActive() const
{
    // Clarity, sharpness, vignette and grain belong to the Detail stage, which always runs.
    AdjustOp grade = m_liveAdjust;
    grade.clarity = 0.0;
    grade.sharpness = 0.0;
    grade.vignette = 0.0;
    grade.grain = 0.0;
    return !grade.isIdentity();
}

QVariantMap AppController::lookMap() const
{
    const core::edit::LookSpec *spec = core::edit::findLook(m_liveFilterPreset);
    const AdjustOp tone = spec ? spec->tone : AdjustOp();
    const core::edit::GradeExtras extras = spec ? spec->extras : core::edit::GradeExtras();
    auto vec = [](const std::array<double, 3> &a) { return QVector3D(float(a[0]), float(a[1]), float(a[2])); };

    QVariantMap map;
    map.insert(QStringLiteral("id"), m_liveFilterPreset);
    map.insert(QStringLiteral("amount"), spec ? m_liveFilterIntensity : 0.0);
    map.insert(QStringLiteral("shadows"), tone.shadows);
    map.insert(QStringLiteral("highlights"), tone.highlights);
    map.insert(QStringLiteral("saturation"), tone.saturation);
    map.insert(QStringLiteral("vibrance"), tone.vibrance);
    map.insert(QStringLiteral("hue"), tone.hue);
    map.insert(QStringLiteral("gradientOn"), extras.gradient ? 1.0 : 0.0);
    map.insert(QStringLiteral("gradient0"), vec(extras.stops[0]));
    map.insert(QStringLiteral("gradient1"), vec(extras.stops[1]));
    map.insert(QStringLiteral("gradient2"), vec(extras.stops[2]));
    map.insert(QStringLiteral("shadowOffset"), vec(extras.shadowOffset));
    map.insert(QStringLiteral("highlightOffset"), vec(extras.highlightOffset));
    map.insert(QStringLiteral("vignette"), extras.vignette);
    map.insert(QStringLiteral("grain"), extras.grain);
    map.insert(QStringLiteral("grainType"), extras.grainType);
    map.insert(QStringLiteral("grainMono"), extras.grainMono ? 1.0 : 0.0);
    return map;
}

QVariantList AppController::lookList() const
{
    QVariantList list;
    for (const core::edit::LookSpec &look : core::edit::allLooks()) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), look.id);
        m.insert(QStringLiteral("name"), look.name);
        m.insert(QStringLiteral("group"), look.group);
        list.append(m);
    }
    return list;
}

QVariantList AppController::lookGroups() const
{
    QVariantList list;
    for (const core::edit::LookGroup &group : core::edit::lookGroups()) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), group.id);
        m.insert(QStringLiteral("name"), group.name);
        list.append(m);
    }
    return list;
}

void AppController::ensureLookThumbnails()
{
    if (!m_lookProxyDirty || m_originalImage.isNull())
        return;

    // A small centered square of the photo (as it is before any look or
    // slider), so the grid of previews is uniform whatever the photo's shape.
    QImage square;
    const QImage proxy = histogramProxy();
    if (!proxy.isNull()) {
        const int side = std::min(proxy.width(), proxy.height());
        const QRect crop((proxy.width() - side) / 2, (proxy.height() - side) / 2, side, side);
        square = proxy.copy(crop)
                     .scaled(128, 128, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                     .convertToFormat(QImage::Format_RGBA8888);
    }
    m_provider->setLookProxy(square);
    m_lookProxyDirty = false;
    ++m_lookThumbRevision;
    emit lookThumbsChanged();
}

void AppController::applyFilterPreset(const QString &presetId, qreal intensity)
{
    if (m_originalImage.isNull())
        return;
    m_liveFilterPreset = core::edit::findLook(presetId) ? presetId : QString();
    m_liveFilterIntensity = std::clamp(double(intensity), 0.0, 1.0);
    refreshLookLut();
    m_adjustedDirty = true;
    recordLiveChange(QStringLiteral("look"));
    // No CPU re-bake here either: ImageCanvas.qml's first Grade ShaderEffect
    // reads `look` and `lookLutSource` and applies the look live.
    emit liveFilterChanged();
    emit adjustedHistogramChanged();
    emit editStackChanged();
}

// --- Efectos ------------------------------------------------------------------

namespace {

// The quick preview is calculated on a copy shrunk to this long side and then
// stretched back to the real size; it is only worth the shortcut above
// kEffectProxyThreshold.
constexpr int kEffectProxySide = 1600;
constexpr int kEffectProxyThreshold = 2200;
// How long the sliders must rest before the exact full-size calculation starts
// (shorter for small pictures, where it is nearly instant anyway).
constexpr int kEffectSettleMs = 260;
constexpr int kEffectSmallSettleMs = 40;

} // namespace

QVariantList AppController::effectList() const
{
    QVariantList list;
    for (const core::edit::EffectSpec &fx : core::edit::allEffects()) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), fx.id);
        m.insert(QStringLiteral("name"), fx.name);
        m.insert(QStringLiteral("group"), fx.group);
        list.append(m);
    }
    return list;
}

QVariantList AppController::effectGroups() const
{
    QVariantList list;
    for (const core::edit::EffectGroup &group : core::edit::effectGroups()) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), group.id);
        m.insert(QStringLiteral("name"), group.name);
        list.append(m);
    }
    return list;
}

QVariantList AppController::effectParams() const
{
    QVariantList list;
    const core::edit::EffectSpec *spec = core::edit::findEffect(m_effectId);
    if (!spec)
        return list;
    for (const core::edit::EffectParam &p : spec->params) {
        QVariantMap m;
        m.insert(QStringLiteral("label"), p.label);
        m.insert(QStringLiteral("min"), p.min);
        m.insert(QStringLiteral("max"), p.max);
        m.insert(QStringLiteral("def"), p.def);
        m.insert(QStringLiteral("suffix"), p.suffix);
        m.insert(QStringLiteral("integer"), p.integer);
        m.insert(QStringLiteral("toggle"), p.toggle);
        m.insert(QStringLiteral("options"), p.options);
        list.append(m);
    }
    return list;
}

QVariantList AppController::effectValues() const
{
    QVariantList list;
    const core::edit::EffectSpec *spec = core::edit::findEffect(m_effectId);
    if (!spec)
        return list;
    for (size_t i = 0; i < spec->params.size() && i < m_effectValues.size(); ++i)
        list.append(m_effectValues[i]);
    return list;
}

QImage AppController::effectProxy() const
{
    if (m_effectProxy.isNull()) {
        const QImage base = structuralBaked();
        if (std::max(base.width(), base.height()) > kEffectProxySide)
            m_effectProxy = base.scaled(kEffectProxySide, kEffectProxySide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        else
            m_effectProxy = base;
    }
    return m_effectProxy;
}

void AppController::publishPicture(const QImage &image)
{
    m_provider->setImage(image);
    ++m_revision;
    m_currentSource = QStringLiteral("image://viewer/current?rev=%1").arg(m_revision);
    // Deliberately NOT structuralImageChanged: the picture is the same size, so
    // the canvas must keep the user's zoom and position.
    emit currentSourceChanged();
}

void AppController::dropEffectPreview()
{
    m_effectDebounce.stop();
    if (m_effectCancel) {
        m_effectCancel->store(true); // anything still being calculated is now stale
        m_effectCancel.reset();
    }
    ++m_effectGeneration;
    m_effectFull = QImage();
    m_effectFullValid = false;
    m_effectShown = false;
    if (m_effectId.isEmpty())
        return;
    m_effectId.clear();
    m_effectValues = {};
    m_effectMix = 1.0;
    emit effectChanged();
    emit effectValuesChanged();
    emit effectBusyChanged();
    emit editStackChanged(); // hasEdits() counts a previewed effect
}

void AppController::selectEffect(const QString &id)
{
    if (m_originalImage.isNull() || isAnimated())
        return;
    const core::edit::EffectSpec *spec = core::edit::findEffect(id);
    if (!spec) {
        cancelEffect();
        return;
    }
    if (id == m_effectId)
        return;
    const bool firstEffect = m_effectId.isEmpty();
    m_effectId = id;
    m_effectValues = core::edit::defaultEffectValues(*spec);
    m_effectMix = 1.0;
    emit effectChanged();
    emit effectValuesChanged();
    if (firstEffect)
        emit editStackChanged(); // hasEdits() counts a previewed effect
    scheduleEffectPreview();
}

void AppController::setEffectValue(int index, qreal value)
{
    const core::edit::EffectSpec *spec = core::edit::findEffect(m_effectId);
    if (!spec || index < 0 || size_t(index) >= spec->params.size())
        return;
    const core::edit::EffectParam &p = spec->params[size_t(index)];
    double v = std::clamp(double(value), p.min, p.max);
    if (p.integer)
        v = std::round(v);
    if (v == m_effectValues[size_t(index)])
        return;
    m_effectValues[size_t(index)] = v;
    emit effectValuesChanged();
    scheduleEffectPreview();
}

void AppController::setEffectMix(qreal mix)
{
    if (m_effectId.isEmpty())
        return;
    const double v = std::clamp(double(mix), 0.0, 1.0);
    if (v == m_effectMix)
        return;
    m_effectMix = v;
    emit effectValuesChanged();
    scheduleEffectPreview();
}

void AppController::resetEffectValues()
{
    const core::edit::EffectSpec *spec = core::edit::findEffect(m_effectId);
    if (!spec)
        return;
    m_effectValues = core::edit::defaultEffectValues(*spec);
    m_effectMix = 1.0;
    emit effectValuesChanged();
    scheduleEffectPreview();
}

void AppController::scheduleEffectPreview()
{
    if (m_effectId.isEmpty() || m_originalImage.isNull())
        return;

    // A new generation: whatever the previous one is still calculating is stale.
    if (m_effectCancel)
        m_effectCancel->store(true);
    m_effectCancel = std::make_shared<std::atomic<bool>>(false);
    ++m_effectGeneration;
    m_effectFull = QImage();
    m_effectFullValid = false;

    const QImage base = structuralBaked();
    if (std::max(base.width(), base.height()) > kEffectProxyThreshold) {
        startEffectJob(1); // approximate, immediately...
        m_effectDebounce.start(kEffectSettleMs); // ...exact once the sliders rest
    } else {
        m_effectDebounce.start(kEffectSmallSettleMs);
    }
    emit effectBusyChanged();
}

void AppController::startEffectJob(int stage)
{
    if (m_effectId.isEmpty() || !m_effectCancel)
        return;
    const QImage base = structuralBaked();
    if (base.isNull())
        return;

    const QImage input = stage == 1 ? effectProxy() : base;
    const QString id = m_effectId;
    const core::edit::EffectValues values = m_effectValues;
    const double mix = m_effectMix;
    const int generation = m_effectGeneration;
    const std::shared_ptr<std::atomic<bool>> cancel = m_effectCancel;
    const QSize fullSize = base.size();

    ++m_effectJobsRunning;
    emit effectBusyChanged();

    // m_effectPool runs this task; its row bands go to the dedicated
    // rowWorkerPool (see ParallelRows.h), so waiting on them cannot deadlock.
    m_effectPool.start(QRunnable::create([=, this]() {
        QImage out = core::edit::applyEffect(input, id, values, mix, cancel.get());
        const bool cancelled = cancel->load();
        if (cancelled)
            out = QImage(); // a stale, half-written result: do not carry a full-size buffer around until the GUI thread discards it
        else if (out.size() != fullSize)
            out = out.scaled(fullSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        QMetaObject::invokeMethod(
            this, [this, generation, stage, out, cancelled]() { finishEffectJob(generation, stage, out, cancelled); },
            Qt::QueuedConnection);
    }));
}

void AppController::finishEffectJob(int generation, int stage, const QImage &image, bool wasCancelled)
{
    --m_effectJobsRunning;
    const bool current = !wasCancelled && generation == m_effectGeneration && !m_effectId.isEmpty();
    if (current) {
        if (stage == 2) {
            m_effectFull = image;
            m_effectFullValid = true;
            m_effectShown = true;
            publishPicture(image);
        } else if (!m_effectFullValid) { // the approximate one, unless the exact one already beat it
            m_effectShown = true;
            publishPicture(image);
        }
    }
    emit effectBusyChanged();
}

void AppController::cancelEffect()
{
    if (m_effectId.isEmpty())
        return;
    const bool shown = m_effectShown;
    dropEffectPreview();
    if (shown)
        publishPicture(structuralBaked());
}

void AppController::commitEffect()
{
    commitPendingEffect(true);
}

void AppController::commitPendingEffect(bool republish)
{
    if (m_effectId.isEmpty() || m_originalImage.isNull())
        return;
    if (m_effectMix <= 0.0) { // nothing would change
        cancelEffect();
        return;
    }

    const core::edit::EffectOp op{m_effectId, m_effectValues, m_effectMix};
    // When the exact full-size result for these very settings is already
    // there, it IS what replaying the stack would produce - reuse it instead of
    // calculating it a second time.
    const bool exact = m_effectFullValid && m_effectCancel && !m_effectCancel->load();
    const QImage full = m_effectFull;

    m_editStack.push(op);
    markStructuralDirty(); // also drops the preview state
    if (exact) {
        m_structuralBaked = full;
        m_structuralDirty = false;
        m_editStack.storeCheckpoint(m_originalImage, full); // so Undo then Redo does not recalculate it
    }
    // Without the exact result the picture would have to be calculated right here, on the
    // UI thread. When the caller is going to calculate it anyway (a save), it asks not to.
    if (republish || exact)
        republishBaked(false);
}


void AppController::resetEdits()
{
    if (m_originalImage.isNull())
        return;
    m_editStack.clear();
    m_liveAdjust = AdjustOp();
    refreshAdjustLut();
    m_liveFilterPreset.clear();
    m_liveFilterIntensity = 1.0;
    refreshLookLut();
    markStructuralDirty();
    republishBaked();
    emit liveAdjustChanged();
    emit liveFilterChanged();
}

bool AppController::saveEdited()
{
    if (saveBlocked())
        return false;
    if (!canSaveInPlace()) {
        m_errorString = m_currentFilePath.isEmpty()
            ? QStringLiteral("Esta imagen no tiene archivo: usa «Guardar como…».")
            : QStringLiteral("Este formato no se puede escribir en el mismo archivo: usa «Guardar como…».");
        emit errorStringChanged();
        return false;
    }
    return saveEditedAs(QUrl::fromLocalFile(m_currentFilePath));
}

bool AppController::saveEditedAs(const QUrl &targetUrl)
{
    if (saveBlocked())
        return false;
    QString path = targetUrl.isLocalFile() ? targetUrl.toLocalFile() : targetUrl.toString();
    if (m_originalImage.isNull() || path.isEmpty())
        return false;

    // No extension typed in the save dialog (e.g. its filename field was
    // edited down to just a bare name) - the encoder has nothing to guess the
    // format from. Default to PNG.
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".png");

    return startSave(path);
}

bool AppController::startSave(const QString &path)
{
    // An effect that is only being previewed is on screen, so it goes into the file -
    // and becomes a real, undoable step first. Writing it WITHOUT committing would
    // leave the file with something the history does not have: "Cancelar efecto" would
    // then put the screen back to a state the file contradicts. (When the exact result
    // is already calculated this is instant; otherwise the effect is calculated by the
    // worker below, not here.)
    if (!m_effectId.isEmpty())
        commitPendingEffect(false);

    // ---- the snapshot: everything the worker needs, taken now, immutable from here on.
    // QImage is copy-on-write, so holding these costs no pixels; the history and the
    // live overlay are small records.
    const QImage original = m_originalImage;
    const std::vector<core::edit::Operation> ops = m_editStack.activePixelOps();
    const QImage structural = m_structuralDirty ? QImage() : m_structuralBaked; // null: the worker replays `ops`
    const AdjustOp adjust = m_liveAdjust;
    const QString lookId = m_liveFilterPreset;
    const double lookAmount = m_liveFilterIntensity;
    const core::edit::EditRecipe recipe = currentRecipe();
    const quint64 serial = m_docSerial;

    // The file is written atomically (a failure leaves the original untouched),
    // JPEG/WebP at high quality, and the original's camera / GPS / date information
    // travels with the picture - see core::saveImageWithOptions(). The colour profile
    // does NOT come from the original: the pixels are sRGB, and the file is tagged so.
    core::SaveOptions options;
    if (!m_currentFilePath.isEmpty() && QFileInfo::exists(m_currentFilePath))
        options.metadataSource = m_currentFilePath;
    options.iccProfile = m_iccProfile; // empty (the normal case) = write an sRGB profile

    m_errorString.clear();
    m_saving = true;
    emit errorStringChanged();
    emit isSavingChanged();

    m_savePool.start(QRunnable::create([=, this]() {
        SaveOutcome outcome;
        outcome.serial = serial;
        outcome.recipe = recipe;
        outcome.path = path;

        QImage base = structural;
        if (base.isNull()) {
            base = core::edit::bakeOperations(original, ops);
            outcome.structural = base; // handed back so the UI can reuse it instead of recalculating
            outcome.ops = ops;
        }
        const QImage rendered = core::edit::renderLiveOverlay(base, lookId, lookAmount, adjust);
        if (rendered.isNull()) {
            outcome.error = QStringLiteral("No hay imagen que guardar");
        } else {
            const core::SaveResult result = core::saveImageWithOptions(rendered, path, options);
            outcome.ok = result.ok;
            outcome.error = result.error;
            outcome.warning = result.warning;
        }
        QMetaObject::invokeMethod(this, [this, outcome]() { finishSave(outcome); }, Qt::QueuedConnection);
    }));
    return true;
}

void AppController::finishSave(const SaveOutcome &outcome)
{
    m_saving = false;
    emit isSavingChanged();

    const PendingAction after = m_afterSaveAction;
    const QString afterPath = m_afterSavePath;
    m_afterSaveAction = PendingAction::None;
    m_afterSavePath.clear();

    if (!outcome.ok) {
        m_errorString = QStringLiteral("No se pudo guardar «%1»: %2")
                            .arg(QDir::toNativeSeparators(outcome.path), outcome.error);
        emit errorStringChanged();
        emit saveFinished(false, outcome.path);
        // Whatever was waiting on this save is dropped; the filmstrip may already have
        // moved to the picture that was asked for - put it back on the open one.
        if (after != PendingAction::None && !m_currentFilePath.isEmpty())
            m_folderModel->openFolderForFile(m_currentFilePath);
        return;
    }

    // What counts as saved is what the snapshot held - NOT whatever is on screen now: an
    // edit made while the file was being written is still unsaved.
    if (outcome.serial == m_docSerial) {
        m_savedRecipe = outcome.recipe;
        // The worker had to calculate the structural picture (an effect committed by the
        // save, say): keep it, if the document still wants exactly that.
        if (!outcome.structural.isNull() && m_structuralDirty && m_editStack.activePixelOps() == outcome.ops) {
            m_structuralBaked = outcome.structural;
            m_structuralDirty = false;
            republishBaked(false);
        }
        emit editStackChanged();
    }
    if (!outcome.warning.isEmpty()) {
        qWarning().noquote() << "Guardado con aviso:" << outcome.warning;
        emit saveNotice(QStringLiteral("La imagen se guardó, pero: %1.").arg(outcome.warning));
    }
    emit saveFinished(true, outcome.path);

    // The request that was waiting for the file to be safe.
    if (after == PendingAction::OpenPath)
        loadPath(afterPath);
    else if (after == PendingAction::Paste)
        pasteFromClipboard();
}

void AppController::resolveUnsaved(const QString &choice)
{
    const PendingAction action = m_pendingAction;
    const QString path = m_pendingPath;
    m_pendingAction = PendingAction::None;
    m_pendingPath.clear();
    if (action == PendingAction::None || choice == QLatin1String("cancel"))
        return; // (loadPath already put the folder strip back on the open picture)

    if (choice == QLatin1String("save")) {
        // Save first; the picture that was asked for is opened when the file is on disk.
        m_afterSaveAction = action;
        m_afterSavePath = path;
        if (!saveEdited()) { // refused: stay, with the error on screen
            m_afterSaveAction = PendingAction::None;
            m_afterSavePath.clear();
        }
        return;
    }

    m_discardApproved = true;
    if (action == PendingAction::OpenPath)
        loadPath(path);
    else
        pasteFromClipboard();
    m_discardApproved = false;
}

bool AppController::copyToClipboard()
{
    const QImage baked = currentBaked();
    if (baked.isNull())
        return false;
    QGuiApplication::clipboard()->setImage(baked);
    return true;
}

bool AppController::pasteFromClipboard()
{
    const QClipboard *clipboard = QGuiApplication::clipboard();

    const QImage img = clipboard->image();
    if (!img.isNull()) {
        if (m_saving) { // see loadPath()
            m_afterSaveAction = PendingAction::Paste;
            m_afterSavePath.clear();
            return false;
        }
        if (isDirty() && !m_discardApproved) {
            m_pendingAction = PendingAction::Paste;
            m_pendingPath.clear();
            emit unsavedChangesBlocked();
            return false;
        }
        core::ImageDocument doc;
        doc.pixels = img.format() == QImage::Format_RGBA8888 ? img : img.convertToFormat(QImage::Format_RGBA8888);
        doc.sourceSize = doc.pixels.size();

        // (doc.filePath is empty: no file backs this image yet - "Guardar" stays
        // disabled until "Guardar como...")
        applyNewDocument(doc);
        return true;
    }

    // Copying a file in Explorer (rather than "copy image" from a browser/
    // editor) puts a file reference on the clipboard, not pixel data -
    // clipboard->image() only ever sees actual bitmap formats. Fall back to
    // loading the first supported image file it references.
    const QMimeData *mime = clipboard->mimeData();
    if (mime && mime->hasUrls()) {
        const QStringList extensions = core::DecoderRegistry::instance().allSupportedExtensions();
        for (const QUrl &url : mime->urls()) {
            if (!url.isLocalFile())
                continue;
            const QString path = url.toLocalFile();
            const QString ext = QFileInfo(path).suffix().toLower();
            if (extensions.contains(ext, Qt::CaseInsensitive)) {
                loadPath(path);
                return true;
            }
        }
    }

    m_errorString = tr("El portapapeles no contiene ninguna imagen.");
    emit errorStringChanged();
    return false;
}

bool AppController::deleteCurrentFile()
{
    if (m_currentFilePath.isEmpty())
        return false;
    if (m_saving) { // the writer is about to (re)create this very file
        m_errorString = QStringLiteral("No se puede eliminar el archivo mientras se guarda.");
        emit errorStringChanged();
        return false;
    }

    // Work out a sibling to show next (if any) while the folder model's
    // list still includes the file about to be deleted - afterward, a
    // rescan simply won't find it there anymore.
    QString nextPath;
    const int count = m_folderModel->count();
    if (count > 1) {
        const int nextIndex = (m_folderModel->currentIndex() + 1) % count;
        nextPath = m_folderModel->data(m_folderModel->index(nextIndex, 0), FolderModel::FilePathRole).toString();
    }

    // Double-null-terminated, per SHFileOperationW's pFrom contract.
    std::wstring wpath = m_currentFilePath.toStdWString();
    wpath.push_back(L'\0');

    SHFILEOPSTRUCTW op{};
    op.wFunc = FO_DELETE;
    op.pFrom = wpath.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    if (SHFileOperationW(&op) != 0 || op.fAnyOperationsAborted)
        return false;

    m_savedRecipe = currentRecipe(); // the file the edits belonged to is gone: nothing to protect
    const QString deletedPath = m_currentFilePath;
    m_currentFilePath.clear(); // the pixels on screen no longer have a file: "Guardar" must not bring it back
    emit currentSourceChanged(); // file name, "Guardar" availability
    if (!nextPath.isEmpty()) {
        loadPath(nextPath);
        return true;
    }

    // No siblings left - rescan (the folder model will find itself empty,
    // since the deleted file was the only one) and clear the viewer.
    m_folderModel->openFolderForFile(deletedPath);
    m_originalImage = QImage();
    m_editStack.clear();
    m_liveAdjust = AdjustOp();
    refreshAdjustLut();
    m_liveFilterPreset.clear();
    m_liveFilterIntensity = 1.0;
    refreshLookLut();
    m_metadata.clear();
    markStructuralDirty();
    m_savedRecipe = currentRecipe();
    m_provider->setImage(QImage());
    m_currentImageSize = QSize();
    ++m_revision;
    m_currentSource = QStringLiteral("image://viewer/current?rev=%1").arg(m_revision);
    emit currentSourceChanged();
    emit currentImageSizeChanged();
    emit metadataChanged();
    emit editStackChanged();
    emit liveAdjustChanged();
    emit liveFilterChanged();
    emit histogramChanged();
    emit adjustedHistogramChanged();
    return true;
}

bool AppController::setAsWallpaper()
{
    const QImage baked = currentBaked();
    if (baked.isNull())
        return false;

    const QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QStringLiteral("/imageviewer_wallpaper.png");
    if (!baked.save(tempPath, "PNG"))
        return false;

    const std::wstring wpath = QDir::toNativeSeparators(tempPath).toStdWString();
    return SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, const_cast<wchar_t *>(wpath.c_str()),
                                  SPIF_UPDATEINIFILE | SPIF_SENDCHANGE) != 0;
}
