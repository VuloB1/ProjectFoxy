#include "PictureCache.h"

#include "edit/EditStack.h"

#include <algorithm>
#include <array>

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

constexpr int kProxyLongSide = 384;

} // namespace

PictureCache::PictureCache(Compute makeStructural) : m_makeStructural(std::move(makeStructural)) {}

QImage PictureCache::structural() const
{
    if (m_structuralDirty) {
        m_structural = m_makeStructural();
        m_structuralDirty = false;
    }
    return m_structural;
}

void PictureCache::setStructural(const QImage &picture)
{
    m_structural = picture;
    m_structuralDirty = false;
}

QVariantList PictureCache::histogram() const
{
    if (m_histogramDirty) {
        m_histogram = core::computeHistogram(structural());
        m_histogramDirty = false;
    }
    QVariantList list;
    list.reserve(256);
    for (int count : m_histogram.luma)
        list.append(double(count) / double(m_histogram.maxCount));
    return list;
}

QVariantMap PictureCache::histogramChannels() const
{
    if (m_histogramDirty) {
        m_histogram = core::computeHistogram(structural());
        m_histogramDirty = false;
    }
    return channelHistogramMap(m_histogram);
}

QImage PictureCache::proxy() const
{
    if (m_proxyDirty) {
        const QImage baked = structural();
        if (baked.isNull())
            m_proxy = QImage();
        else if (std::max(baked.width(), baked.height()) > kProxyLongSide)
            m_proxy = baked.scaled(kProxyLongSide, kProxyLongSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        else
            m_proxy = baked;
        m_proxyDirty = false;
    }
    return m_proxy;
}

QVariantMap PictureCache::adjustedHistogram(const core::edit::AdjustOp &adjust, const QString &lookId,
                                            double lookAmount) const
{
    if (m_adjustedDirty) {
        QImage img = proxy();
        if (!img.isNull()) {
            if (!lookId.isEmpty())
                img = core::edit::applyOperation(img, core::edit::FilterPresetOp{lookId, lookAmount});
            img = core::edit::applyOperation(img, adjust);
        }
        m_adjusted = channelHistogramMap(core::computeHistogram(img));
        m_adjustedDirty = false;
    }
    return m_adjusted;
}

void PictureCache::invalidatePicture()
{
    m_structuralDirty = true;
    m_histogramDirty = true;
    m_proxyDirty = true;
    m_adjustedDirty = true;
    m_lookThumbsStale = true;
}
