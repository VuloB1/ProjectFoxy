#include "EditStack.h"
#include "AdjustMath.h"
#include "Effects.h"
#include "Looks.h"
#include "Resample.h"

#include <QTransform>
#include <algorithm>
#include <cmath>

namespace core::edit {

namespace {

QImage applyCrop(const QImage &source, const CropOp &op)
{
    QRect pixelRect(
        qRound(op.normalizedRect.x() * source.width()),
        qRound(op.normalizedRect.y() * source.height()),
        qRound(op.normalizedRect.width() * source.width()),
        qRound(op.normalizedRect.height() * source.height()));
    pixelRect = pixelRect.intersected(source.rect());
    if (pixelRect.isEmpty())
        return source;
    return source.copy(pixelRect);
}

QImage applyResize(const QImage &source, const ResizeOp &op)
{
    if (op.targetSize.isEmpty())
        return source;
    if (op.resampleFilter == QLatin1String("nearest"))
        return resizeNearest(source, op.targetSize, op.keepAspectRatio);
    return resizeHighQuality(source, op.targetSize, op.keepAspectRatio, op.enhanceDetail);
}

QImage applyRotate(const QImage &source, const RotateOp &op)
{
    if (std::fmod(op.degrees, 360.0) == 0.0)
        return source;
    QTransform t;
    t.rotate(op.degrees);
    return source.transformed(t, op.smooth ? Qt::SmoothTransformation : Qt::FastTransformation);
}

QImage applyFlip(const QImage &source, const FlipOp &op)
{
    if (!op.horizontal && !op.vertical)
        return source;
    return source.mirrored(op.horizontal, op.vertical);
}

} // namespace

QImage applyOperation(const QImage &source, const Operation &op)
{
    if (source.isNull())
        return source;
    return std::visit([&source](const auto &concreteOp) -> QImage {
        using T = std::decay_t<decltype(concreteOp)>;
        if constexpr (std::is_same_v<T, CropOp>) return applyCrop(source, concreteOp);
        else if constexpr (std::is_same_v<T, ResizeOp>) return applyResize(source, concreteOp);
        else if constexpr (std::is_same_v<T, RotateOp>) return applyRotate(source, concreteOp);
        else if constexpr (std::is_same_v<T, FlipOp>) return applyFlip(source, concreteOp);
        else if constexpr (std::is_same_v<T, AdjustOp>) return applyAdjustOp(source, concreteOp);
        else if constexpr (std::is_same_v<T, FilterPresetOp>) return applyLook(source, concreteOp.presetId, concreteOp.intensity);
        else if constexpr (std::is_same_v<T, EffectOp>) return applyEffect(source, concreteOp.effectId, concreteOp.values, concreteOp.mix);
        else if constexpr (std::is_same_v<T, LiveOp>) return source; // a live-overlay snapshot: no pixels change
        else return source;
    }, op);
}

QImage bakeOperations(const QImage &source, const std::vector<Operation> &ops)
{
    QImage img = source;
    for (const Operation &op : ops)
        img = applyOperation(img, op);
    return img;
}

QImage renderLiveOverlay(const QImage &structural, const QString &lookId, double lookAmount, const AdjustOp &adjust)
{
    QImage img = structural;
    if (!lookId.isEmpty())
        img = applyOperation(img, FilterPresetOp{lookId, lookAmount});
    if (!adjust.isIdentity())
        img = applyOperation(img, adjust);
    return img;
}

void EditStack::push(Operation op)
{
    m_ops.resize(m_undoPosition); // drop any redo tail
    dropCheckpointsAbove(m_undoPosition); // ...and what was calculated from it
    m_ops.push_back(std::move(op));
    m_undoPosition = m_ops.size();
}

void EditStack::truncateRedo()
{
    m_ops.resize(m_undoPosition);
    dropCheckpointsAbove(m_undoPosition);
}

bool EditStack::undo()
{
    if (m_undoPosition == 0)
        return false;
    --m_undoPosition;
    return true;
}

bool EditStack::redo()
{
    if (m_undoPosition >= m_ops.size())
        return false;
    ++m_undoPosition;
    return true;
}

const LiveOp *EditStack::activeLiveState() const
{
    for (size_t i = m_undoPosition; i > 0; --i)
        if (const auto *live = std::get_if<LiveOp>(&m_ops[i - 1]))
            return live;
    return nullptr;
}

bool EditStack::undoTargetIsLive() const
{
    return m_undoPosition > 0 && std::holds_alternative<LiveOp>(m_ops[m_undoPosition - 1]);
}

bool EditStack::redoTargetIsLive() const
{
    return m_undoPosition < m_ops.size() && std::holds_alternative<LiveOp>(m_ops[m_undoPosition]);
}

bool EditStack::hasActivePixelOps() const
{
    for (size_t i = 0; i < m_undoPosition; ++i)
        if (!std::holds_alternative<LiveOp>(m_ops[i]))
            return true;
    return false;
}

std::vector<Operation> EditStack::activePixelOps() const
{
    std::vector<Operation> active;
    for (size_t i = 0; i < m_undoPosition; ++i)
        if (!std::holds_alternative<LiveOp>(m_ops[i]))
            active.push_back(m_ops[i]);
    return active;
}

bool EditStack::replaceLastLive(const LiveOp &op)
{
    if (m_undoPosition == 0 || m_undoPosition != m_ops.size() || !std::holds_alternative<LiveOp>(m_ops[m_undoPosition - 1]))
        return false;
    // No checkpoint is invalidated: a LiveOp changes no pixels, so every result
    // remembered for this position is still exactly right.
    m_ops[m_undoPosition - 1] = op;
    return true;
}

void EditStack::clear()
{
    m_ops.clear();
    m_undoPosition = 0;
    m_checkpoints.clear();
}

void EditStack::dropCheckpointsAbove(size_t position)
{
    m_checkpoints.erase(std::remove_if(m_checkpoints.begin(), m_checkpoints.end(),
                                       [position](const Checkpoint &c) { return c.position > position; }),
                        m_checkpoints.end());
}

void EditStack::rememberCheckpoint(size_t position, const QImage &image) const
{
    if (position == 0)
        return; // position 0 is the source itself
    for (Checkpoint &c : m_checkpoints) {
        if (c.position == position) {
            c.image = image;
            c.used = ++m_useClock;
            return;
        }
    }
    m_checkpoints.push_back({position, image, ++m_useClock});

    // A handful of entries, and no more bytes than the budget: they are full-size
    // pictures. (Entries share memory with whatever else holds the same QImage, so
    // the real cost is often lower.) The least recently used go first, but the
    // newest is always kept.
    constexpr size_t kMaxEntries = 6;
    auto total = [this]() {
        qint64 sum = 0;
        for (const Checkpoint &c : m_checkpoints)
            sum += c.image.sizeInBytes();
        return sum;
    };
    while (m_checkpoints.size() > 1 && (m_checkpoints.size() > kMaxEntries || total() > m_checkpointBudget)) {
        auto oldest = std::min_element(m_checkpoints.begin(), m_checkpoints.end(),
                                       [](const Checkpoint &a, const Checkpoint &b) { return a.used < b.used; });
        m_checkpoints.erase(oldest);
    }
}

void EditStack::setCheckpointBudget(qint64 bytes)
{
    m_checkpointBudget = std::max<qint64>(0, bytes);
}

void EditStack::storeCheckpoint(const QImage &source, const QImage &image) const
{
    if (source.cacheKey() != m_checkpointSource) {
        m_checkpoints.clear();
        m_checkpointSource = source.cacheKey();
    }
    rememberCheckpoint(m_undoPosition, image);
}

QImage EditStack::bake(const QImage &source) const
{
    if (source.cacheKey() != m_checkpointSource) { // a different picture: nothing remembered applies
        m_checkpoints.clear();
        m_checkpointSource = source.cacheKey();
    }

    // Start from the furthest result already known at or before this position.
    QImage img = source;
    size_t start = 0;
    Checkpoint *used = nullptr;
    for (Checkpoint &c : m_checkpoints) {
        if (c.position <= m_undoPosition && c.position >= start) {
            img = c.image;
            start = c.position;
            used = &c;
        }
    }
    if (used)
        used->used = ++m_useClock; // reused: keep it around longer
    for (size_t i = start; i < m_undoPosition; ++i)
        img = applyOperation(img, m_ops[i]);

    if (start < m_undoPosition)
        rememberCheckpoint(m_undoPosition, img);
    return img;
}

} // namespace core::edit
