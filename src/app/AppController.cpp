#include "AppController.h"
#include "AdjustModel.h"
#include "LookModel.h"
#include "ImageProvider.h"
#include "FolderModel.h"
#include "ComicBook.h"
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
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QStandardPaths>
#include <QGuiApplication>
#include <QWindow>
#include <QClipboard>
#include <QMimeData>
#include <QRunnable>
#include <QThreadPool>

#ifdef _WIN32
// NOMINMAX before Windows.h - otherwise its min/max macros shadow std::min/
// std::max used throughout this file (and everywhere that includes
// AppController.h afterward).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shellapi.h>
#else
#include <QProcess>
#include <QStandardPaths>
#include "LinuxPortal.h"
#endif
#include "Translate.h"

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
    , m_cache([this]() { return m_editStack.bake(m_originalImage); })
    , m_effect([this]() { return structuralBaked(); })
{
    connect(&m_save, &SaveCoordinator::finished, this, &AppController::finishSave);
    connect(&m_animation, &AnimationPlayback::frameChanged, this, &AppController::showAnimationFrame);
    connect(&m_animation, &AnimationPlayback::playingChanged, this, &AppController::animationPlayingChanged);
    connect(&m_wallpaper, &DesktopWallpaper::finished, this, &AppController::wallpaperResult);
    m_liveCoalesce.setSingleShot(true);
    m_liveCoalesce.setInterval(kLiveCoalesceMs);
    connect(&m_effect, &EffectPreview::changed, this, &AppController::effectChanged);
    connect(&m_effect, &EffectPreview::valuesChanged, this, &AppController::effectValuesChanged);
    connect(&m_effect, &EffectPreview::busyChanged, this, &AppController::effectBusyChanged);
    connect(&m_effect, &EffectPreview::activeChanged, this, &AppController::editStackChanged); // hasEdits() counts a previewed effect
    connect(&m_effect, &EffectPreview::pictureReady, this, &AppController::publishPicture);
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
                    m_save.blockOnUnsaved(PendingAction::OpenPath, doc.filePath);
                    if (!m_currentFilePath.isEmpty())
                        m_folderModel->openFolderForFileAsync(m_currentFilePath);
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
    // The effect preview (m_effect) cancels and waits for its own workers.
    // The save coordinator (m_save) lets a save that is still writing finish.
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
    // A comic is read, not shown: the picture that is open stays as it is, edits included.
    if (core::ComicBook::isArchivePath(path)) {
        emit comicRequested(path);
        return;
    }

    // A save is still writing this picture's file: wait for it, then go (the picture is
    // clean by then unless it was edited meanwhile, and the usual question follows).
    if (m_save.saving()) {
        m_save.afterSave(PendingAction::OpenPath, path);
        return;
    }

    // Replacing the picture would throw away edits nobody has saved: do nothing yet
    // and let the UI ask (it calls resolveUnsaved() with the answer).
    if (isDirty() && !m_save.discardApproved()) {
        m_save.blockOnUnsaved(PendingAction::OpenPath, path);
        // The folder strip already moved to the requested file; put it back on the
        // one that is still showing.
        if (!m_currentFilePath.isEmpty())
            m_folderModel->openFolderForFileAsync(m_currentFilePath);
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
    m_folderModel->openFolderForFileAsync(path);
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
        qint64 budget = 600LL * 1024 * 1024;
#ifdef _WIN32
        MEMORYSTATUSEX mem{};
        mem.dwLength = sizeof(mem);
        if (GlobalMemoryStatusEx(&mem))
            budget = std::clamp<qint64>(qint64(mem.ullAvailPhys / 4), 256LL * 1024 * 1024, 1536LL * 1024 * 1024);
#else
        QFile info(QStringLiteral("/proc/meminfo"));
        if (info.open(QIODevice::ReadOnly)) {
            for (const QByteArray &line : info.readAll().split('\n')) {
                if (line.startsWith("MemAvailable:")) { // in kB
                    const qint64 kb = line.mid(13).trimmed().split(' ').value(0).toLongLong();
                    if (kb > 0)
                        budget = std::clamp<qint64>(kb * 1024 / 4, 256LL * 1024 * 1024, 1536LL * 1024 * 1024);
                    break;
                }
            }
        }
#endif
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
    m_lensHints = QVariantMap{{QStringLiteral("make"), doc.metadata.cameraMake},
                              {QStringLiteral("model"), doc.metadata.cameraModel},
                              {QStringLiteral("lens"), doc.metadata.lensModel},
                              {QStringLiteral("focal"), doc.metadata.focalLengthMm},
                              {QStringLiteral("aperture"), doc.metadata.apertureF},
                              {QStringLiteral("distance"), doc.metadata.subjectDistanceM}};
    m_isLoading = false;
    m_errorString.clear();

    m_animation.setAnimation(doc.animationFrames, doc.animationDelaysMs); // stops the old animation first

    // Bump the revision so the "image://viewer/current?rev=N" URL changes
    // and QML's Image actually re-requests the provider instead of serving
    // its own cached pixmap.
    ++m_revision;
    ++m_originalRevision;
    m_currentSource = QStringLiteral("image://viewer/current?rev=%1").arg(m_revision);

    // The size first: the canvas lays the picture out from it (not from the loaded texture, which may be a reduced copy).
    emit currentImageSizeChanged();
    emit currentSourceChanged();
    emit structuralImageChanged();
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

void AppController::showAnimationFrame(const QImage &frame)
{
    // Publishes the frame through ImageProvider exactly like republishBaked() does for an edit.
    m_provider->setImage(frame);
    ++m_revision;
    m_currentSource = QStringLiteral("image://viewer/current?rev=%1").arg(m_revision);
    emit currentSourceChanged();
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
        display.insert(core::tr("Cámara"), camera);

    if (meta.dateTaken.isValid())
        display.insert(core::tr("Fecha"), meta.dateTaken.toString(QStringLiteral("dd/MM/yyyy HH:mm")));

    if (meta.focalLengthMm > 0.0)
        display.insert(core::tr("Distancia focal"), QStringLiteral("%1 mm").arg(meta.focalLengthMm, 0, 'g', 3));

    if (meta.apertureF > 0.0)
        display.insert(core::tr("Apertura"), QStringLiteral("f/%1").arg(meta.apertureF, 0, 'g', 2));

    if (meta.exposureSeconds > 0.0) {
        const QString shutter = meta.exposureSeconds < 1.0
            ? QStringLiteral("1/%1 s").arg(qRound(1.0 / meta.exposureSeconds))
            : QStringLiteral("%1 s").arg(meta.exposureSeconds, 0, 'g', 3);
        display.insert(core::tr("Velocidad"), shutter);
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
        || m_effect.active();
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
    if (m_isLoading || m_save.saving() || m_currentFilePath.isEmpty())
        return false;
    return core::supportedSaveExtensions().contains(QFileInfo(m_currentFilePath).suffix().toLower());
}

bool AppController::saveBlocked()
{
    if (!m_isLoading && !m_save.saving())
        return false;
    m_errorString = m_isLoading
        ? core::tr("Espera a que termine de cargarse la otra imagen antes de guardar.")
        : core::tr("Ya se está guardando: espera a que termine.");
    emit errorStringChanged();
    return true;
}

QImage AppController::structuralBaked() const
{
    return m_cache.structural();
}

QVariantList AppController::histogram() const
{
    return m_cache.histogram();
}

QVariantMap AppController::histogramChannels() const
{
    return m_cache.histogramChannels();
}

QImage AppController::histogramProxy() const
{
    return m_cache.proxy();
}

QVariantMap AppController::adjustedHistogram() const
{
    return m_cache.adjustedHistogram(m_liveAdjust, m_liveFilterPreset, m_liveFilterIntensity);
}

QImage AppController::currentBaked() const
{
    if (m_originalImage.isNull())
        return {};

    QImage img = structuralBaked();
    // An effect that is only being previewed is part of what is on screen, so it is
    // part of what gets saved / copied - but it is NOT committed: the history is
    // not touched and the preview stays pending (the user can still cancel it).
    img = m_effect.applyTo(img);
    return core::edit::renderLiveOverlay(img, m_liveFilterPreset, m_liveFilterIntensity, m_liveAdjust);
}

void AppController::republishBaked(bool resetView)
{
    const QImage baked = structuralBaked();
    m_provider->setImage(baked);
    // A picture of another size (a stretch, a perspective correction) cannot keep the view of the old one.
    resetView = resetView || baked.size() != m_currentImageSize;
    m_currentImageSize = baked.size();
    ++m_revision;
    m_currentSource = QStringLiteral("image://viewer/current?rev=%1").arg(m_revision);
    emit currentImageSizeChanged();
    emit currentSourceChanged();
    if (resetView)
        emit structuralImageChanged();
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
    if (m_effect.active()) {
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
    m_cache.invalidateAdjusted();
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
    return AdjustModel::toMap(m_liveAdjust);
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
    m_cache.invalidateAdjusted();
    recordLiveChange(coalesceKey);
    // No CPU re-bake here: ImageCanvas.qml's Adjust/Detail ShaderEffects read
    // these straight off this signal and apply them live on the GPU.
    emit liveAdjustChanged();
    emit adjustedHistogramChanged();
    emit editStackChanged();
}

void AppController::applyAdjust(const std::optional<AdjustModel::Change> &change)
{
    if (change)
        setLiveAdjust(change->op, change->key);
}

void AppController::setAdjustParam(const QString &name, qreal value)
{
    if (m_originalImage.isNull())
        return;
    applyAdjust(AdjustModel::setParam(m_liveAdjust, name, value));
}

void AppController::setGrainType(int type)
{
    if (m_originalImage.isNull())
        return;
    applyAdjust(AdjustModel::setGrainType(m_liveAdjust, type));
}

void AppController::setGrainMono(bool mono)
{
    if (m_originalImage.isNull())
        return;
    applyAdjust(AdjustModel::setGrainMono(m_liveAdjust, mono));
}

void AppController::setNegative(bool on)
{
    if (m_originalImage.isNull())
        return;
    applyAdjust(AdjustModel::setNegative(m_liveAdjust, on));
}

void AppController::setLevels(int channel, qreal inBlack, qreal gamma, qreal inWhite)
{
    if (m_originalImage.isNull())
        return;
    applyAdjust(AdjustModel::setLevels(m_liveAdjust, channel, inBlack, gamma, inWhite));
}

void AppController::setOutputLevels(qreal black, qreal white)
{
    if (m_originalImage.isNull())
        return;
    applyAdjust(AdjustModel::setOutputLevels(m_liveAdjust, black, white));
}

void AppController::setCurvePoints(int channel, const QVariantList &points)
{
    if (m_originalImage.isNull())
        return;
    applyAdjust(AdjustModel::setCurvePoints(m_liveAdjust, channel, points));
}

QVariantList AppController::curveSamples(const QVariantList &points) const
{
    return AdjustModel::curveSamples(points);
}

void AppController::autoAdjust(const QString &kind)
{
    if (m_originalImage.isNull())
        return;
    const auto adjustKind = AdjustModel::autoKind(kind);
    if (!adjustKind)
        return;
    setLiveAdjust(core::edit::autoAdjusted(m_liveAdjust, structuralBaked(), *adjustKind));
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
    const AdjustModel::Change reset = AdjustModel::resetLevels(m_liveAdjust);
    setLiveAdjust(reset.op, reset.key);
}

void AppController::refreshLookLut()
{
    m_provider->setLookLut(core::edit::lutToImage(core::edit::buildAdjustLut(LookModel::tone(m_liveFilterPreset))));
    ++m_lookLutRevision;
}

QString AppController::lookLutSource() const
{
    return QStringLiteral("image://viewer/looklut?rev=%1").arg(m_lookLutRevision);
}

bool AppController::lookActive() const
{
    return LookModel::active(m_liveFilterPreset, m_liveFilterIntensity);
}

bool AppController::gradeActive() const
{
    return AdjustModel::gradeActive(m_liveAdjust);
}

QVariantMap AppController::lookMap() const
{
    return LookModel::toMap(m_liveFilterPreset, m_liveFilterIntensity);
}

QVariantList AppController::lookList() const
{
    return LookModel::list();
}

QVariantList AppController::lookGroups() const
{
    return LookModel::groups();
}

void AppController::ensureLookThumbnails()
{
    if (!m_cache.lookThumbsStale() || m_originalImage.isNull())
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
    m_cache.lookThumbsMade();
    ++m_lookThumbRevision;
    emit lookThumbsChanged();
}

void AppController::applyFilterPreset(const QString &presetId, qreal intensity)
{
    if (m_originalImage.isNull())
        return;
    const LookModel::Picked picked = LookModel::pick(presetId, intensity);
    m_liveFilterPreset = picked.id;
    m_liveFilterIntensity = picked.amount;
    refreshLookLut();
    m_cache.invalidateAdjusted();
    recordLiveChange(QStringLiteral("look"));
    // No CPU re-bake here either: ImageCanvas.qml's first Grade ShaderEffect
    // reads `look` and `lookLutSource` and applies the look live.
    emit liveFilterChanged();
    emit adjustedHistogramChanged();
    emit editStackChanged();
}

// --- Efectos ------------------------------------------------------------------

void AppController::publishPicture(const QImage &image)
{
    m_provider->setImage(image);
    ++m_revision;
    m_currentSource = QStringLiteral("image://viewer/current?rev=%1").arg(m_revision);
    // The picture is normally the same size, so the canvas must keep the user's zoom and position
    // (no structuralImageChanged). An effect that changes the size (stretch, perspective) is the
    // exception: the view is refitted to what is now on show.
    const bool resized = image.size() != m_currentImageSize && !image.isNull();
    if (resized)
        m_currentImageSize = image.size();
    if (resized)
        emit currentImageSizeChanged();
    emit currentSourceChanged();
    if (resized)
        emit structuralImageChanged();
}

void AppController::selectEffect(const QString &id)
{
    if (m_originalImage.isNull() || isAnimated())
        return;
    if (!core::edit::findEffect(id)) { // an empty or unknown id cancels
        cancelEffect();
        return;
    }
    m_effect.select(id);
}

void AppController::cancelEffect()
{
    if (!m_effect.active())
        return;
    if (m_effect.drop()) // a preview was on the canvas: bring the real picture back
        publishPicture(structuralBaked());
}

void AppController::commitEffect()
{
    commitPendingEffect(true);
}

void AppController::commitPendingEffect(bool republish)
{
    if (!m_effect.active() || m_originalImage.isNull())
        return;
    if (m_effect.mix() <= 0.0) { // nothing would change
        cancelEffect();
        return;
    }

    const core::edit::EffectOp op = m_effect.operation();
    // When the exact full-size result for these very settings is already
    // there, it IS what replaying the stack would produce - reuse it instead of
    // calculating it a second time.
    const bool exact = m_effect.exactReady();
    const QImage full = m_effect.exactResult();

    m_editStack.push(op);
    markStructuralDirty(); // also drops the preview state
    if (exact) {
        m_cache.setStructural(full);
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
            ? core::tr("Esta imagen no tiene archivo: usa «Guardar como…».")
            : core::tr("Este formato no se puede escribir en el mismo archivo: usa «Guardar como…».");
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
    if (m_effect.active())
        commitPendingEffect(false);

    // ---- the snapshot: everything the worker needs, taken now, immutable from here on (see SaveJob).
    SaveJob job;
    job.original = m_originalImage;
    job.ops = m_editStack.activePixelOps();
    job.structural = m_cache.structuralIfReady(); // null: the worker replays `ops`
    job.adjust = m_liveAdjust;
    job.lookId = m_liveFilterPreset;
    job.lookAmount = m_liveFilterIntensity;
    job.recipe = currentRecipe();
    job.serial = m_docSerial;
    job.path = path;

    // The file is written atomically (a failure leaves the original untouched),
    // JPEG/WebP at high quality, and the original's camera / GPS / date information
    // travels with the picture - see core::saveImageWithOptions(). The colour profile
    // does NOT come from the original: the pixels are sRGB, and the file is tagged so.
    if (!m_currentFilePath.isEmpty() && QFileInfo::exists(m_currentFilePath))
        job.options.metadataSource = m_currentFilePath;
    job.options.iccProfile = m_iccProfile; // empty (the normal case) = write an sRGB profile

    m_errorString.clear();
    m_save.start(std::move(job));
    emit errorStringChanged();
    emit isSavingChanged();
    return true;
}

void AppController::finishSave(const SaveOutcome &outcome)
{
    emit isSavingChanged(); // (m_save.saving() is already false)

    const PendingRequest afterRequest = m_save.takeAfterSave();
    const PendingAction after = afterRequest.action;
    const QString afterPath = afterRequest.path;

    if (!outcome.ok) {
        m_errorString = core::tr("No se pudo guardar «%1»: %2")
                            .arg(QDir::toNativeSeparators(outcome.path), outcome.error);
        emit errorStringChanged();
        emit saveFinished(false, outcome.path);
        // Whatever was waiting on this save is dropped; the filmstrip may already have
        // moved to the picture that was asked for - put it back on the open one.
        if (after != PendingAction::None && !m_currentFilePath.isEmpty())
            m_folderModel->openFolderForFileAsync(m_currentFilePath);
        return;
    }

    // What counts as saved is what the snapshot held - NOT whatever is on screen now: an
    // edit made while the file was being written is still unsaved.
    if (outcome.serial == m_docSerial) {
        m_savedRecipe = outcome.recipe;
        // The worker had to calculate the structural picture (an effect committed by the
        // save, say): keep it, if the document still wants exactly that.
        if (!outcome.structural.isNull() && !m_cache.structuralIsReady() && m_editStack.activePixelOps() == outcome.ops) {
            m_cache.setStructural(outcome.structural);
            republishBaked(false);
        }
        emit editStackChanged();
    }
    if (!outcome.warning.isEmpty()) {
        qWarning().noquote() << "Guardado con aviso:" << outcome.warning;
        emit saveNotice(core::tr("La imagen se guardó, pero: %1.").arg(outcome.warning));
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
    const PendingRequest request = m_save.takeBlocked();
    const PendingAction action = request.action;
    const QString path = request.path;
    if (action == PendingAction::None || choice == QLatin1String("cancel"))
        return; // (loadPath already put the folder strip back on the open picture)

    if (choice == QLatin1String("save")) {
        // Save first; the picture that was asked for is opened when the file is on disk.
        m_save.afterSave(action, path);
        if (!saveEdited()) // refused: stay, with the error on screen
            m_save.takeAfterSave();
        return;
    }

    m_save.setDiscardApproved(true);
    if (action == PendingAction::OpenPath)
        loadPath(path);
    else
        pasteFromClipboard();
    m_save.setDiscardApproved(false);
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
        if (m_save.saving()) { // see loadPath()
            m_save.afterSave(PendingAction::Paste);
            return false;
        }
        if (isDirty() && !m_save.discardApproved()) {
            m_save.blockOnUnsaved(PendingAction::Paste);
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
    if (m_save.saving()) { // the writer is about to (re)create this very file
        m_errorString = core::tr("No se puede eliminar el archivo mientras se guarda.");
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

#ifdef _WIN32
    // Double-null-terminated, per SHFileOperationW's pFrom contract.
    std::wstring wpath = m_currentFilePath.toStdWString();
    wpath.push_back(L'\0');

    SHFILEOPSTRUCTW op{};
    op.wFunc = FO_DELETE;
    op.pFrom = wpath.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    if (SHFileOperationW(&op) != 0 || op.fAnyOperationsAborted)
        return false;
#else
    // The desktop's trash (freedesktop.org Trash specification); never a permanent delete. A sandbox has a trash of
    // its own that nobody empties, so inside Flatpak the file goes to the real one through the Trash portal.
    if (LinuxPortal::inFlatpak()) {
        if (!LinuxPortal::trashFile(m_currentFilePath))
            return false;
    } else if (!QFile::moveToTrash(m_currentFilePath) && !LinuxPortal::trashFile(m_currentFilePath)) {
        return false;
    }
#endif

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
    m_folderModel->openFolderForFileAsync(deletedPath);
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
    emit currentImageSizeChanged();
    emit currentSourceChanged();
    emit metadataChanged();
    emit editStackChanged();
    emit liveAdjustChanged();
    emit liveFilterChanged();
    emit histogramChanged();
    emit adjustedHistogramChanged();
    return true;
}

void AppController::setAsWallpaper()
{
    const QImage baked = currentBaked();
    if (!baked.isNull())
        m_wallpaper.set(baked);
}
