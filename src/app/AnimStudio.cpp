#include "AnimStudio.h"
#include "PaneImageProvider.h"
#include "decoders/AnimatedDecoder.h"

#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QMutexLocker>
#include <QRunnable>
#include <QSaveFile>
#include <QThreadPool>
#include <QElapsedTimer>
#include <algorithm>
#include <cmath>

using namespace core::anim;

namespace {

constexpr int kMaxEntries = 400;
constexpr int kPreviewLongSide = 480;
constexpr int kPreviewCacheSize = 24;

QVariantMap defaultOptions()
{
    QVariantMap m;
    m.insert("width", 640);
    m.insert("height", 480);
    m.insert("lockAspect", true);
    m.insert("fit", 0);
    m.insert("background", 0x000000);
    m.insert("transparent", false);
    m.insert("transition", 0);
    m.insert("transitionMs", 400);
    m.insert("transitionSteps", 6);
    m.insert("transitionOnLoop", true);
    m.insert("reverse", false);
    m.insert("pingPong", false);
    m.insert("speed", 1.0);
    m.insert("loops", 0);
    m.insert("format", 0);
    m.insert("gifColors", 256);
    m.insert("gifDither", true);
    m.insert("gifLocal", false);
    m.insert("webpLossless", false);
    m.insert("webpQuality", 80);
    m.insert("defaultHold", 1000);
    return m;
}

QString entryName(const QString &path) { return QFileInfo(path).fileName(); }

} // namespace

// ---- the frames of an export, made one at a time ----------------------------------------------------------------

class StudioFrameSource : public FrameSource {
public:
    StudioFrameSource(std::shared_ptr<const AnimStudio::Snapshot> snap, PaneImageStore *store)
        : m_snap(std::move(snap)), m_store(store)
    {
    }
    int count() const override { return int(m_snap->plan.size()); }
    QSize size() const override { return m_snap->settings.size; }
    QImage frame(int i, int *delayMs) override
    {
        const PlanStep &step = m_snap->plan[size_t(i)];
        if (delayMs)
            *delayMs = step.delayMs;
        const QImage a = fitted(step.a);
        const QImage b = step.isStill() ? a : fitted(step.b);
        return renderStep(a, b, step, m_snap->settings);
    }

private:
    QImage fitted(int index)
    {
        for (const auto &c : m_cache)
            if (c.first == index)
                return c.second;
        const Settings &s = m_snap->settings;
        const int side = std::max(s.size.width(), s.size.height()) * (s.fit == Fit::Cover ? 3 : 2);
        const QImage source = AnimStudio::loadEntry(m_store, m_snap->entries[size_t(index)], side);
        QImage out = fitToCanvas(source, s);
        m_cache.push_back({index, out});
        if (m_cache.size() > 3)
            m_cache.erase(m_cache.begin());
        return out;
    }

    std::shared_ptr<const AnimStudio::Snapshot> m_snap;
    PaneImageStore *m_store;
    std::vector<std::pair<int, QImage>> m_cache;
};

// ---- the model ------------------------------------------------------------------------------------------------------

AnimStudio::AnimStudio(PaneImageStore *store, QObject *parent) : QAbstractListModel(parent), m_store(store), m_options(defaultOptions())
{
    rebuildSnapshot();
}

AnimStudio::~AnimStudio()
{
    if (m_cancel)
        m_cancel->store(true);
    QThreadPool::globalInstance()->waitForDone(3000); // an export still running reports back to this object
}

int AnimStudio::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : int(m_entries.size()); }

QVariant AnimStudio::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= int(m_entries.size()))
        return {};
    const Entry &e = m_entries[size_t(index.row())];
    switch (role) {
    case IdRole: return e.id;
    case PathRole: return e.path;
    case NameRole: return e.label;
    case HoldRole: return e.holdMs;
    case FromAnimationRole: return !e.memory.isNull();
    default: return {};
    }
}

QHash<int, QByteArray> AnimStudio::roleNames() const
{
    return {{IdRole, "entryId"}, {PathRole, "path"}, {NameRole, "name"}, {HoldRole, "holdMs"}, {FromAnimationRole, "fromAnimation"}};
}

QVariantMap AnimStudio::options() const { return m_options; }

std::shared_ptr<const AnimStudio::Snapshot> AnimStudio::snapshot() const
{
    QMutexLocker lock(&m_mutex);
    return m_snapshot;
}

AnimStudio::Snapshot AnimStudio::makeSnapshot() const
{
    Snapshot s;
    s.entries = m_entries;
    const QVariantMap &o = m_options;
    Settings &st = s.settings;
    st.size = QSize(o.value("width").toInt(), o.value("height").toInt());
    st.fit = Fit(std::clamp(o.value("fit").toInt(), 0, 2));
    st.background = QColor::fromRgb(QRgb(o.value("background").toUInt() & 0xFFFFFF));
    st.transparentBackground = o.value("transparent").toBool();
    st.transition = Transition(std::clamp(o.value("transition").toInt(), 0, 6));
    st.transitionMs = o.value("transitionMs").toInt();
    st.transitionSteps = o.value("transitionSteps").toInt();
    st.transitionOnLoop = o.value("transitionOnLoop").toBool();
    st.reverse = o.value("reverse").toBool();
    st.pingPong = o.value("pingPong").toBool();
    st.speed = o.value("speed").toDouble();
    const int loops = o.value("loops").toInt();
    s.gif.colors = o.value("gifColors").toInt();
    s.gif.dither = o.value("gifDither").toBool();
    s.gif.localPalettes = o.value("gifLocal").toBool();
    s.gif.loops = loops;
    s.apng.loops = loops;
    s.webp.lossless = o.value("webpLossless").toBool();
    s.webp.quality = o.value("webpQuality").toInt();
    s.webp.loops = loops;
    s.format = o.value("format").toInt();
    std::vector<int> holds;
    for (const Entry &e : m_entries)
        holds.push_back(e.holdMs);
    s.plan = buildPlan(holds, st);
    s.revision = m_revision;
    return s;
}

void AnimStudio::rebuildSnapshot()
{
    auto snap = std::make_shared<const Snapshot>(makeSnapshot());
    QMutexLocker lock(&m_mutex);
    m_snapshot = std::move(snap);
}

void AnimStudio::changed(bool planAffected)
{
    ++m_revision;
    rebuildSnapshot();
    emit optionsChanged();
    if (planAffected)
        emit planChanged();
}

int AnimStudio::planCount() const { return int(snapshot()->plan.size()); }

int AnimStudio::totalMs() const
{
    int total = 0;
    for (const PlanStep &s : snapshot()->plan)
        total += s.delayMs;
    return total;
}

int AnimStudio::planDelay(int planIndex) const
{
    const auto snap = snapshot();
    return planIndex >= 0 && size_t(planIndex) < snap->plan.size() ? snap->plan[size_t(planIndex)].delayMs : 100;
}

int AnimStudio::planIndexOf(int entryIndex) const
{
    const auto snap = snapshot();
    for (size_t i = 0; i < snap->plan.size(); ++i)
        if (snap->plan[i].a == entryIndex && snap->plan[i].isStill())
            return int(i);
    return -1;
}

// ---- the pictures --------------------------------------------------------------------------------------------------

int AnimStudio::addPaths(const QStringList &paths)
{
    int added = 0;
    const bool wasEmpty = m_entries.empty();
    const int hold = m_options.value("defaultHold").toInt();
    for (const QString &path : paths) {
        if (int(m_entries.size()) >= kMaxEntries)
            break;
        const QFileInfo info(path);
        if (!info.isFile())
            continue;

        core::AnimatedDecoder animated;
        if (animated.canDecode(path)) {
            const core::DecodeResult r = animated.decode(path);
            if (r.ok && r.frames.size() > 1) {
                beginInsertRows({}, int(m_entries.size()), int(std::min<size_t>(m_entries.size() + size_t(r.frames.size()), kMaxEntries)) - 1);
                for (int i = 0; i < r.frames.size() && int(m_entries.size()) < kMaxEntries; ++i) {
                    Entry e;
                    e.id = m_nextId++;
                    e.path = path;
                    e.memory = r.frames[i].convertToFormat(QImage::Format_RGBA8888);
                    e.label = QStringLiteral("%1 · %2").arg(entryName(path)).arg(i + 1);
                    e.holdMs = i < r.frameDelaysMs.size() ? std::max(20, r.frameDelaysMs[i]) : hold;
                    m_entries.push_back(std::move(e));
                    ++added;
                }
                endInsertRows();
                continue;
            }
        }
        Entry e;
        e.id = m_nextId++;
        e.path = path;
        e.label = entryName(path);
        e.holdMs = hold;
        beginInsertRows({}, int(m_entries.size()), int(m_entries.size()));
        m_entries.push_back(std::move(e));
        endInsertRows();
        ++added;
    }
    if (added == 0)
        return 0;
    if (wasEmpty) {
        // the first picture sets the proportions, and (until the size is chosen by hand) the size
        const Entry &first = m_entries.front();
        QSize s = first.memory.isNull() ? QSize() : first.memory.size();
        if (!s.isValid()) {
            m_store->image(first.path, 64); // decodes it, which makes its real size known
            s = m_store->sourceSize(first.path);
        }
        if (s.isValid() && s.height() > 0)
            m_aspect = double(s.width()) / s.height();
        if (m_sizeIsAutomatic)
            applyFirstSize(640);
    }
    changed();
    return added;
}

int AnimStudio::addUrls(const QVariantList &urls)
{
    QStringList paths;
    for (const QVariant &u : urls) {
        const QUrl url = u.toUrl();
        paths << (url.isLocalFile() ? url.toLocalFile() : u.toString());
    }
    return addPaths(paths);
}

void AnimStudio::removeAt(int index)
{
    if (index < 0 || index >= int(m_entries.size()))
        return;
    beginRemoveRows({}, index, index);
    m_entries.erase(m_entries.begin() + index);
    endRemoveRows();
    changed();
}

void AnimStudio::duplicateAt(int index)
{
    if (index < 0 || index >= int(m_entries.size()) || int(m_entries.size()) >= kMaxEntries)
        return;
    Entry copy = m_entries[size_t(index)];
    copy.id = m_nextId++;
    beginInsertRows({}, index + 1, index + 1);
    m_entries.insert(m_entries.begin() + index + 1, std::move(copy));
    endInsertRows();
    changed();
}

void AnimStudio::move(int from, int to)
{
    const int n = int(m_entries.size());
    if (from < 0 || from >= n || to < 0 || to >= n || from == to)
        return;
    beginMoveRows({}, from, from, {}, to > from ? to + 1 : to);
    Entry e = std::move(m_entries[size_t(from)]);
    m_entries.erase(m_entries.begin() + from);
    m_entries.insert(m_entries.begin() + to, std::move(e));
    endMoveRows();
    changed();
}

void AnimStudio::reverseOrder()
{
    if (m_entries.size() < 2)
        return;
    beginResetModel();
    std::reverse(m_entries.begin(), m_entries.end());
    endResetModel();
    changed();
}

void AnimStudio::clear()
{
    if (m_entries.empty())
        return;
    beginResetModel();
    m_entries.clear();
    endResetModel();
    m_sizeIsAutomatic = true;
    changed();
}

void AnimStudio::setHold(int index, int ms)
{
    if (index < 0 || index >= int(m_entries.size()))
        return;
    ms = std::clamp(ms, 20, 60000);
    if (m_entries[size_t(index)].holdMs == ms)
        return;
    m_entries[size_t(index)].holdMs = ms;
    emit dataChanged(this->index(index), this->index(index), {HoldRole});
    changed();
}

void AnimStudio::setAllHold(int ms)
{
    if (m_entries.empty())
        return;
    ms = std::clamp(ms, 20, 60000);
    for (Entry &e : m_entries)
        e.holdMs = ms;
    emit dataChanged(index(0), index(int(m_entries.size()) - 1), {HoldRole});
    changed();
}

// ---- the options -----------------------------------------------------------------------------------------------------

void AnimStudio::setOption(const QString &key, const QVariant &value)
{
    if (!m_options.contains(key))
        return;
    QVariant v = value;
    auto clampInt = [&](int lo, int hi) { v = std::clamp(value.toInt(), lo, hi); };
    if (key == "width" || key == "height") {
        clampInt(16, 4096);
        m_sizeIsAutomatic = false;
    } else if (key == "fit") clampInt(0, 2);
    else if (key == "transition") clampInt(0, 6);
    else if (key == "transitionMs") clampInt(60, 5000);
    else if (key == "transitionSteps") clampInt(1, 30);
    else if (key == "loops") clampInt(0, 100);
    else if (key == "format") clampInt(0, 2);
    else if (key == "gifColors") clampInt(2, 256);
    else if (key == "webpQuality") clampInt(0, 100);
    else if (key == "defaultHold") clampInt(20, 60000);
    else if (key == "background") v = int(value.toUInt() & 0xFFFFFF);
    else if (key == "speed") v = std::clamp(value.toDouble(), 0.1, 10.0);
    else if (value.typeId() == QMetaType::Bool || m_options.value(key).typeId() == QMetaType::Bool) v = value.toBool();

    if (m_options.value(key) == v)
        return;
    m_options.insert(key, v);

    if (m_options.value("lockAspect").toBool() && (key == "width" || key == "lockAspect")) {
        m_options.insert("height", std::clamp(int(std::lround(m_options.value("width").toInt() / m_aspect)), 16, 4096));
    } else if (m_options.value("lockAspect").toBool() && key == "height") {
        m_options.insert("width", std::clamp(int(std::lround(m_options.value("height").toInt() * m_aspect)), 16, 4096));
    }
    changed();
}

void AnimStudio::sizeFromFirst(int longSide)
{
    applyFirstSize(longSide);
    m_sizeIsAutomatic = false;
    changed();
}

void AnimStudio::applyFirstSize(int longSide)
{
    longSide = std::clamp(longSide, 16, 4096);
    int w, h;
    if (m_aspect >= 1.0) {
        w = longSide;
        h = std::max(16, int(std::lround(longSide / m_aspect)));
    } else {
        h = longSide;
        w = std::max(16, int(std::lround(longSide * m_aspect)));
    }
    m_options.insert("width", w);
    m_options.insert("height", h);
}

// ---- pictures for the preview and the film strip ----------------------------------------------------------------

QImage AnimStudio::loadEntry(PaneImageStore *store, const Entry &entry, int maxSide)
{
    if (!entry.memory.isNull())
        return entry.memory;
    return store->image(entry.path, maxSide);
}

QImage AnimStudio::previewFrame(int planIndex) const
{
    const auto snap = snapshot();
    if (planIndex < 0 || size_t(planIndex) >= snap->plan.size())
        return {};
    // the preview canvas: the same proportions, at most kPreviewLongSide along the long side
    Settings ps = snap->settings;
    const double k = std::min(1.0, double(kPreviewLongSide) / std::max(ps.size.width(), ps.size.height()));
    ps.size = QSize(std::max(8, int(std::lround(ps.size.width() * k))), std::max(8, int(std::lround(ps.size.height() * k))));

    auto fitted = [&](int entryIndex) -> QImage {
        const Entry &e = snap->entries[size_t(entryIndex)];
        const QString key = QStringLiteral("%1|%2x%3|%4|%5|%6").arg(e.id).arg(ps.size.width()).arg(ps.size.height()).arg(int(ps.fit))
                                .arg(ps.background.rgba(), 0, 16).arg(ps.transparentBackground);
        {
            QMutexLocker lock(&m_previewMutex);
            const auto it = m_previewFitted.constFind(key);
            if (it != m_previewFitted.constEnd())
                return it.value();
        }
        const int side = std::max(ps.size.width(), ps.size.height()) * 2;
        const QImage out = fitToCanvas(loadEntry(m_store, e, side), ps);
        QMutexLocker lock(&m_previewMutex);
        m_previewFitted.insert(key, out);
        m_previewOrder.append(key);
        while (m_previewOrder.size() > kPreviewCacheSize)
            m_previewFitted.remove(m_previewOrder.takeFirst());
        return out;
    };

    const PlanStep &step = snap->plan[size_t(planIndex)];
    const QImage a = fitted(step.a);
    const QImage b = step.isStill() ? a : fitted(step.b);
    return renderStep(a, b, step, ps);
}

QImage AnimStudio::thumbnail(int entryId, int maxSide) const
{
    const auto snap = snapshot();
    for (const Entry &e : snap->entries) {
        if (e.id != entryId)
            continue;
        const QImage full = loadEntry(m_store, e, maxSide * 2);
        if (full.isNull())
            return {};
        return full.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return {};
}

// ---- writing the file ---------------------------------------------------------------------------------------------

bool AnimStudio::exportTo(const QUrl &file)
{
    if (m_exporting)
        return false;
    const auto snap = snapshot();
    if (snap->plan.empty())
        return false;
    QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    if (path.isEmpty())
        return false;
    static const char *const suffixes[] = {"gif", "png", "webp"};
    const QString wanted = QString::fromLatin1(suffixes[std::clamp(snap->format, 0, 2)]);
    if (QFileInfo(path).suffix().compare(wanted, Qt::CaseInsensitive) != 0)
        path += QLatin1Char('.') + wanted;

    m_exporting = true;
    m_progress = 0.0;
    m_cancel = std::make_shared<std::atomic<bool>>(false);
    emit exportingChanged();
    emit progressChanged();

    const auto cancel = m_cancel;
    PaneImageStore *store = m_store;
    QThreadPool::globalInstance()->start(QRunnable::create([this, snap, path, cancel, store]() {
        StudioFrameSource source(snap, store);
        QSaveFile out(path);
        QString error;
        bool ok = out.open(QIODevice::WriteOnly);
        if (!ok)
            error = QStringLiteral("No se pudo crear el archivo.");
        QElapsedTimer sinceReport;
        sinceReport.start();
        qreal lastReported = -1;
        auto progress = [&](int done, int total) {
            const qreal p = total > 0 ? qreal(done) / total : 1.0;
            if (p - lastReported >= 0.01 || sinceReport.elapsed() > 100 || done == total) {
                lastReported = p;
                sinceReport.restart();
                QMetaObject::invokeMethod(this, [this, p]() { m_progress = p; emit progressChanged(); }, Qt::QueuedConnection);
            }
            return !cancel->load();
        };
        if (ok) {
            switch (snap->format) {
            case 1: ok = writeApng(source, snap->apng, out, progress, &error); break;
            case 2: ok = writeWebp(source, snap->webp, out, progress, &error); break;
            default: ok = writeGif(source, snap->gif, out, progress, &error); break;
            }
        }
        const bool cancelled = cancel->load();
        qint64 bytes = 0;
        if (ok && !cancelled) {
            ok = out.commit();
            if (!ok)
                error = QStringLiteral("No se pudo guardar el archivo.");
            else
                bytes = QFileInfo(path).size();
        } else {
            out.cancelWriting();
            ok = false;
            if (cancelled)
                error = QStringLiteral("Cancelado.");
        }
        QMetaObject::invokeMethod(this, [this, ok, path, bytes, error]() {
            m_exporting = false;
            m_progress = ok ? 1.0 : 0.0;
            emit exportingChanged();
            emit progressChanged();
            emit exportFinished(ok, path, bytes, error);
        }, Qt::QueuedConnection);
    }));
    return true;
}

void AnimStudio::cancelExport()
{
    if (m_cancel)
        m_cancel->store(true);
}

// ---- providers ---------------------------------------------------------------------------------------------------------

QImage AnimPreviewProvider::requestImage(const QString &id, QSize *size, const QSize &)
{
    const int index = id.left(id.indexOf(QLatin1Char('?')) < 0 ? id.size() : id.indexOf(QLatin1Char('?'))).toInt();
    const QImage image = m_studio->previewFrame(index);
    if (size)
        *size = image.size();
    return image;
}

QImage AnimThumbnailProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const int entry = id.left(id.indexOf(QLatin1Char('?')) < 0 ? id.size() : id.indexOf(QLatin1Char('?'))).toInt();
    const int side = requestedSize.isValid() ? std::max(requestedSize.width(), requestedSize.height()) : 200;
    const QImage image = m_studio->thumbnail(entry, side);
    if (size)
        *size = image.size();
    return image;
}
