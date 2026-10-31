#include "AnimStudio.h"
#include "PaneImageProvider.h"
#include "edit/Effects.h"
#include "decoders/AnimatedDecoder.h"

#include <QBuffer>
#include <QDir>
#include <QFontDatabase>
#include <QFileInfo>
#include <QMetaObject>
#include <QMutexLocker>
#include <QRunnable>
#include <QSaveFile>
#include <QThreadPool>
#include <QElapsedTimer>
#include <algorithm>
#include <cmath>
#include "Translate.h"

using namespace core::anim;

namespace {

constexpr int kMaxEntries = 600; // (a video of 60 s at 10 fps)
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
    m.insert("gifCompression", 0);
    m.insert("gifColors", 256);
    m.insert("gifDither", true);
    m.insert("gifLocal", false);
    m.insert("webpLossless", false);
    m.insert("webpQuality", 80);
    m.insert("defaultHold", 100); // (a video or an animation brings its own)
    m.insert("skip", 1);          // one picture of every `skip`
    m.insert("cropX", 0.0);
    m.insert("cropY", 0.0);
    m.insert("cropW", 1.0);
    m.insert("cropH", 1.0);
    return m;
}

// `source` cut to the crop (fractions of the picture).
QImage cropped(const QImage &source, const QRectF &crop)
{
    if (source.isNull() || (crop.x() <= 0.0 && crop.y() <= 0.0 && crop.width() >= 0.999 && crop.height() >= 0.999))
        return source;
    const QRect r(qRound(crop.x() * source.width()), qRound(crop.y() * source.height()), std::max(1, qRound(crop.width() * source.width())),
                  std::max(1, qRound(crop.height() * source.height())));
    return source.copy(r.intersected(source.rect()));
}

// How much bigger a picture has to be read for what is left of it after the crop to be as sharp.
double cropLoadFactor(const QRectF &crop) { return 1.0 / std::clamp(std::min(crop.width(), crop.height()), 0.05, 1.0); }

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
        const int side = std::min(8192, int(std::max(s.size.width(), s.size.height()) * (s.fit == Fit::Cover ? 3 : 2) * cropLoadFactor(m_snap->crop)));
        const QImage source = cropped(AnimStudio::loadEntry(m_store, m_snap->used[size_t(index)], side), m_snap->crop);
        QImage out = renderFrame(source, s, m_snap->used[size_t(index)].style);
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
    m_estimatePool.setMaxThreadCount(1);
    rebuildSnapshot();
}

AnimStudio::~AnimStudio()
{
    if (m_cancel)
        m_cancel->store(true);
    if (m_estimateCancel)
        m_estimateCancel->store(true);
    m_estimatePool.waitForDone();
    m_pool.waitForDone(); // no timeout: an export still running reports back to this object (it stops at the next frame)
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
    case StyledRole: return !e.style.isPlain();
    default: return {};
    }
}

QHash<int, QByteArray> AnimStudio::roleNames() const
{
    return {{IdRole, "entryId"}, {PathRole, "path"}, {NameRole, "name"}, {HoldRole, "holdMs"}, {FromAnimationRole, "fromAnimation"}, {StyledRole, "styled"}};
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
    s.skip = std::clamp(o.value("skip").toInt(), 1, 8);
    s.crop = QRectF(o.value("cropX").toDouble(), o.value("cropY").toDouble(), o.value("cropW").toDouble(), o.value("cropH").toDouble());
    // the pictures used: one of every `skip`, holding the time of the ones that are left out
    for (size_t i = 0; i < m_entries.size(); i += size_t(s.skip)) {
        Entry e = m_entries[i];
        int total = 0;
        for (size_t j = i; j < std::min(m_entries.size(), i + size_t(s.skip)); ++j)
            total += m_entries[j].holdMs;
        e.holdMs = total;
        s.used.push_back(std::move(e));
    }
    std::vector<int> holds;
    for (const Entry &e : s.used)
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
    if (m_estimating || m_estimatedBytes >= 0) { // what was found out no longer holds
        if (m_estimateCancel)
            m_estimateCancel->store(true);
        ++m_estimateGeneration;
        m_estimating = false;
        m_estimatedBytes = -1;
        emit estimateChanged();
    }
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
    if (entryIndex % snap->skip != 0) // left out
        return -1;
    const int used = entryIndex / snap->skip;
    for (size_t i = 0; i < snap->plan.size(); ++i)
        if (snap->plan[i].a == used && snap->plan[i].isStill())
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
                    if (!m_entries.empty())
                        e.style = m_entries.front().style; // the effect and text are the animation's
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
        if (!m_entries.empty())
            e.style = m_entries.front().style; // the effect and text are the animation's
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
        if (s.isValid() && s.height() > 0) {
            m_firstSize = s;
            refreshAspect();
        }
        if (m_sizeIsAutomatic)
            applyFirstSize(640);
    }
    changed();
    return added;
}

int AnimStudio::addFrames(const QList<QImage> &frames, int holdMs, const QString &label)
{
    const bool wasEmpty = m_entries.empty();
    const int room = std::max(0, kMaxEntries - int(m_entries.size()));
    const int n = std::min(int(frames.size()), room);
    if (n == 0)
        return 0;
    beginInsertRows({}, int(m_entries.size()), int(m_entries.size()) + n - 1);
    for (int i = 0; i < n; ++i) {
        Entry e;
        e.id = m_nextId++;
        e.memory = frames[i].convertToFormat(QImage::Format_RGBA8888);
        e.label = QStringLiteral("%1 · %2").arg(label).arg(i + 1);
        e.holdMs = std::max(20, holdMs);
        if (!m_entries.empty())
            e.style = m_entries.front().style; // the effect and text are the animation's
        m_entries.push_back(std::move(e));
    }
    endInsertRows();
    if (wasEmpty) {
        const QSize s = m_entries.front().memory.size();
        if (s.isValid() && s.height() > 0) {
            m_firstSize = s;
            refreshAspect();
        }
        applyFirstSize(std::max(s.width(), s.height())); // the size chosen for the video
        m_sizeIsAutomatic = false;
    }
    changed();
    return n;
}

int AnimStudio::addUrls(const QVariantList &urls)
{
    QStringList paths;
    for (const QVariant &u : urls) {
        // from the page these arrive as text ("file:///C:/..."); a QUrl converts to the same text
        const QString text = u.toString();
        paths << (text.startsWith(QLatin1String("file:")) ? QUrl(text).toLocalFile() : text);
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
    m_options.insert("cropX", 0.0); // a new animation starts whole
    m_options.insert("cropY", 0.0);
    m_options.insert("cropW", 1.0);
    m_options.insert("cropH", 1.0);
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

// ---- the look of one picture -----------------------------------------------------------------------------------------

namespace {
uint packRgb(const QColor &c) { return (uint(c.red()) << 16) | (uint(c.green()) << 8) | uint(c.blue()); }
QColor unpackRgb(const QVariant &v) { const uint c = v.toUInt() & 0xFFFFFF; return QColor(int((c >> 16) & 255), int((c >> 8) & 255), int(c & 255)); }
}

QVariantMap AnimStudio::styleOf(int index) const
{
    const core::anim::FrameStyle st = index >= 0 && index < count() ? m_entries[size_t(index)].style : core::anim::FrameStyle{};
    QVariantMap m;
    m.insert("effectId", st.effectId);
    m.insert("effectPreset", st.effectPreset);
    m.insert("effectMix", st.effectMix * 100.0);
    m.insert("text", st.text.text);
    m.insert("family", st.text.family);
    m.insert("textX", st.text.x * 100.0);
    m.insert("textY", st.text.y * 100.0);
    m.insert("textSize", st.text.size);
    m.insert("bold", st.text.bold);
    m.insert("color", packRgb(st.text.color));
    m.insert("outline", st.text.outline);
    m.insert("outlineColor", packRgb(st.text.outlineColor));
    m.insert("caption", st.caption);
    m.insert("bandColor", packRgb(st.bandColor));
    return m;
}

bool AnimStudio::applyStyleKey(core::anim::FrameStyle &st, const QString &key, const QVariant &value)
{
    const core::anim::FrameStyle before = st;
    if (key == QLatin1String("effectId")) {
        const QString id = value.toString();
        st.effectId = id.isEmpty() || effectUsableOnFrames(id) ? id : QString();
        st.effectPreset = -1;
    } else if (key == QLatin1String("effectPreset")) st.effectPreset = std::max(-1, value.toInt());
    else if (key == QLatin1String("effectMix")) st.effectMix = std::clamp(value.toDouble() / 100.0, 0.0, 1.0);
    else if (key == QLatin1String("text")) st.text.text = value.toString().left(500);
    else if (key == QLatin1String("family")) st.text.family = value.toString();
    else if (key == QLatin1String("textX")) st.text.x = std::clamp(value.toDouble() / 100.0, 0.0, 1.0);
    else if (key == QLatin1String("textY")) st.text.y = std::clamp(value.toDouble() / 100.0, 0.0, 1.0);
    else if (key == QLatin1String("textSize")) st.text.size = std::clamp(value.toDouble(), 1.0, 60.0);
    else if (key == QLatin1String("bold")) st.text.bold = value.toBool();
    else if (key == QLatin1String("color")) st.text.color = unpackRgb(value);
    else if (key == QLatin1String("outline")) st.text.outline = value.toBool();
    else if (key == QLatin1String("outlineColor")) st.text.outlineColor = unpackRgb(value);
    else if (key == QLatin1String("bandColor")) st.bandColor = unpackRgb(value);
    else if (key == QLatin1String("caption")) {
        // the captions of the Efectos catalogue come with their own look: the colours they start from
        const int c = std::clamp(value.toInt(), -1, 3);
        st.caption = c;
        if (c == 0) { st.text.color = QColor(0, 0, 0); st.bandColor = QColor(255, 255, 255); st.text.size = 9.0; }
        else if (c == 1) { st.text.color = QColor(255, 255, 255); st.bandColor = QColor(0, 0, 0); st.text.size = 8.0; }
        else if (c == 2) { st.text.color = QColor(255, 255, 255); st.text.size = 12.0; }
        else if (c == 3) { st.text.color = QColor(255, 255, 255); st.text.size = 7.0; }
    }
    else return false;
    return !(st == before);
}

void AnimStudio::setStyleValue(int index, const QString &key, const QVariant &value)
{
    if (index < 0 || index >= count())
        return;
    if (!applyStyleKey(m_entries[size_t(index)].style, key, value))
        return;
    emit dataChanged(this->index(index), this->index(index), {StyledRole});
    changed();
}

void AnimStudio::setStyleValueAll(const QString &key, const QVariant &value)
{
    bool any = false;
    for (Entry &e : m_entries)
        any = applyStyleKey(e.style, key, value) || any;
    if (!any)
        return;
    emit dataChanged(this->index(0), this->index(count() - 1), {StyledRole});
    changed();
}

void AnimStudio::clearStyleAll()
{
    bool any = false;
    for (Entry &e : m_entries)
        if (!(e.style == core::anim::FrameStyle{})) {
            e.style = {};
            any = true;
        }
    if (!any)
        return;
    emit dataChanged(this->index(0), this->index(count() - 1), {StyledRole});
    changed();
}

void AnimStudio::copyStyleToAll(int index)
{
    if (index < 0 || index >= count())
        return;
    const core::anim::FrameStyle st = m_entries[size_t(index)].style;
    for (Entry &e : m_entries)
        e.style = st;
    emit dataChanged(this->index(0), this->index(count() - 1), {StyledRole});
    changed();
}

void AnimStudio::clearStyle(int index)
{
    if (index < 0 || index >= count() || m_entries[size_t(index)].style == core::anim::FrameStyle{})
        return;
    m_entries[size_t(index)].style = {};
    emit dataChanged(this->index(index), this->index(index), {StyledRole});
    changed();
}

QStringList AnimStudio::fontFamilies() const { return QFontDatabase::families(); }

QVariantList AnimStudio::frameEffects() const
{
    QVariantList list;
    for (const auto &spec : core::edit::allEffects()) {
        if (!effectUsableOnFrames(spec.id))
            continue;
        QStringList presets;
        for (const auto &p : spec.presets)
            presets << core::tr(p.name);
        QString group;
        for (const auto &g : core::edit::effectGroups())
            if (g.id == spec.group)
                group = core::tr(g.name);
        list.append(QVariantMap{{"id", spec.id}, {"name", core::tr(spec.name)}, {"group", group}, {"presets", presets}});
    }
    return list;
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
    else if (key == "defaultHold") clampInt(20, 300);
    else if (key == "skip") clampInt(1, 8);
    else if (key == "gifCompression") clampInt(0, 100);
    else if (key == "background") v = int(value.toUInt() & 0xFFFFFF);
    else if (key == "speed") v = std::clamp(value.toDouble(), 0.1, 10.0);
    else if (value.typeId() == QMetaType::Bool || m_options.value(key).typeId() == QMetaType::Bool) v = value.toBool();

    if (m_options.value(key) == v)
        return;
    m_options.insert(key, v);
    if (key == "gifCompression") {
        // one bar for the weight: 256 colours (with dithering) down to 16 (without)
        const double level = v.toInt() / 100.0;
        m_options.insert("gifColors", std::clamp(int(std::lround(256.0 * std::pow(16.0 / 256.0, level))), 2, 256));
        m_options.insert("gifDither", v.toInt() < 35);
        m_options.insert("gifLocal", false);
    }

    if (m_options.value("lockAspect").toBool() && (key == "width" || key == "lockAspect")) {
        m_options.insert("height", std::clamp(int(std::lround(m_options.value("width").toInt() / m_aspect)), 16, 4096));
    } else if (m_options.value("lockAspect").toBool() && key == "height") {
        m_options.insert("width", std::clamp(int(std::lround(m_options.value("height").toInt() * m_aspect)), 16, 4096));
    }
    changed();
}

void AnimStudio::refreshAspect()
{
    const QRectF c(m_options.value("cropX").toDouble(), m_options.value("cropY").toDouble(), m_options.value("cropW").toDouble(),
                   m_options.value("cropH").toDouble());
    if (m_firstSize.isValid() && c.width() > 0 && c.height() > 0)
        m_aspect = (c.width() * m_firstSize.width()) / (c.height() * m_firstSize.height());
}

void AnimStudio::setCrop(qreal x, qreal y, qreal w, qreal h)
{
    w = std::clamp(w, 0.05, 1.0);
    h = std::clamp(h, 0.05, 1.0);
    x = std::clamp(x, 0.0, 1.0 - w);
    y = std::clamp(y, 0.0, 1.0 - h);
    m_options.insert("cropX", x);
    m_options.insert("cropY", y);
    m_options.insert("cropW", w);
    m_options.insert("cropH", h);
    refreshAspect();
    if (m_sizeIsAutomatic)
        applyFirstSize(std::max(m_options.value("width").toInt(), m_options.value("height").toInt()));
    else if (m_options.value("lockAspect").toBool()) // the proportions follow the crop, the width stays
        m_options.insert("height", std::clamp(int(std::lround(m_options.value("width").toInt() / m_aspect)), 16, 4096));
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
        const Entry &e = snap->used[size_t(entryIndex)];
        const QString key = QStringLiteral("%1|%2x%3|%4|%5|%6|%7|%8,%9,%10,%11").arg(e.id).arg(ps.size.width()).arg(ps.size.height()).arg(int(ps.fit))
                                .arg(ps.background.rgba(), 0, 16).arg(ps.transparentBackground).arg(e.style.signature())
                                .arg(snap->crop.x()).arg(snap->crop.y()).arg(snap->crop.width()).arg(snap->crop.height());
        {
            QMutexLocker lock(&m_previewMutex);
            const auto it = m_previewFitted.constFind(key);
            if (it != m_previewFitted.constEnd())
                return it.value();
        }
        const int side = std::min(8192, int(std::max(ps.size.width(), ps.size.height()) * 2 * cropLoadFactor(snap->crop)));
        const QImage out = renderFrame(cropped(loadEntry(m_store, e, side), snap->crop), ps, e.style);
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
    m_pool.start(QRunnable::create([this, snap, path, cancel, store]() {
        StudioFrameSource source(snap, store);
        QSaveFile out(path);
        QString error;
        bool ok = out.open(QIODevice::WriteOnly);
        if (!ok)
            error = core::tr("No se pudo crear el archivo.");
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
                error = core::tr("No se pudo guardar el archivo.");
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

void AnimStudio::estimateSize()
{
    if (m_exporting || m_estimating)
        return;
    const auto snap = snapshot();
    if (snap->plan.empty())
        return;
    m_estimating = true;
    m_estimatedBytes = -1;
    const int generation = ++m_estimateGeneration;
    m_estimateCancel = std::make_shared<std::atomic<bool>>(false);
    emit estimateChanged();

    const auto cancel = m_estimateCancel;
    PaneImageStore *store = m_store;
    m_estimatePool.start(QRunnable::create([this, snap, cancel, store, generation]() {
        StudioFrameSource source(snap, store);
        QBuffer out;
        QString error;
        bool ok = out.open(QIODevice::WriteOnly);
        auto progress = [&](int, int) { return !cancel->load(); };
        if (ok) {
            switch (snap->format) {
            case 1: ok = writeApng(source, snap->apng, out, progress, &error); break;
            case 2: ok = writeWebp(source, snap->webp, out, progress, &error); break;
            default: ok = writeGif(source, snap->gif, out, progress, &error); break;
            }
        }
        const qint64 bytes = ok && !cancel->load() ? out.size() : -1;
        QMetaObject::invokeMethod(this, [this, bytes, generation]() {
            if (generation != m_estimateGeneration)
                return; // something changed meanwhile
            m_estimating = false;
            m_estimatedBytes = bytes;
            emit estimateChanged();
        }, Qt::QueuedConnection);
    }));
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
