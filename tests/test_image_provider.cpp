#include "ImageProvider.h"
#include <QtTest>

// The provider that hands the current picture to QML, and its display proxy: a reduced copy served when the URL
// carries "side=N".
class TestImageProvider : public QObject {
    Q_OBJECT

    static QImage picture(int w, int h, QColor c)
    {
        QImage img(w, h, QImage::Format_RGBA8888);
        img.fill(c);
        return img;
    }

private slots:
    void reducedToKeepsTheShapeAndNeverEnlarges()
    {
        const QImage big = picture(8000, 4000, Qt::red);
        const QImage small = ImageProvider::reducedTo(big, 1000);
        QCOMPARE(small.size(), QSize(1000, 500));
        QCOMPARE(small.pixelColor(10, 10), QColor(Qt::red));
        QCOMPARE(ImageProvider::reducedTo(big, 9000).size(), big.size()); // already small enough: untouched
        QCOMPARE(ImageProvider::reducedTo(big, 0).size(), big.size());
        QVERIFY(ImageProvider::reducedTo(QImage(), 100).isNull());
    }

    void theCurrentPictureIsServedWholeUnlessAProxyIsAsked()
    {
        ImageProvider provider;
        provider.setImage(picture(3000, 2000, Qt::blue));
        QSize size;
        QCOMPARE(provider.requestImage("current?rev=1", &size, QSize()).size(), QSize(3000, 2000));
        const QImage proxy = provider.requestImage("current?rev=1&side=600", &size, QSize());
        QCOMPARE(proxy.size(), QSize(600, 400));
        QCOMPARE(size, QSize(600, 400)); // what was actually delivered
        // the picture itself is still intact for everything else (edits, readouts, saving)
        QCOMPARE(provider.pixelAt(2999, 1999), QColor(Qt::blue));
    }

    void aSideBiggerThanThePictureGivesThePicture()
    {
        ImageProvider provider;
        provider.setImage(picture(300, 200, Qt::green));
        QSize size;
        QCOMPARE(provider.requestImage("current?rev=1&side=4096", &size, QSize()).size(), QSize(300, 200));
    }

    void aNewPictureDropsTheOldProxy()
    {
        ImageProvider provider;
        QSize size;
        provider.setImage(picture(4000, 2000, Qt::red));
        QCOMPARE(provider.requestImage("current?rev=1&side=500", &size, QSize()).pixelColor(5, 5), QColor(Qt::red));
        // same side asked again for another picture: it must not get the previous one's copy
        provider.setImage(picture(4000, 2000, Qt::blue));
        const QImage next = provider.requestImage("current?rev=2&side=500", &size, QSize());
        QCOMPARE(next.size(), QSize(500, 250));
        QCOMPARE(next.pixelColor(5, 5), QColor(Qt::blue));
    }

    void anotherSideIsMadeAgain()
    {
        ImageProvider provider;
        QSize size;
        provider.setImage(picture(4000, 2000, Qt::red));
        QCOMPARE(provider.requestImage("current?rev=1&side=500", &size, QSize()).size(), QSize(500, 250));
        QCOMPARE(provider.requestImage("current?rev=1&side=1000", &size, QSize()).size(), QSize(1000, 500));
    }

    // "Comparar" shows the as-opened picture: it follows the same proxy, with a copy of its own.
    void theOriginalHasItsOwnProxy()
    {
        ImageProvider provider;
        provider.setImage(picture(4000, 2000, Qt::red));
        provider.setOriginalImage(picture(4000, 2000, Qt::green));
        QSize size;
        QCOMPARE(provider.requestImage("original?rev=1", &size, QSize()).size(), QSize(4000, 2000));
        const QImage orig = provider.requestImage("original?rev=1&side=500", &size, QSize());
        QCOMPARE(orig.size(), QSize(500, 250));
        QCOMPARE(orig.pixelColor(5, 5), QColor(Qt::green)); // not the current picture's copy
        const QImage cur = provider.requestImage("current?rev=1&side=500", &size, QSize());
        QCOMPARE(cur.pixelColor(5, 5), QColor(Qt::red));
        provider.setOriginalImage(picture(4000, 2000, Qt::blue)); // a new original drops the old copy
        QCOMPARE(provider.requestImage("original?rev=2&side=500", &size, QSize()).pixelColor(5, 5), QColor(Qt::blue));
    }
};

QTEST_MAIN(TestImageProvider)
#include "test_image_provider.moc"
