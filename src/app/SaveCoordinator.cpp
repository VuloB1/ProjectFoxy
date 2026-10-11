#include "SaveCoordinator.h"

#include "Translate.h"
#include "edit/EditStack.h"

#include <QMetaObject>
#include <QRunnable>

SaveCoordinator::SaveCoordinator(QObject *parent) : QObject(parent)
{
    m_pool.setMaxThreadCount(1); // one save at a time
}

SaveCoordinator::~SaveCoordinator()
{
    // A save that is still writing is allowed to finish: closing the program must not
    // leave a half-done file (the write is atomic, but the user asked for it).
    m_pool.waitForDone();
}

void SaveCoordinator::start(SaveJob job)
{
    m_saving = true;
    m_pool.start(QRunnable::create([this, job = std::move(job)]() {
        SaveOutcome outcome;
        outcome.serial = job.serial;
        outcome.recipe = job.recipe;
        outcome.path = job.path;

        QImage base = job.structural;
        if (base.isNull()) {
            base = core::edit::bakeOperations(job.original, job.ops);
            outcome.structural = base; // handed back so the UI can reuse it instead of recalculating
            outcome.ops = job.ops;
        }
        const QImage rendered = core::edit::renderLiveOverlay(base, job.lookId, job.lookAmount, job.adjust);
        if (rendered.isNull()) {
            outcome.error = core::tr("No hay imagen que guardar");
        } else {
            const core::SaveResult result = core::saveImageWithOptions(rendered, job.path, job.options);
            outcome.ok = result.ok;
            outcome.error = result.error;
            outcome.warning = result.warning;
        }
        QMetaObject::invokeMethod(this, [this, outcome]() {
            m_saving = false;
            emit finished(outcome);
        }, Qt::QueuedConnection);
    }));
}

void SaveCoordinator::blockOnUnsaved(PendingAction action, const QString &path)
{
    m_blocked = {action, path};
}

PendingRequest SaveCoordinator::takeBlocked()
{
    return std::exchange(m_blocked, PendingRequest{});
}

void SaveCoordinator::afterSave(PendingAction action, const QString &path)
{
    m_after = {action, path};
}

PendingRequest SaveCoordinator::takeAfterSave()
{
    return std::exchange(m_after, PendingRequest{});
}
