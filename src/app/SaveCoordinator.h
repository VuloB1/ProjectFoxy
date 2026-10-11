#pragma once

#include "ImageWriter.h"
#include "edit/Operations.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <QThreadPool>
#include <vector>

// What opening a picture or pasting one would have done, when something stopped it: unsaved changes (the UI asks what
// to do), or a save still writing the file.
enum class PendingAction { None, OpenPath, Paste };
struct PendingRequest {
    PendingAction action = PendingAction::None;
    QString path; // for OpenPath
};

// Everything the save worker needs, taken on the UI thread at the moment of the save and immutable from there on.
// QImage is copy-on-write, so holding these costs no pixels; the history and the live overlay are small records.
struct SaveJob {
    QImage original;                                 // the picture as opened
    std::vector<core::edit::Operation> ops;          // the structural edits (crop, resize, rotate, flip, effects)
    QImage structural;                               // their result when already known; null: the worker replays `ops`
    core::edit::AdjustOp adjust;                     // the live Ajustes overlay...
    QString lookId;                                  // ...and the Filtros look with its amount
    double lookAmount = 1.0;
    core::edit::EditRecipe recipe;                   // what the file will hold: becomes "saved" if it works
    quint64 serial = 0;                              // the document the save was started for
    QString path;
    core::SaveOptions options;
};

// What the worker reports back.
struct SaveOutcome {
    quint64 serial = 0;               // the document the save was started for
    core::edit::EditRecipe recipe;    // what the file holds, if it worked
    QString path;
    bool ok = false;
    QString error;
    QString warning;
    QImage structural;                // the structural bake the worker had to compute, if it did...
    std::vector<core::edit::Operation> ops; // ...and the pixel operations it belongs to
};

// Saving the picture and the requests that wait for it. One save at a time, written on its own pool (the destructor
// waits for it: the worker posts its result back to this object). It keeps the two kinds of request that can be
// waiting - what the unsaved-changes question will decide, and what to do once the running save has worked - but
// knows nothing of the document: what a finished save means for it is the caller's to apply.
class SaveCoordinator : public QObject {
    Q_OBJECT
public:
    explicit SaveCoordinator(QObject *parent = nullptr);
    ~SaveCoordinator() override;

    // ---- the save itself
    bool saving() const { return m_saving; }
    // Starts writing; saving() is true until finished() is emitted.
    void start(SaveJob job);

    // ---- an open/paste that unsaved changes stopped: the UI asks, then carries it out (or not)
    void blockOnUnsaved(PendingAction action, const QString &path = {});
    PendingRequest takeBlocked();
    // The person said "discard": the request that follows must not be blocked again.
    bool discardApproved() const { return m_discardApproved; }
    void setDiscardApproved(bool approved) { m_discardApproved = approved; }

    // ---- an open/paste asked for while a save runs (or the "save" answer to the question): done once the file is
    // safe, dropped if the save fails
    void afterSave(PendingAction action, const QString &path = {});
    PendingRequest takeAfterSave();

signals:
    // The save ended (saving() is already false).
    void finished(const SaveOutcome &outcome);

private:
    QThreadPool m_pool;
    bool m_saving = false;
    PendingRequest m_blocked;
    PendingRequest m_after;
    bool m_discardApproved = false;
};
