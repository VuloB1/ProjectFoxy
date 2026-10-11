#include "SaveCoordinator.h"
#include <QtTest>

Q_DECLARE_METATYPE(SaveOutcome)

namespace {

SaveJob jobFor(const QString &path, QColor colour = Qt::red)
{
    SaveJob job;
    job.original = QImage(60, 40, QImage::Format_RGBA8888);
    job.original.fill(colour);
    job.path = path;
    job.serial = 7;
    return job;
}

} // namespace

// Saving the picture on its own thread, and the open/paste requests that wait for a save or for the answer to the
// unsaved-changes question.
class TestSaveCoordinator : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

private slots:
    void initTestCase() { qRegisterMetaType<SaveOutcome>(); }

    void aSaveWritesTheFileAndReportsBack()
    {
        SaveCoordinator saver;
        QSignalSpy finished(&saver, &SaveCoordinator::finished);
        const QString path = m_dir.filePath("a.png");
        QVERIFY(!saver.saving());
        saver.start(jobFor(path));
        QVERIFY(saver.saving());
        QVERIFY(finished.wait(10000));
        QVERIFY(!saver.saving()); // already false when the result arrives
        const SaveOutcome outcome = finished.first().first().value<SaveOutcome>();
        QVERIFY2(outcome.ok, qPrintable(outcome.error));
        QCOMPARE(outcome.serial, quint64(7)); // the document it was started for
        QCOMPARE(outcome.path, path);
        QVERIFY(!outcome.structural.isNull()); // no structural result was given, so the worker made (and returns) it
        const QImage written(path);
        QCOMPARE(written.size(), QSize(60, 40));
        QCOMPARE(written.pixelColor(5, 5), QColor(Qt::red));
    }

    void aFailureIsReportedNotThrown()
    {
        SaveCoordinator saver;
        QSignalSpy finished(&saver, &SaveCoordinator::finished);
        saver.start(jobFor(m_dir.filePath("no/such/folder/b.png")));
        QVERIFY(finished.wait(10000));
        const SaveOutcome outcome = finished.first().first().value<SaveOutcome>();
        QVERIFY(!outcome.ok);
        QVERIFY(!outcome.error.isEmpty());
        QVERIFY(!saver.saving());
    }

    void aNullPictureHasNothingToSave()
    {
        SaveCoordinator saver;
        QSignalSpy finished(&saver, &SaveCoordinator::finished);
        SaveJob job;
        job.path = m_dir.filePath("c.png");
        saver.start(job);
        QVERIFY(finished.wait(10000));
        QVERIFY(!finished.first().first().value<SaveOutcome>().ok);
        QVERIFY(!QFileInfo::exists(job.path));
    }

    void saveAfterSaveRunsOneAtATimeInOrder()
    {
        SaveCoordinator saver;
        QSignalSpy finished(&saver, &SaveCoordinator::finished);
        saver.start(jobFor(m_dir.filePath("d1.png"), Qt::red));
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);
        saver.start(jobFor(m_dir.filePath("d2.png"), Qt::blue));
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 10000);
        QCOMPARE(finished.at(0).first().value<SaveOutcome>().path, m_dir.filePath("d1.png"));
        QCOMPARE(finished.at(1).first().value<SaveOutcome>().path, m_dir.filePath("d2.png"));
    }

    // Closing the program while the file is being written: the write is allowed to finish.
    void destroyingWhileSavingLeavesACompleteFile()
    {
        const QString path = m_dir.filePath("e.png");
        {
            SaveCoordinator saver;
            SaveJob job = jobFor(path, Qt::green);
            job.original = QImage(3000, 2000, QImage::Format_RGBA8888);
            job.original.fill(Qt::green);
            saver.start(job);
        } // ~SaveCoordinator waits
        QTest::qWait(100); // the result posted to the dead object must not run
        const QImage written(path);
        QCOMPARE(written.size(), QSize(3000, 2000));
        QCOMPARE(written.pixelColor(100, 100), QColor(Qt::green));
    }

    void aBlockedRequestIsKeptUntilTaken()
    {
        SaveCoordinator saver;
        QCOMPARE(saver.takeBlocked().action, PendingAction::None);
        saver.blockOnUnsaved(PendingAction::OpenPath, QStringLiteral("x.jpg"));
        const PendingRequest request = saver.takeBlocked();
        QCOMPARE(request.action, PendingAction::OpenPath);
        QCOMPARE(request.path, QStringLiteral("x.jpg"));
        QCOMPARE(saver.takeBlocked().action, PendingAction::None); // taken once
        saver.blockOnUnsaved(PendingAction::Paste);
        QCOMPARE(saver.takeBlocked().action, PendingAction::Paste);
    }

    void theRequestWaitingForASaveIsKeptApartFromTheBlockedOne()
    {
        SaveCoordinator saver;
        saver.blockOnUnsaved(PendingAction::OpenPath, QStringLiteral("blocked.png"));
        saver.afterSave(PendingAction::Paste);
        const PendingRequest after = saver.takeAfterSave();
        QCOMPARE(after.action, PendingAction::Paste);
        QVERIFY(after.path.isEmpty());
        QCOMPARE(saver.takeAfterSave().action, PendingAction::None);
        QCOMPARE(saver.takeBlocked().path, QStringLiteral("blocked.png")); // untouched by the other
    }

    void discardApprovalIsAFlag()
    {
        SaveCoordinator saver;
        QVERIFY(!saver.discardApproved());
        saver.setDiscardApproved(true);
        QVERIFY(saver.discardApproved());
        saver.setDiscardApproved(false);
        QVERIFY(!saver.discardApproved());
    }
};

QTEST_MAIN(TestSaveCoordinator)
#include "test_save_coordinator.moc"
