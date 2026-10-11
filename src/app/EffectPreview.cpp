#include "EffectPreview.h"

#include "Translate.h"

#include <QMetaObject>
#include <QRunnable>
#include <algorithm>
#include <cmath>

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

EffectPreview::EffectPreview(BaseProvider base, QObject *parent)
    : QObject(parent)
    , m_base(std::move(base))
{
    m_pool.setMaxThreadCount(3); // the quick preview, the exact one, and one being cancelled
    m_debounce.setSingleShot(true);
    connect(&m_debounce, &QTimer::timeout, this, [this]() { startJob(2); });
}

EffectPreview::~EffectPreview()
{
    // Stop any effect still being calculated and let the workers finish, so none can
    // post its result to an object that no longer exists. The kernels poll the
    // cancel flag between rows, so this is a short wait - and it is a complete one:
    // a worker that outlived a timeout would be holding a dangling `this`.
    m_debounce.stop();
    if (m_cancel)
        m_cancel->store(true);
    m_pool.waitForDone();
}

// --- the catalogue ---------------------------------------------------------------------------------------------------

QVariantList EffectPreview::list()
{
    QVariantList list;
    for (const core::edit::EffectSpec &fx : core::edit::allEffects()) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), fx.id);
        m.insert(QStringLiteral("name"), core::tr(fx.name));
        m.insert(QStringLiteral("group"), fx.group);
        m.insert(QStringLiteral("hidden"), fx.hidden);
        m.insert(QStringLiteral("changesSize"), fx.changesSize);
        list.append(m);
    }
    return list;
}

QVariantList EffectPreview::groups()
{
    QVariantList list;
    for (const core::edit::EffectGroup &group : core::edit::effectGroups()) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), group.id);
        m.insert(QStringLiteral("name"), core::tr(group.name));
        list.append(m);
    }
    return list;
}

QVariantList EffectPreview::params() const
{
    QVariantList list;
    const core::edit::EffectSpec *spec = core::edit::findEffect(m_id);
    if (!spec)
        return list;
    for (const core::edit::EffectParam &p : spec->params) {
        QVariantMap m;
        m.insert(QStringLiteral("label"), core::tr(p.label));
        m.insert(QStringLiteral("min"), p.min);
        m.insert(QStringLiteral("max"), p.max);
        m.insert(QStringLiteral("def"), p.def);
        m.insert(QStringLiteral("suffix"), p.suffix);
        m.insert(QStringLiteral("integer"), p.integer);
        m.insert(QStringLiteral("toggle"), p.toggle);
        QStringList options;
        for (const QString &o : p.options)
            options << core::tr(o);
        m.insert(QStringLiteral("options"), options);
        m.insert(QStringLiteral("color"), p.color);
        m.insert(QStringLiteral("seed"), p.seed);
        m.insert(QStringLiteral("dependsOn"), p.dependsOn);
        m.insert(QStringLiteral("dependsMask"), p.dependsMask);
        m.insert(QStringLiteral("hint"), core::tr(p.hint));
        list.append(m);
    }
    return list;
}

QStringList EffectPreview::presets() const
{
    QStringList names;
    const core::edit::EffectSpec *spec = core::edit::findEffect(m_id);
    if (spec)
        for (const core::edit::EffectPreset &p : spec->presets)
            names << core::tr(p.name);
    return names;
}

QVariantList EffectPreview::overlays() const
{
    QVariantList list;
    const core::edit::EffectSpec *spec = core::edit::findEffect(m_id);
    if (!spec)
        return list;
    auto range = [spec](int index, double &lo, double &hi) {
        if (index >= 0 && size_t(index) < spec->params.size()) {
            lo = spec->params[size_t(index)].min;
            hi = spec->params[size_t(index)].max;
        }
    };
    for (const core::edit::EffectOverlay &o : spec->overlays) {
        QVariantMap m;
        const char *kind = o.kind == core::edit::EffectOverlay::Point ? "point" : o.kind == core::edit::EffectOverlay::VLine ? "vline" : "hline";
        m.insert(QStringLiteral("kind"), QString::fromLatin1(kind));
        m.insert(QStringLiteral("x"), o.x);
        m.insert(QStringLiteral("y"), o.y);
        m.insert(QStringLiteral("label"), core::tr(o.label));
        m.insert(QStringLiteral("mapping"), o.mapping);
        double xlo = 0, xhi = 1, ylo = 0, yhi = 1;
        range(o.x, xlo, xhi);
        range(o.y, ylo, yhi);
        m.insert(QStringLiteral("xmin"), xlo);
        m.insert(QStringLiteral("xmax"), xhi);
        m.insert(QStringLiteral("ymin"), ylo);
        m.insert(QStringLiteral("ymax"), yhi);
        list.append(m);
    }
    return list;
}

QVariantList EffectPreview::values() const
{
    QVariantList list;
    const core::edit::EffectSpec *spec = core::edit::findEffect(m_id);
    if (!spec)
        return list;
    for (size_t i = 0; i < spec->params.size() && i < m_values.size(); ++i)
        list.append(m_values[i]);
    return list;
}

bool EffectPreview::usesText() const
{
    const core::edit::EffectSpec *spec = core::edit::findEffect(m_id);
    return spec && spec->usesText;
}

QImage EffectPreview::proxyPicture() const
{
    if (m_proxy.isNull()) {
        const QImage base = m_base();
        if (std::max(base.width(), base.height()) > kEffectProxySide)
            m_proxy = base.scaled(kEffectProxySide, kEffectProxySide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        else
            m_proxy = base;
    }
    return m_proxy;
}

QImage EffectPreview::applyTo(const QImage &picture) const
{
    if (m_id.isEmpty() || m_mix <= 0.0)
        return picture;
    return (m_fullValid && !m_full.isNull())
               ? m_full // the exact full-size result, already calculated
               : core::edit::applyEffect(picture, m_id, m_values, m_mix, nullptr, m_text);
}

// --- commands --------------------------------------------------------------------------------------------------------

bool EffectPreview::drop()
{
    m_debounce.stop();
    if (m_cancel) {
        m_cancel->store(true); // anything still being calculated is now stale
        m_cancel.reset();
    }
    ++m_generation;
    m_full = QImage();
    m_fullValid = false;
    const bool wasShown = m_shown;
    m_shown = false;
    if (m_id.isEmpty())
        return wasShown;
    m_id.clear();
    m_values = {};
    m_mix = 1.0;
    m_text.clear();
    emit changed();
    emit valuesChanged();
    emit busyChanged();
    emit activeChanged();
    return wasShown;
}

void EffectPreview::select(const QString &id)
{
    const core::edit::EffectSpec *spec = core::edit::findEffect(id);
    if (!spec || id == m_id)
        return;
    const bool first = m_id.isEmpty();
    m_id = id;
    m_values = core::edit::defaultEffectValues(*spec);
    m_mix = 1.0;
    m_text.clear();
    emit changed();
    emit valuesChanged();
    if (first)
        emit activeChanged();
    schedule();
}

void EffectPreview::setValue(int index, qreal value)
{
    const core::edit::EffectSpec *spec = core::edit::findEffect(m_id);
    if (!spec || index < 0 || size_t(index) >= spec->params.size())
        return;
    const core::edit::EffectParam &p = spec->params[size_t(index)];
    double v = std::clamp(double(value), p.min, p.max);
    if (p.integer)
        v = std::round(v);
    if (v == m_values[size_t(index)])
        return;
    m_values[size_t(index)] = v;
    // The guides of "Estirar" only mean something while that direction is being stretched: moving one while its
    // Horizontal / Vertical slider is at 100% (no stretch) would show nothing at all, so it starts stretching.
    if (m_id == QLatin1String("stretch") && index >= 0 && index <= 3) {
        const size_t scale = index <= 1 ? 4 : 5;
        if (std::abs(m_values[scale] - 100.0) < 0.5)
            m_values[scale] = 150.0;
    }
    emit valuesChanged();
    schedule();
}

void EffectPreview::setText(const QString &text)
{
    const core::edit::EffectSpec *spec = core::edit::findEffect(m_id);
    if (!spec || !spec->usesText || text == m_text)
        return;
    m_text = text.left(1000);
    emit valuesChanged();
    schedule();
}

void EffectPreview::setMix(qreal mix)
{
    if (m_id.isEmpty())
        return;
    const double v = std::clamp(double(mix), 0.0, 1.0);
    if (v == m_mix)
        return;
    m_mix = v;
    emit valuesChanged();
    schedule();
}

void EffectPreview::applyPreset(int index)
{
    const core::edit::EffectSpec *spec = core::edit::findEffect(m_id);
    if (!spec || index < 0 || size_t(index) >= spec->presets.size())
        return;
    m_values = core::edit::applyEffectPreset(*spec, spec->presets[size_t(index)], m_values);
    emit valuesChanged();
    schedule();
}

void EffectPreview::setValues(const QVariantList &values)
{
    const core::edit::EffectSpec *spec = core::edit::findEffect(m_id);
    if (!spec)
        return;
    bool changed = false;
    for (int i = 0; i < values.size() && size_t(i) < spec->params.size(); ++i) {
        const core::edit::EffectParam &p = spec->params[size_t(i)];
        double v = std::clamp(values[i].toDouble(), p.min, p.max);
        if (p.integer)
            v = std::round(v);
        if (v != m_values[size_t(i)]) {
            m_values[size_t(i)] = v;
            changed = true;
        }
    }
    if (!changed)
        return;
    emit valuesChanged();
    schedule();
}

void EffectPreview::resetValues()
{
    const core::edit::EffectSpec *spec = core::edit::findEffect(m_id);
    if (!spec)
        return;
    m_values = core::edit::defaultEffectValues(*spec);
    m_mix = 1.0;
    emit valuesChanged();
    schedule();
}

// --- the calculations ------------------------------------------------------------------------------------------------

// Called after any parameter change: starts the quick preview and (re)arms the debounce timer that launches the
// exact full-size one.
void EffectPreview::schedule()
{
    const QImage base = m_base();
    if (m_id.isEmpty() || base.isNull())
        return;

    // A new generation: whatever the previous one is still calculating is stale.
    if (m_cancel)
        m_cancel->store(true);
    m_cancel = std::make_shared<std::atomic<bool>>(false);
    ++m_generation;
    m_full = QImage();
    m_fullValid = false;

    const core::edit::EffectSpec *spec = core::edit::findEffect(m_id);
    if (spec && spec->fullSize) {
        // Noise reduction only means something on the real pixels (a shrunken copy has already
        // lost most of its noise): no quick approximation, just the exact result once the sliders rest.
        m_debounce.start(kEffectSettleMs);
    } else if (std::max(base.width(), base.height()) > kEffectProxyThreshold) {
        startJob(1); // approximate, immediately...
        m_debounce.start(kEffectSettleMs); // ...exact once the sliders rest
    } else {
        m_debounce.start(kEffectSmallSettleMs);
    }
}

// stage 1 = preview computed on a shrunken copy and stretched back up; stage 2 = the effect on the real pixels.
void EffectPreview::startJob(int stage)
{
    if (m_id.isEmpty() || !m_cancel)
        return;
    const QImage base = m_base();
    if (base.isNull())
        return;

    const QImage input = stage == 1 ? proxyPicture() : base;
    const QString id = m_id;
    const core::edit::EffectValues values = m_values;
    const double mix = m_mix;
    const int generation = m_generation;
    const std::shared_ptr<std::atomic<bool>> cancel = m_cancel;
    // What the exact result measures: the same as the picture, unless the effect changes the size.
    const QString text = m_text;
    const QSize fullSize = core::edit::effectOutputSize(id, values, base.size(), text);

    ++m_jobsRunning;
    emit busyChanged();

    // m_pool runs this task; its row bands go to the dedicated
    // rowWorkerPool (see ParallelRows.h), so waiting on them cannot deadlock.
    m_pool.start(QRunnable::create([=, this]() {
        QImage out = core::edit::applyEffect(input, id, values, mix, cancel.get(), text);
        const bool cancelled = cancel->load();
        if (cancelled)
            out = QImage(); // a stale, half-written result: do not carry a full-size buffer around until the GUI thread discards it
        else if (out.size() != fullSize)
            out = out.scaled(fullSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        QMetaObject::invokeMethod(
            this, [this, generation, stage, out, cancelled]() { finishJob(generation, stage, out, cancelled); },
            Qt::QueuedConnection);
    }));
}

void EffectPreview::finishJob(int generation, int stage, const QImage &image, bool wasCancelled)
{
    --m_jobsRunning;
    const bool current = !wasCancelled && generation == m_generation && !m_id.isEmpty();
    if (current) {
        if (stage == 2) {
            m_full = image;
            m_fullValid = true;
            m_shown = true;
            emit pictureReady(image);
        } else if (!m_fullValid) { // the approximate one, unless the exact one already beat it
            m_shown = true;
            emit pictureReady(image);
        }
    }
    emit busyChanged();
}
