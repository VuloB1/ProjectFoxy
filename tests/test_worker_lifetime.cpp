#include "BatchExporter.h"
#include "LensController.h"
#include <QtTest>

// The background workers of the secondary tools hold a pointer to their owner: destroying the owner while the
// worker runs (closing the program during a batch export) must wait for it instead of leaving it to call a dead object.
class TestWorkerLifetime : public QObject {
    Q_OBJECT

private slots:
    void destroyingABatchExporterDuringTheBatchIsSafe()
    {
        QTemporaryDir src, dst;
        QStringList files;
        for (int i = 0; i < 30; ++i) {
            QImage img(600, 400, QImage::Format_RGBA8888);
            img.fill(QColor(i * 8, 90, 200));
            const QString path = src.filePath(QStringLiteral("p%1.png").arg(i));
            QVERIFY(img.save(path));
            files << path;
        }
        {
            BatchExporter exporter;
            exporter.start(files, QUrl::fromLocalFile(dst.path()), QStringLiteral("jpg"), 300, 80);
            QVERIFY(exporter.running());
        } // ~BatchExporter: cancel, then wait
        QTest::qWait(200);
        QVERIFY(QDir(dst.path()).entryList(QDir::Files).size() < files.size()); // it was stopped, not run to the end
    }

    void destroyingALensControllerWhileItLoadsIsSafe()
    {
        {
            LensController lens;
            lens.load();
        }
        QTest::qWait(100);
    }
};

QTEST_MAIN(TestWorkerLifetime)
#include "test_worker_lifetime.moc"
