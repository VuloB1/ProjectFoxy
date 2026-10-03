#include "DecoderRegistry.h"
#include "ThumbnailCache.h"
#include <QtTest>
#include <memory>

using core::DecodeResult;
using core::IImageDecoder;

namespace {

// Stand-in decoder: "*.tpng" gives a picture that is half fully transparent (and is
// "blue" under the transparent half, the way a cut-out's hidden colour often is),
// "*.solid" a plain opaque one.
class FakeDecoder : public IImageDecoder {
public:
    bool canDecode(const QString &filePath) const override
    {
        return filePath.endsWith(QLatin1String(".tpng")) || filePath.endsWith(QLatin1String(".solid"));
    }
    QStringList supportedExtensions() const override { return {QStringLiteral("tpng"), QStringLiteral("solid")}; }

    DecodeResult decode(const QString &filePath, QSize) override
    {
        DecodeResult r;
        r.ok = true;
        r.image = QImage(32, 32, QImage::Format_RGBA8888);
        const bool translucent = filePath.endsWith(QLatin1String(".tpng"));
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x)
                r.image.setPixelColor(x, y, translucent && x < 16 ? QColor(0, 0, 255, 0) : QColor(250, 20, 20, 255));
        r.sourceSize = r.image.size();
        return r;
    }
};

} // namespace

class TestThumbnailCache : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        core::DecoderRegistry::instance().registerDecoder(std::make_shared<FakeDecoder>());
    }

    // A thumbnail that was written to disk and is read back by another run (a new cache
    // over the same folder) must keep its transparency: it used to be stored as a JPEG,
    // which has no alpha, so cut-outs came back with their hidden colour showing.
    void aTransparentThumbnailKeepsItsTransparencyOnDisk()
    {
        QTemporaryDir cacheDir;
        QTemporaryDir picturesDir;
        const QString path = picturesDir.filePath(QStringLiteral("cutout.tpng"));
        QFile(path).open(QIODevice::WriteOnly); // the decoder is a fake, the file only has to exist

        {
            core::ThumbnailCache first;
            first.setDiskLocation(cacheDir.path());
            const QImage made = first.thumbnail(path, 32);
            QVERIFY(!made.isNull());
            QCOMPARE(made.pixelColor(2, 2).alpha(), 0);
        }

        core::ThumbnailCache second; // a later run: nothing in memory, only the disk
        second.setDiskLocation(cacheDir.path());
        const auto fromDisk = second.cachedThumbnail(path, 32);
        QVERIFY(fromDisk.has_value());
        QVERIFY2(fromDisk->hasAlphaChannel(), "the stored thumbnail lost its alpha channel");
        QCOMPARE(fromDisk->pixelColor(2, 2).alpha(), 0);   // still see-through on the left...
        QCOMPARE(fromDisk->pixelColor(30, 2).alpha(), 255); // ...and solid on the right
        QVERIFY(qAbs(fromDisk->pixelColor(30, 2).red() - 250) <= 3);
    }

    // Opaque pictures keep the small JPEG files.
    void anOpaqueThumbnailIsStillAJpeg()
    {
        QTemporaryDir cacheDir;
        QTemporaryDir picturesDir;
        const QString path = picturesDir.filePath(QStringLiteral("plain.solid"));
        QFile(path).open(QIODevice::WriteOnly);

        core::ThumbnailCache cache;
        cache.setDiskLocation(cacheDir.path());
        QVERIFY(!cache.thumbnail(path, 32).isNull());
        const QStringList files = QDir(cacheDir.path()).entryList(QDir::Files);
        QCOMPARE(files.size(), 1);
        QVERIFY(files.first().endsWith(QLatin1String(".jpg")));
    }
};

QTEST_MAIN(TestThumbnailCache)
#include "test_thumbnail_cache.moc"
