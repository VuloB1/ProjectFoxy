#pragma once

#include "Operations.h"
#include <QImage>
#include <vector>

namespace core::edit {

// Applies a single operation to `source` and returns the result. Exposed as
// a free function (not just used internally by bake()) so callers that want
// a "live" edit that isn't part of the undo history - AppController's
// brightness/contrast/saturation sliders and filter presets - can apply one
// operation on demand without pushing it onto a stack.
QImage applyOperation(const QImage &source, const Operation &op);

// Replays `ops` over `source` (LiveOps change no pixels and are skipped). The same
// result as EditStack::bake() for the same operations, but it keeps no checkpoints and
// touches no shared state, so it may run on any thread - which is what the safe-save
// worker needs.
QImage bakeOperations(const QImage &source, const std::vector<Operation> &ops);

// The live overlay on top of a structural result: the Filtros look, then the Ajustes
// sliders, both through the CPU pipeline. The one definition of "what the file holds",
// used by saving, copying and the wallpaper.
QImage renderLiveOverlay(const QImage &structural, const QString &lookId, double lookAmount, const AdjustOp &adjust);

// Ordered, non-destructive list of edits applied to one ImageDocument.
// The source pixels are never mutated: bake() re-applies the stack from the
// original image, which keeps behavior simple and avoids compounding rounding
// error across edits. The one shortcut is a few "checkpoints" - finished results
// remembered per history position - so stepping back and forth through the
// undo history (or redoing a slow effect) does not redo work already done.
class EditStack {
public:
    void push(Operation op);
    bool undo();
    bool redo();
    void clear();

    // How many operations are active (= where the undo history currently stands).
    size_t undoPosition() const { return m_undoPosition; }
    // Forgets what could still be redone.
    void truncateRedo();

    bool canUndo() const { return m_undoPosition > 0; }
    bool canRedo() const { return m_undoPosition < m_ops.size(); }

    const std::vector<Operation> &operations() const { return m_ops; }
    bool isEmpty() const { return m_ops.empty(); }

    // --- the live overlay's place in the history (see LiveOp) -----------------
    // The newest LiveOp among the active operations, or nullptr when there is none
    // (= the overlay is at its neutral state).
    const LiveOp *activeLiveState() const;
    // Is the operation that undo() would remove / redo() would restore a LiveOp?
    bool undoTargetIsLive() const;
    bool redoTargetIsLive() const;
    // True when some active operation actually changes pixels (not just a LiveOp).
    bool hasActivePixelOps() const;
    // Those operations themselves, in order (the active LiveOps left out).
    std::vector<Operation> activePixelOps() const;
    // Overwrites the newest active operation with `op` when it is a LiveOp and
    // nothing has been undone past it - used to merge the many ticks of one slider
    // drag into a single history entry. Returns false (and does nothing) otherwise.
    bool replaceLastLive(const LiveOp &op);

    QImage bake(const QImage &source) const;

    // Tells the stack that `image` is exactly what bake(source) returns for the
    // CURRENT history position (the caller got it some other way, e.g. a preview
    // that was already calculated), so it can be reused instead of recalculated.
    void storeCheckpoint(const QImage &source, const QImage &image) const;

    // How many bytes of remembered full-size pictures the stack may hold (the least
    // recently used are dropped first; one is always kept). The caller sizes it to
    // the machine - a quarter of the free RAM, say - because these are as big as the
    // picture itself.
    void setCheckpointBudget(qint64 bytes);

private:
    struct Checkpoint {
        size_t position = 0; // the number of active operations this image is the result of
        QImage image;
        quint64 used = 0;    // last time it was stored or reused (for least-recently-used eviction)
    };
    void dropCheckpointsAbove(size_t position);
    void rememberCheckpoint(size_t position, const QImage &image) const;

    std::vector<Operation> m_ops;
    size_t m_undoPosition = 0; // ops[0..m_undoPosition) are "active"

    // Oldest first. Only meaningful for the source image they were made from
    // (identified by its cacheKey), and for the operations below `position`.
    mutable std::vector<Checkpoint> m_checkpoints;
    mutable qint64 m_checkpointSource = 0;
    mutable quint64 m_useClock = 0;
    qint64 m_checkpointBudget = 600LL * 1024 * 1024;
};

} // namespace core::edit
