#pragma once

#include "anim/Anim.h"

#include <QAbstractListModel>
#include <QHash>
#include <QImage>
#include <QList>
#include <QMutex>
#include <QQuickImageProvider>
#include <QString>
#include <QThreadPool>
#include <QUrl>
#include <QVariantMap>
#include <atomic>
#include <memory>
#include <vector>

class PaneImageStore;

// The animation studio ("Crear > GIF animado"): a list of pictures, how long each stays and how they are
// fitted, joined and played, and the writing of the result as a GIF, an APNG or an animated WebP.
//
// It is the list model of the pictures (roles below) and holds all the options in one map, so the page does
// not need a property per control: read them from `options`, change one with setOption().
class AnimStudio : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QVariantMap options READ options NOTIFY optionsChanged)
    Q_PROPERTY(int count READ count NOTIFY planChanged)
    // How many frames the finished animation has (pictures plus the frames of the transitions) and how long it
    // lasts; both follow the options.
    Q_PROPERTY(int planCount READ planCount NOTIFY planChanged)
    Q_PROPERTY(int totalMs READ totalMs NOTIFY planChanged)
    // Bumped whenever a preview frame would look different: put it in the preview URLs.
    Q_PROPERTY(int revision READ revision NOTIFY planChanged)
    Q_PROPERTY(bool exporting READ exporting NOTIFY exportingChanged)
    // How much the file would weigh with the current pictures and options (-1: not known yet, or out of date),
    // found by writing it into memory in the background (estimateSize()).
    Q_PROPERTY(qint64 estimatedBytes READ estimatedBytes NOTIFY estimateChanged)
    Q_PROPERTY(bool estimating READ estimating NOTIFY estimateChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)

public:
    enum Roles { IdRole = Qt::UserRole + 1, PathRole, NameRole, HoldRole, FromAnimationRole, StyledRole };

    // What one picture of the list is: a file, or (for the frames of an animation that was taken apart) a picture
    // that lives in memory.
    struct Entry {
        int id = 0;
        QString path;     // the file (for a frame taken from an animation: the animation's)
        QImage memory;    // set when the picture is not a file of its own
        QString label;
        int holdMs = 1000;
        core::anim::FrameStyle style; // effect and text of this picture
    };

    explicit AnimStudio(PaneImageStore *store, QObject *parent = nullptr);
    ~AnimStudio() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QVariantMap options() const;
    int count() const { return int(m_entries.size()); }
    Q_INVOKABLE int holdAt(int index) const { return index >= 0 && index < count() ? m_entries[size_t(index)].holdMs : 1000; }
    int planCount() const;
    int totalMs() const;
    int revision() const { return m_revision; }
    bool exporting() const { return m_exporting; }
    qint64 estimatedBytes() const { return m_estimatedBytes; }
    bool estimating() const { return m_estimating; }
    qreal progress() const { return m_progress; }

    // The pictures. Files that are animations (GIF, APNG, animated WebP) are taken apart into their frames.
    Q_INVOKABLE int addPaths(const QStringList &paths);
    Q_INVOKABLE int addUrls(const QVariantList &urls);
    // Pictures that live in memory (the frames read from a video): each stays `holdMs`. The first ones set the size
    // of the animation (the size of the pictures themselves).
    int addFrames(const QList<QImage> &frames, int holdMs, const QString &label);
    Q_INVOKABLE void removeAt(int index);
    Q_INVOKABLE void duplicateAt(int index);
    Q_INVOKABLE void move(int from, int to);
    Q_INVOKABLE void reverseOrder();
    Q_INVOKABLE void clear();
    Q_INVOKABLE void setHold(int index, int ms);
    Q_INVOKABLE void setAllHold(int ms);

    // The look of one picture: an effect of the Efectos catalogue and a text over it. Keys: effectId, effectPreset
    // (-1 = defaults), effectMix (0..100), text, family, textX, textY (0..100, % of the canvas), textSize (% of the short
    // side), bold, color, outline, outlineColor (0xRRGGBB).
    Q_INVOKABLE QVariantMap styleOf(int index) const;
    Q_INVOKABLE void setStyleValue(int index, const QString &key, const QVariant &value);
    Q_INVOKABLE void copyStyleToAll(int index);
    Q_INVOKABLE void clearStyle(int index);
    // The same, for every picture at once (the studio applies effect and text to the whole animation).
    Q_INVOKABLE void setStyleValueAll(const QString &key, const QVariant &value);
    Q_INVOKABLE void clearStyleAll();
    // The effects a picture can be given: [{id, name, group, presets: [name...]}].
    Q_INVOKABLE QVariantList frameEffects() const;
    Q_INVOKABLE QStringList fontFamilies() const;

    // One option: width, height, lockAspect, fit, background (0xRRGGBB), transparent, transition,
    // transitionMs, transitionSteps, transitionOnLoop, reverse, pingPong, speed, loops, format (0 GIF, 1 APNG,
    // 2 WebP), gifColors, gifDither, gifLocal, webpLossless, webpQuality, defaultHold, skip (use one picture of every `skip`).
    Q_INVOKABLE void setOption(const QString &key, const QVariant &value);
    // The size of the first picture, as the starting size (and, when the aspect is locked, the proportions).
    Q_INVOKABLE void sizeFromFirst(int longSide);
    // The crop of the whole animation: the part of every picture that is kept, as fractions of the picture (x, y, w, h).
    // The proportions of the animation follow it. (1, 1) of width and height = no crop.
    Q_INVOKABLE void setCrop(qreal x, qreal y, qreal w, qreal h);
    // The first picture, as it is (for choosing the crop on it): its entry id (-1 when there is none) and its size.
    Q_INVOKABLE int firstEntryId() const { return m_entries.empty() ? -1 : m_entries.front().id; }
    Q_INVOKABLE QSize firstSize() const { return m_firstSize; }

    // The playback: how long frame `planIndex` of the finished animation stays.
    Q_INVOKABLE int planDelay(int planIndex) const;
    // Where picture `entryIndex` is held still in the finished animation (-1 when it does not appear).
    Q_INVOKABLE int planIndexOf(int entryIndex) const;

    // Writes the animation (in the background); progress, exportFinished.
    Q_INVOKABLE bool exportTo(const QUrl &file);
    Q_INVOKABLE void cancelExport();
    // Writes the animation into memory in the background to learn its weight (estimatedBytes); a change of anything
    // makes the last answer out of date again.
    Q_INVOKABLE void estimateSize();

    // For the image providers (any thread).
    QImage previewFrame(int planIndex) const;
    QImage thumbnail(int entryId, int maxSide) const;

signals:
    void optionsChanged();
    void planChanged();
    void exportingChanged();
    void estimateChanged();
    void progressChanged();
    void exportFinished(bool ok, const QString &path, qint64 bytes, const QString &error);

private:
    // Everything the background parts need, frozen: replaced (not edited) on every change.
    struct Snapshot {
        std::vector<Entry> entries;
        // The pictures the animation is made of: all of them, or one of every `skip` (each takes the time of those it
        // stands for, so the animation lasts the same).
        std::vector<Entry> used;
        int skip = 1;
        QRectF crop = QRectF(0, 0, 1, 1);
        core::anim::Settings settings;
        std::vector<core::anim::PlanStep> plan;
        core::anim::GifOptions gif;
        core::anim::ApngOptions apng;
        core::anim::WebpOptions webp;
        int format = 0;
        int revision = 0;
    };
    friend class StudioFrameSource;

    void changed(bool planAffected = true);
    void applyFirstSize(int longSide);
    void rebuildSnapshot();
    std::shared_ptr<const Snapshot> snapshot() const;
    Snapshot makeSnapshot() const;
    static QImage loadEntry(PaneImageStore *store, const Entry &entry, int maxSide);
    // Sets one key of a picture's look; false when the key is unknown or nothing changed.
    static bool applyStyleKey(core::anim::FrameStyle &style, const QString &key, const QVariant &value);

    PaneImageStore *m_store;
    std::vector<Entry> m_entries;
    int m_nextId = 1;
    QVariantMap m_options;
    bool m_sizeIsAutomatic = true;
    double m_aspect = 4.0 / 3.0; // width / height of the first picture, cut to the crop
    QSize m_firstSize;           // the first picture, as it is
    void refreshAspect();
    int m_revision = 0;

    mutable QMutex m_mutex; // guards m_snapshot
    std::shared_ptr<const Snapshot> m_snapshot;

    // the pictures fitted to the preview canvas, so that stepping through a transition does not fit them again
    mutable QMutex m_previewMutex;
    mutable QHash<QString, QImage> m_previewFitted;
    mutable QList<QString> m_previewOrder;

    bool m_exporting = false;
    qreal m_progress = 0.0;
    std::shared_ptr<std::atomic<bool>> m_cancel;
    QThreadPool m_pool; // the export worker holds `this`: its own pool, waited for in the destructor

    qint64 m_estimatedBytes = -1;
    bool m_estimating = false;
    int m_estimateGeneration = 0;
    std::shared_ptr<std::atomic<bool>> m_estimateCancel;
    QThreadPool m_estimatePool; // one at a time, so an estimate never slows the export down for long
};

// "image://animprev/<plan index>?r=<revision>": a frame of the animation as the preview shows it.
class AnimPreviewProvider : public QQuickImageProvider {
public:
    explicit AnimPreviewProvider(AnimStudio *studio) : QQuickImageProvider(QQuickImageProvider::Image), m_studio(studio) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    AnimStudio *m_studio;
};

// "image://animsrc/<entry id>?r=...": a small picture of one entry, for the film strip.
class AnimThumbnailProvider : public QQuickImageProvider {
public:
    explicit AnimThumbnailProvider(AnimStudio *studio) : QQuickImageProvider(QQuickImageProvider::Image), m_studio(studio) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    AnimStudio *m_studio;
};
