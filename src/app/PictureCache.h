#pragma once

#include "Histogram.h"
#include "edit/Operations.h"

#include <QImage>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <functional>

// What is derived from the picture and costs something to make, kept until the picture changes: the structural result
// (the history's crop/resize/rotate/flip/effects replayed on the original), its histogram, the small copy the panels
// work on, the histogram of what the live Ajustes/Filtros make of it, and whether the look previews still match.
//
// Everything is made lazily, the first time it is asked for after an invalidation. Two kinds of invalidation:
//   invalidatePicture(): the pixels changed (any structural edit, a new picture): everything goes.
//   invalidateAdjusted(): only the live Ajustes / Filtros changed: just the adjusted histogram goes.
// Not thread-safe: it lives on the UI thread, like what it caches.
class PictureCache {
public:
    // `makeStructural` computes the structural picture from scratch (what the history bakes onto the original).
    using Compute = std::function<QImage()>;
    explicit PictureCache(Compute makeStructural);

    // ---- the structural picture
    QImage structural() const;
    // A result already known (the exact effect result, or what a save worker calculated): used as the structural
    // picture without recalculating it. The things derived from it are not touched: invalidate them first if needed.
    void setStructural(const QImage &picture);
    bool structuralIsReady() const { return !m_structuralDirty; }
    // The structural picture when it is ready, a null image when it would have to be calculated (what a save hands to
    // its worker: null means "replay the operations yourself").
    QImage structuralIfReady() const { return m_structuralDirty ? QImage() : m_structural; }

    // ---- derived from it
    QVariantList histogram() const;         // luma, 0..1
    QVariantMap histogramChannels() const;  // luma, r, g, b: 256 heights each, normalised
    QImage proxy() const;                   // the structural picture on a 384 px long side (itself when smaller)
    // The histograms of the picture after the live overlay (look, then Ajustes), on the small copy.
    QVariantMap adjustedHistogram(const core::edit::AdjustOp &adjust, const QString &lookId, double lookAmount) const;

    // ---- the look previews (their pixels live in ImageProvider; this only says whether they are up to date)
    bool lookThumbsStale() const { return m_lookThumbsStale; }
    void lookThumbsMade() { m_lookThumbsStale = false; }

    void invalidatePicture();
    void invalidateAdjusted() { m_adjustedDirty = true; }

private:
    Compute m_makeStructural;
    mutable QImage m_structural;
    mutable bool m_structuralDirty = true;
    mutable core::Histogram m_histogram;
    mutable bool m_histogramDirty = true;
    mutable QImage m_proxy;
    mutable bool m_proxyDirty = true;
    mutable QVariantMap m_adjusted;
    mutable bool m_adjustedDirty = true;
    bool m_lookThumbsStale = true;
};
