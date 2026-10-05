#include <QtTest>
#include "PaneImageProvider.h"

#include <thread>
#include <vector>

// The pictures of the "Varias imágenes" view: decoded like the main viewer does (EXIF turn applied, sRGB),
// remembered, and with their real size known once decoded.
class TestPaneImages : public QObject {
    Q_OBJECT

private slots:
    void aDecodedPictureComesBackAndItsSizeBecomesKnown()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QImage src(200, 100, QImage::Format_RGB32);
        src.fill(QColor(30, 120, 220));
        const QString path = dir.filePath("wide.png");
        QVERIFY(src.save(path));

        PaneImageStore store;
        QSignalSpy known(&store, &PaneImageStore::sourceSizeKnown);
        QVERIFY(!store.sourceSize(path).isValid()); // nothing is known before it is decoded

        QString error;
        const QImage full = store.image(path, 0, &error);
        QVERIFY2(!full.isNull(), qPrintable(error));
        QCOMPARE(full.size(), QSize(200, 100));
        QCOMPARE(store.sourceSize(path), QSize(200, 100));
        QTRY_COMPARE(known.count(), 1);
        QCOMPARE(known.first().first().toString(), path);

        // a smaller copy never exceeds the file's own size, and the real size stays what it is
        const QImage small = store.image(path, 64);
        QVERIFY(!small.isNull());
        QVERIFY(small.width() <= 200 && small.height() <= 100);
        QCOMPARE(store.sourceSize(path), QSize(200, 100));
        QCOMPARE(known.count(), 1); // announced only when it is news
    }

    void askingAgainDoesNotDecodeAgain()
    {
        QTemporaryDir dir;
        QImage src(120, 80, QImage::Format_RGB32);
        src.fill(Qt::red);
        const QString path = dir.filePath("a.png");
        QVERIFY(src.save(path));

        PaneImageStore store;
        const QImage first = store.image(path, 0);
        const QImage second = store.image(path, 0);
        QVERIFY(!first.isNull());
        QCOMPARE(first.cacheKey(), second.cacheKey()); // the very same pixels, shared
    }

    void theExifTurnIsApplied()
    {
        // 32 x 48 on disk with orientation 6: it is a 48 x 32 picture
        PaneImageStore store;
        const QImage image = store.image(QStringLiteral(FIXTURES_DIR "/exif_orient_6.jpg"), 0);
        QVERIFY(!image.isNull());
        QCOMPARE(image.size(), QSize(48, 32));
        QCOMPARE(store.sourceSize(QStringLiteral(FIXTURES_DIR "/exif_orient_6.jpg")), QSize(48, 32));
    }

    void aFileThatIsNotAPictureFails()
    {
        QTemporaryDir dir;
        QFile f(dir.filePath("notes.txt"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("hello");
        f.close();

        PaneImageStore store;
        QString error;
        QVERIFY(store.image(f.fileName(), 0, &error).isNull());
        QVERIFY(!error.isEmpty());
        QVERIFY(store.image(dir.filePath("missing.png"), 0).isNull());
    }

    void severalThreadsAtOnceAreSafe()
    {
        QTemporaryDir dir;
        QStringList paths;
        for (int i = 0; i < 4; ++i) {
            QImage src(160 + i, 90, QImage::Format_RGB32);
            src.fill(QColor(i * 50, 100, 200));
            paths << dir.filePath(QString("p%1.png").arg(i));
            QVERIFY(src.save(paths.last()));
        }
        PaneImageStore store;
        QAtomicInt ok = 0;
        std::vector<std::thread> threads;
        for (int round = 0; round < 3; ++round)
            for (const QString &p : paths)
                threads.emplace_back([&store, &ok, p, round]() { if (!store.image(p, round % 2 ? 0 : 100).isNull()) ok.ref(); });
        for (std::thread &t : threads)
            t.join();
        QCOMPARE(int(ok), 12);
        for (int i = 0; i < 4; ++i)
            QCOMPARE(store.sourceSize(paths[i]), QSize(160 + i, 90));
    }
};

QTEST_MAIN(TestPaneImages)
#include "test_pane_images.moc"
