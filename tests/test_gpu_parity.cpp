// CPU / GPU parity: the live preview (Grade.frag + Detail.frag, drawn by Direct3D 11)
// against the file the same settings would write (AdjustMath.cpp on the CPU).
//
// The scene (gpu_parity/ParityScene.qml) is the preview chain of ImageCanvas.qml; the
// settings go in through the real AppController; the "file" side is what
// AppController::saveEditedAs() writes. The two are compared in PREMULTIPLIED RGBA,
// which is what a screen actually holds, so a pixel's weight is its own alpha.
//
// Skips itself (QSKIP) when no Direct3D 11 device can be created.
#include "AppController.h"
#include "FolderModel.h"
#include "ImageProvider.h"
#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickView>
#include <QTemporaryDir>
#include <QtTest>
#include <cmath>

namespace {

struct Diff {
    int maxAll = 0;     // largest channel difference anywhere (0..255)
    double meanAll = 0; // mean channel difference over every pixel
    int maxSolid = 0;   // ...over pixels with alpha 255 only
    int maxTranslucent = 0; // ...over pixels with alpha 1..254
    int translucentPixels = 0;
};

Diff compare(const QImage &gpuIn, const QImage &cpuIn)
{
    const QImage gpu = gpuIn.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    const QImage cpu = cpuIn.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    Diff d;
    double sum = 0;
    for (int y = 0; y < cpu.height(); ++y) {
        const uchar *a = gpu.constScanLine(y);
        const uchar *b = cpu.constScanLine(y);
        for (int x = 0; x < cpu.width(); ++x) {
            const int alpha = b[x * 4 + 3];
            for (int c = 0; c < 4; ++c) {
                const int diff = std::abs(int(a[x * 4 + c]) - int(b[x * 4 + c]));
                sum += diff;
                d.maxAll = std::max(d.maxAll, diff);
                if (alpha == 255)
                    d.maxSolid = std::max(d.maxSolid, diff);
                else if (alpha > 0)
                    d.maxTranslucent = std::max(d.maxTranslucent, diff);
            }
            if (alpha > 0 && alpha < 255)
                ++d.translucentPixels;
        }
    }
    d.meanAll = sum / (double(cpu.width()) * cpu.height() * 4);
    return d;
}

// 128x96 test pictures. Even height: the grain's per-row pattern is only comparable
// that way (see the noise notes in DESARROLLO.md).
QImage pictureWithAlpha(bool translucent)
{
    constexpr int W = 128, H = 96;
    QImage img(W, H, QImage::Format_RGBA8888);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const QColor c = QColor::fromHsv((x * 360 / W + y * 3) % 360, 90 + (y * 150) / H, 60 + (x * 190) / W);
            // alpha ramps 8..255 along x; a few hard edges inside so sharpen/clarity have something to bite
            int alpha = translucent ? 8 + (x * 247) / (W - 1) : 255;
            if (translucent && ((x / 16 + y / 16) % 4 == 0))
                alpha = std::min(255, alpha + 60);
            img.setPixelColor(x, y, QColor(c.red(), c.green(), c.blue(), alpha));
        }
    }
    return img;
}

// Largest accepted channel difference (0..255) per setting. Plain settings agree to a
// couple of levels. Every stage of the preview is an 8-bit texture, and a stage whose
// rounding differs by one level from the CPU's gets that difference multiplied by
// sharpen/clarity at hard edges - hence the room for those and for the combination.
int toleranceFor(const QByteArray &name, bool translucent)
{
    if (name == "combination") {
#ifdef Q_OS_WIN
        return translucent ? 17 : 12;
#else
        // Mesa's software rasteriser (llvmpipe, what a CI runner has) rounds each 8-bit stage slightly differently
        // from Direct3D 11 and from a real GPU: it reached 18 here, while a real GPU on Mesa stays at 17.
        return translucent ? 20 : 12;
#endif
    }
    if (name == "colourSharpness")
        return translucent ? 12 : 5;
    if (name == "sharpness")
        return translucent ? 9 : 3;
    if (name == "claritySharpness" || name == "colourClarity" || name == "lookClarity" || name == "detailFinish")
        return translucent ? 6 : 3;
    return 3;
}

} // namespace

class TestGpuParity : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        m_provider = new ImageProvider;
        m_folder = new FolderModel;
        m_ctl = new AppController(m_provider, m_folder);

        m_view = new QQuickView;
        m_view->engine()->addImageProvider(QStringLiteral("viewer"), m_provider); // the engine owns it now
        m_view->rootContext()->setContextProperty(QStringLiteral("appController"), m_ctl);
        m_view->setColor(Qt::transparent);
        m_view->setResizeMode(QQuickView::SizeRootObjectToView);
        m_view->resize(128, 96);
        m_view->setSource(QUrl::fromLocalFile(QStringLiteral(PARITY_SCENE)));
        if (m_view->status() != QQuickView::Ready) {
            QStringList errors;
            for (const auto &e : m_view->errors())
                errors << e.toString();
            QFAIL(qPrintable(QStringLiteral("scene did not load: ") + errors.join(QLatin1Char('\n'))));
        }
        QVERIFY(m_dir.isValid());
    }

    void cleanupTestCase()
    {
        delete m_view; // the scene first: its bindings read the controller (and the engine owns the provider)
        delete m_ctl;
        delete m_folder;
    }

    void testData_data()
    {
        QTest::addColumn<bool>("translucent");
        QTest::addColumn<QStringList>("settings");
        QTest::addColumn<int>("tolerance"); // largest accepted channel difference (0..255)

        struct Row { const char *name; QStringList settings; };
        const QList<Row> rows = {
            // no stage runs at all: Detail reads the plain image
            {"untouched", {}},
            // a stage switched on and off again must leave nothing behind
            {"backToNeutral", {"exposure=0.5", "exposure=0", "look=sepia", "look="}},
            {"exposure", {"exposure=0.5"}},
            {"brightness", {"brightness=0.35"}},
            {"contrast", {"contrast=0.6"}},
            {"gamma", {"gamma=0.5"}},
            {"blacksWhites", {"blacks=0.5", "whites=-0.4"}},
            {"temperature", {"temperature=0.6", "tint=-0.3"}},
            {"shadowsHighlights", {"shadows=0.6", "highlights=-0.5"}},
            {"saturation", {"saturation=0.7"}},
            {"vibrance", {"vibrance=0.8"}},
            {"hue", {"hue=0.3"}},
            {"negative", {"negative=1"}},
            {"sharpness", {"sharpness=0.8"}},
            {"clarity", {"clarity=0.7"}},
            {"vignette", {"vignette=0.6"}},
            {"grain", {"grain=0.5", "grainType=1", "grainMono=0"}},
            // the Filtros pass: tone + split toning, and a gradient map
            {"lookCine", {"look=cine"}},
            {"lookSepia", {"look=sepia"}},
            {"lookDuotone", {"look=duo_oceano"}},
            // pieces of the combination below, to see which pairing drifts
            {"claritySharpness", {"clarity=0.4", "sharpness=0.5"}},
            {"colourClarity", {"exposure=0.3", "contrast=0.3", "saturation=0.4", "clarity=0.4"}},
            {"colourSharpness", {"exposure=0.3", "contrast=0.3", "saturation=0.4", "sharpness=0.5"}},
            {"lookClarity", {"look=cine_calido", "clarity=0.4"}},
            {"detailFinish", {"clarity=0.4", "sharpness=0.5", "vignette=0.3", "grain=0.3", "grainType=1", "grainMono=1"}},
            // everything at once, as a user would leave the sliders
            {"combination", {"look=cine_calido", "exposure=0.3", "contrast=0.3", "saturation=0.4", "clarity=0.4",
                              "sharpness=0.5", "vignette=0.3", "grain=0.3", "grainType=1", "grainMono=1"}},
        };
        for (const bool translucent : {false, true}) {
            for (const Row &r : rows) {
                const QByteArray tag = QByteArray(translucent ? "translucent/" : "opaque/") + r.name;
                const int tolerance = toleranceFor(QByteArray(r.name), translucent);
                QTest::newRow(tag.constData()) << translucent << r.settings << tolerance;
            }
        }
    }

    void testData()
    {
        QFETCH(bool, translucent);
        QFETCH(QStringList, settings);
        QFETCH(int, tolerance);

        const QString src = m_dir.filePath(QStringLiteral("src.png"));
        QVERIFY(pictureWithAlpha(translucent).save(src));
        m_ctl->openPath(src);
        QTRY_VERIFY_WITH_TIMEOUT(!m_ctl->isLoading() && !m_ctl->currentSource().isEmpty(), 5000);
        QCOMPARE(m_ctl->currentImageSize(), QSize(128, 96));
        for (const QString &s : settings) {
            const QStringList kv = s.split(QLatin1Char('='));
            const double v = kv.at(1).toDouble();
            if (kv.at(0) == QLatin1String("look")) m_ctl->applyFilterPreset(kv.at(1), 1.0);
            else if (kv.at(0) == QLatin1String("negative")) m_ctl->setNegative(v != 0);
            else if (kv.at(0) == QLatin1String("grainType")) m_ctl->setGrainType(int(v));
            else if (kv.at(0) == QLatin1String("grainMono")) m_ctl->setGrainMono(v != 0);
            else m_ctl->setAdjustParam(kv.at(0), v);
        }

        // what the file would hold
        const QString out = m_dir.filePath(QStringLiteral("cpu.png"));
        QSignalSpy saved(m_ctl, &AppController::saveFinished);
        QVERIFY(m_ctl->saveEditedAs(QUrl::fromLocalFile(out)));
        if (saved.isEmpty())
            QVERIFY(saved.wait(30000));
        QVERIFY(saved.first().first().toBool());
        const QImage cpu(out);
        QVERIFY(!cpu.isNull());

        // what the preview shows
        QTest::qWait(60);
        QImage gpu = m_view->grabWindow();
        if (gpu.isNull())
            QSKIP("no Direct3D 11 device: cannot render the preview");
        QTest::qWait(60);
        gpu = m_view->grabWindow(); // a second frame: the sources of the chain are settled
        QCOMPARE(gpu.size(), cpu.size());

        const Diff d = compare(gpu, cpu);
        const QString report = QStringLiteral("max %1 (solid %2, translucent %3 over %4 px), mean %5")
                                   .arg(d.maxAll).arg(d.maxSolid).arg(d.maxTranslucent).arg(d.translucentPixels)
                                   .arg(d.meanAll, 0, 'f', 3);
        qInfo().noquote() << report;
        QVERIFY2(d.maxAll <= tolerance, qPrintable(report));
        QVERIFY2(d.meanAll <= 1.2, qPrintable(report)); // and no widespread drift either
    }

private:
    QTemporaryDir m_dir;
    ImageProvider *m_provider = nullptr;
    FolderModel *m_folder = nullptr;
    AppController *m_ctl = nullptr;
    QQuickView *m_view = nullptr;
};

int main(int argc, char *argv[])
{
    // The preview is drawn with Direct3D 11 in the application (on Windows); do the same here.
#ifdef Q_OS_WIN
    qputenv("QSG_RHI_BACKEND", "d3d11");
#endif
    QGuiApplication app(argc, argv);
    TestGpuParity test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_gpu_parity.moc"
