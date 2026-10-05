#include <QtTest>
#include <QBuffer>
#include <QImageReader>
#include "anim/Anim.h"
#include "decoders/AnimatedDecoder.h"

#include <webp/demux.h>

#include <cmath>
#include <random>

using namespace core::anim;

namespace {

class VectorSource : public FrameSource {
public:
    VectorSource(std::vector<QImage> images, std::vector<int> delays) : m_images(std::move(images)), m_delays(std::move(delays)) {}
    int count() const override { return int(m_images.size()); }
    QSize size() const override { return m_images.front().size(); }
    QImage frame(int i, int *delayMs) override
    {
        if (delayMs)
            *delayMs = m_delays[size_t(i)];
        return m_images[size_t(i)];
    }

private:
    std::vector<QImage> m_images;
    std::vector<int> m_delays;
};

QImage solid(int w, int h, QColor c)
{
    QImage img(w, h, QImage::Format_RGBA8888);
    img.fill(c);
    return img;
}

// A square of colour `c` on a dark background at (x, y).
QImage square(int w, int h, int x, int y, QColor c)
{
    QImage img = solid(w, h, QColor(20, 20, 40));
    for (int j = y; j < y + 10; ++j)
        for (int i = x; i < x + 10; ++i)
            img.setPixelColor(i, j, c);
    return img;
}

QImage gradient(int w, int h)
{
    QImage img(w, h, QImage::Format_RGBA8888);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            img.setPixelColor(x, y, QColor(x * 255 / (w - 1), y * 255 / (h - 1), (x + y) * 255 / (w + h - 2)));
    return img;
}

bool near(QRgb c, QColor want, int tol)
{
    return std::abs(qRed(c) - want.red()) <= tol && std::abs(qGreen(c) - want.green()) <= tol && std::abs(qBlue(c) - want.blue()) <= tol;
}

double meanAbsDiff(const QImage &a, const QImage &b)
{
    double sum = 0;
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x) {
            const QRgb p = a.pixel(x, y), q = b.pixel(x, y);
            sum += std::abs(qRed(p) - qRed(q)) + std::abs(qGreen(p) - qGreen(q)) + std::abs(qBlue(p) - qBlue(q));
        }
    return sum / (3.0 * a.width() * a.height());
}

QByteArray gifBytes(FrameSource &src, const GifOptions &o)
{
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    QString error;
    if (!writeGif(src, o, buf, nullptr, &error))
        qWarning("writeGif: %s", qPrintable(error));
    return bytes;
}

// Every frame of a GIF as Qt's own reader composes them, with their delays.
struct Decoded {
    std::vector<QImage> frames;
    std::vector<int> delays;
};
Decoded readGif(const QByteArray &bytes)
{
    Decoded d;
    QBuffer buf;
    buf.setData(bytes);
    buf.open(QIODevice::ReadOnly);
    QImageReader reader(&buf, "gif");
    while (reader.canRead()) {
        const QImage img = reader.read();
        if (img.isNull())
            break;
        d.frames.push_back(img.convertToFormat(QImage::Format_ARGB32));
        d.delays.push_back(reader.nextImageDelay());
    }
    return d;
}

} // namespace

class TestAnim : public QObject {
    Q_OBJECT

private slots:
    // ---- GIF -------------------------------------------------------------------------------------------------

    void gifKeepsFlatColoursAndDelays()
    {
        VectorSource src({solid(40, 30, Qt::red), solid(40, 30, Qt::green), solid(40, 30, Qt::blue)}, {100, 200, 300});
        const QByteArray bytes = gifBytes(src, GifOptions{});
        QVERIFY(bytes.startsWith("GIF89a"));
        QVERIFY(bytes.endsWith(char(0x3B)));
        const Decoded d = readGif(bytes);
        QCOMPARE(int(d.frames.size()), 3);
        QCOMPARE(d.delays, (std::vector<int>{100, 200, 300}));
        QVERIFY(near(d.frames[0].pixel(5, 5), Qt::red, 2));
        QVERIFY(near(d.frames[1].pixel(20, 15), Qt::green, 2));
        QVERIFY(near(d.frames[2].pixel(39, 29), Qt::blue, 2));
        QCOMPARE(d.frames[0].size(), QSize(40, 30));
    }

    void gifWithOnlyTheChangedPartIsSmallerAndLooksTheSame()
    {
        std::vector<QImage> frames;
        for (int i = 0; i < 6; ++i)
            frames.push_back(square(120, 90, 10 + i * 15, 40, QColor(240, 200, 30)));
        VectorSource a(frames, std::vector<int>(6, 80)), b(frames, std::vector<int>(6, 80));
        GifOptions opt;
        opt.dither = false;
        opt.optimize = true;
        const QByteArray small = gifBytes(a, opt);
        opt.optimize = false;
        const QByteArray big = gifBytes(b, opt);
        QVERIFY2(small.size() < big.size(), qPrintable(QString("%1 vs %2").arg(small.size()).arg(big.size())));
        const Decoded d = readGif(small);
        QCOMPARE(int(d.frames.size()), 6);
        for (int i = 0; i < 6; ++i) {
            QVERIFY2(meanAbsDiff(d.frames[size_t(i)], frames[size_t(i)].convertToFormat(QImage::Format_ARGB32)) < 0.5, qPrintable(QString::number(i)));
            QVERIFY(near(d.frames[size_t(i)].pixel(12 + i * 15, 44), QColor(240, 200, 30), 2));
        }
    }

    void aGradientKeepsItsShapeWithAndWithoutDithering()
    {
        const QImage g = gradient(128, 96);
        for (bool dither : {false, true}) {
            VectorSource src({g, g}, {100, 100});
            GifOptions opt;
            opt.dither = dither;
            const Decoded d = readGif(gifBytes(src, opt));
            QCOMPARE(int(d.frames.size()), 2);
            const double err = meanAbsDiff(d.frames[0], g.convertToFormat(QImage::Format_ARGB32));
            QVERIFY2(err < 14.0, qPrintable(QString("dither %1: %2").arg(dither).arg(err)));
        }
    }

    void theLzwSurvivesWhiteNoiseAndTableResets()
    {
        // 200 distinct colours in random order: the palette can hold them exactly, and the noise fills the
        // LZW table over and over (12-bit codes, several resets) - what comes out must be what went in.
        std::mt19937 rng(7);
        std::vector<QColor> colours;
        for (int i = 0; i < 200; ++i)
            colours.push_back(QColor((i * 37) % 256, (i * 91 + 13) % 256, (i * 151 + 77) % 256));
        QImage noise(300, 300, QImage::Format_RGBA8888);
        for (int y = 0; y < 300; ++y)
            for (int x = 0; x < 300; ++x)
                noise.setPixelColor(x, y, colours[rng() % colours.size()]);
        VectorSource src({noise}, {100});
        GifOptions opt;
        opt.dither = false;
        opt.colors = 256;
        const Decoded d = readGif(gifBytes(src, opt));
        QCOMPARE(int(d.frames.size()), 1);
        int wrong = 0;
        for (int y = 0; y < 300; ++y)
            for (int x = 0; x < 300; ++x)
                wrong += !near(d.frames[0].pixel(x, y), noise.pixelColor(x, y), 8); // the 5-bit grid may merge colours <= 8 apart
        QVERIFY2(wrong < 300 * 300 / 50, qPrintable(QString::number(wrong)));
        // and a picture of only two flat colours must be exact (tiny alphabets and long runs)
        QImage two = solid(500, 400, Qt::white);
        for (int y = 100; y < 300; ++y)
            for (int x = 100; x < 400; ++x)
                two.setPixelColor(x, y, Qt::black);
        VectorSource s2({two}, {100});
        const Decoded d2 = readGif(gifBytes(s2, opt));
        QCOMPARE(int(d2.frames.size()), 1);
        QCOMPARE(meanAbsDiff(d2.frames[0], two.convertToFormat(QImage::Format_ARGB32)), 0.0);
    }

    void gifKeepsTransparency()
    {
        QImage f(60, 40, QImage::Format_RGBA8888);
        f.fill(QColor(0, 0, 0, 0));
        for (int y = 10; y < 30; ++y)
            for (int x = 10; x < 50; ++x)
                f.setPixelColor(x, y, QColor(200, 30, 30));
        VectorSource src({f, f}, {100, 100});
        const Decoded d = readGif(gifBytes(src, GifOptions{}));
        QCOMPARE(int(d.frames.size()), 2);
        for (const QImage &frame : d.frames) {
            QCOMPARE(qAlpha(frame.pixel(2, 2)), 0);
            QCOMPARE(qAlpha(frame.pixel(30, 20)), 255);
            QVERIFY(near(frame.pixel(30, 20), QColor(200, 30, 30), 3));
        }
    }

    void localPalettesGiveEachFrameItsOwnColours()
    {
        // two frames with completely different colours: with one shared palette 255 colours are split
        // between them; per frame, each gets all of them
        QImage a(100, 100, QImage::Format_RGBA8888), b(100, 100, QImage::Format_RGBA8888);
        for (int y = 0; y < 100; ++y)
            for (int x = 0; x < 100; ++x) {
                a.setPixelColor(x, y, QColor(x * 2, y * 2, 0));
                b.setPixelColor(x, y, QColor(0, 255 - x * 2, 255 - y * 2));
            }
        GifOptions shared, local;
        shared.dither = local.dither = false;
        local.localPalettes = true;
        VectorSource s1({a, b}, {100, 100}), s2({a, b}, {100, 100});
        const Decoded ds = readGif(gifBytes(s1, shared)), dl = readGif(gifBytes(s2, local));
        QCOMPARE(int(dl.frames.size()), 2);
        const double errShared = meanAbsDiff(ds.frames[0], a.convertToFormat(QImage::Format_ARGB32)) + meanAbsDiff(ds.frames[1], b.convertToFormat(QImage::Format_ARGB32));
        const double errLocal = meanAbsDiff(dl.frames[0], a.convertToFormat(QImage::Format_ARGB32)) + meanAbsDiff(dl.frames[1], b.convertToFormat(QImage::Format_ARGB32));
        QVERIFY2(errLocal < errShared, qPrintable(QString("%1 vs %2").arg(errLocal).arg(errShared)));
    }

    void gifLoopsAndFewColours()
    {
        VectorSource a({solid(8, 8, Qt::red), solid(8, 8, Qt::blue)}, {50, 50});
        GifOptions endless;
        QVERIFY(gifBytes(a, endless).contains("NETSCAPE2.0"));
        VectorSource b({solid(8, 8, Qt::red), solid(8, 8, Qt::blue)}, {50, 50});
        GifOptions once;
        once.loops = 1;
        QVERIFY(!gifBytes(b, once).contains("NETSCAPE2.0")); // plays once: no loop extension
        // 4 colours still make a valid, readable file
        VectorSource c({gradient(32, 32), gradient(32, 32)}, {50, 50});
        GifOptions few;
        few.colors = 4;
        QCOMPARE(int(readGif(gifBytes(c, few)).frames.size()), 2);
    }

    void thePaletteStandsForTheColoursThatAreThere()
    {
        const auto two = quantizePalette({solid(10, 10, Qt::red), solid(10, 10, Qt::blue)}, 8);
        QCOMPARE(int(two.size()), 2);
        const auto many = quantizePalette({gradient(100, 100)}, 64);
        QVERIFY(many.size() <= 64 && many.size() > 16);
        const auto transparentOnly = quantizePalette({solid(10, 10, QColor(5, 5, 5, 0))}, 16);
        QCOMPARE(int(transparentOnly.size()), 1); // nothing opaque: a placeholder
    }

    // ---- APNG ------------------------------------------------------------------------------------------------

    void apngRoundTripsExactly()
    {
        std::vector<QImage> frames;
        for (int i = 0; i < 5; ++i)
            frames.push_back(square(90, 60, 5 + i * 12, 20 + i * 3, QColor(250 - i * 30, 40 + i * 40, 90)));
        QTemporaryDir dir;
        for (bool optimize : {true, false}) {
            VectorSource src(frames, {100, 150, 200, 250, 300});
            QFile file(dir.filePath(optimize ? "a.png" : "b.png"));
            QVERIFY(file.open(QIODevice::WriteOnly));
            ApngOptions opt;
            opt.optimize = optimize;
            QString error;
            QVERIFY2(writeApng(src, opt, file, nullptr, &error), qPrintable(error));
            file.close();

            core::AnimatedDecoder decoder;
            QVERIFY(decoder.canDecode(file.fileName()));
            const core::DecodeResult r = decoder.decode(file.fileName());
            QVERIFY2(r.ok, qPrintable(r.error));
            QCOMPARE(int(r.frames.size()), 5);
            QCOMPARE(r.frameDelaysMs, (QVector<int>{100, 150, 200, 250, 300}));
            for (int i = 0; i < 5; ++i)
                QCOMPARE(meanAbsDiff(r.frames[i].convertToFormat(QImage::Format_ARGB32), frames[size_t(i)].convertToFormat(QImage::Format_ARGB32)), 0.0);
        }
        QVERIFY(QFileInfo(dir.filePath("a.png")).size() < QFileInfo(dir.filePath("b.png")).size());
    }

    void apngKeepsTransparency()
    {
        QImage f(30, 20, QImage::Format_RGBA8888);
        f.fill(QColor(0, 0, 0, 0));
        f.setPixelColor(5, 5, QColor(255, 0, 0, 128));
        QImage g = f;
        g.setPixelColor(6, 5, QColor(0, 255, 0, 255));
        QTemporaryDir dir;
        QFile file(dir.filePath("t.png"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        VectorSource src({f, g}, {100, 100});
        QVERIFY(writeApng(src, ApngOptions{}, file));
        file.close();
        core::AnimatedDecoder decoder;
        const core::DecodeResult r = decoder.decode(file.fileName());
        QVERIFY(r.ok && r.frames.size() == 2);
        QCOMPARE(qAlpha(r.frames[1].convertToFormat(QImage::Format_ARGB32).pixel(5, 5)), 128);
        QCOMPARE(qAlpha(r.frames[1].convertToFormat(QImage::Format_ARGB32).pixel(6, 5)), 255);
        QCOMPARE(qAlpha(r.frames[1].convertToFormat(QImage::Format_ARGB32).pixel(0, 0)), 0);
    }

    // ---- WebP ------------------------------------------------------------------------------------------------

    void webpLosslessRoundTrips()
    {
        std::vector<QImage> frames;
        for (int i = 0; i < 4; ++i)
            frames.push_back(square(64, 48, 4 + i * 10, 10 + i * 4, QColor(250 - i * 50, 60 + i * 50, 120)));
        VectorSource src(frames, {100, 120, 140, 160});
        QByteArray bytes;
        QBuffer buf(&bytes);
        buf.open(QIODevice::WriteOnly);
        WebpOptions opt;
        opt.lossless = true;
        QString error;
        QVERIFY2(writeWebp(src, opt, buf, nullptr, &error), qPrintable(error));
        QVERIFY(bytes.startsWith("RIFF") && bytes.mid(8, 4) == "WEBP");

        WebPData data{reinterpret_cast<const uint8_t *>(bytes.constData()), size_t(bytes.size())};
        WebPAnimDecoderOptions dopt;
        WebPAnimDecoderOptionsInit(&dopt);
        dopt.color_mode = MODE_RGBA;
        WebPAnimDecoder *dec = WebPAnimDecoderNew(&data, &dopt);
        QVERIFY(dec);
        WebPAnimInfo info;
        WebPAnimDecoderGetInfo(dec, &info);
        QCOMPARE(int(info.frame_count), 4);
        QCOMPARE(int(info.canvas_width), 64);
        int previousTs = 0, i = 0;
        std::vector<int> delays;
        while (WebPAnimDecoderHasMoreFrames(dec)) {
            uint8_t *rgba = nullptr;
            int ts = 0;
            QVERIFY(WebPAnimDecoderGetNext(dec, &rgba, &ts));
            delays.push_back(ts - previousTs);
            previousTs = ts;
            const QImage got(rgba, 64, 48, 64 * 4, QImage::Format_RGBA8888);
            QCOMPARE(meanAbsDiff(got.convertToFormat(QImage::Format_ARGB32), frames[size_t(i)].convertToFormat(QImage::Format_ARGB32)), 0.0);
            ++i;
        }
        WebPAnimDecoderDelete(dec);
        QCOMPARE(delays, (std::vector<int>{100, 120, 140, 160}));
    }

    void theViewerPlaysTheAnimatedWebpItWrites()
    {
        std::vector<QImage> frames;
        for (int i = 0; i < 3; ++i)
            frames.push_back(square(64, 48, 4 + i * 20, 10, QColor(250 - i * 80, 60 + i * 80, 120)));
        VectorSource src(frames, {100, 200, 300});
        QTemporaryDir dir;
        QFile file(dir.filePath("a.webp"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        WebpOptions opt;
        opt.lossless = true;
        QVERIFY(writeWebp(src, opt, file));
        file.close();
        core::AnimatedDecoder decoder;
        QVERIFY(decoder.canDecode(file.fileName()));
        const core::DecodeResult r = decoder.decode(file.fileName());
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(int(r.frames.size()), 3);
        QCOMPARE(r.frameDelaysMs, (QVector<int>{100, 200, 300}));
        for (int i = 0; i < 3; ++i)
            QCOMPARE(meanAbsDiff(r.frames[i].convertToFormat(QImage::Format_ARGB32), frames[size_t(i)].convertToFormat(QImage::Format_ARGB32)), 0.0);
        // a still WebP is not "animated": it goes to the ordinary decoder
        QImage still(16, 16, QImage::Format_RGBA8888);
        still.fill(Qt::red);
        QVERIFY(still.save(dir.filePath("still.webp"), "WEBP") || true);
        if (QFileInfo::exists(dir.filePath("still.webp")))
            QVERIFY(!decoder.canDecode(dir.filePath("still.webp")));
    }

    void webpLossyIsSmallerAndClose()
    {
        const QImage g = gradient(96, 72);
        std::vector<QImage> frames{g, g.mirrored(true, false)};
        QByteArray lossy, lossless;
        for (bool l : {false, true}) {
            VectorSource src(frames, {100, 100});
            QBuffer buf(l ? &lossless : &lossy);
            buf.open(QIODevice::WriteOnly);
            WebpOptions opt;
            opt.lossless = l;
            opt.quality = 60;
            QVERIFY(writeWebp(src, opt, buf));
        }
        QVERIFY2(lossy.size() < lossless.size(), qPrintable(QString("%1 vs %2").arg(lossy.size()).arg(lossless.size())));
    }

    void exportsCanBeCancelled()
    {
        std::vector<QImage> frames(10, gradient(64, 48));
        for (int kind = 0; kind < 3; ++kind) {
            VectorSource src(frames, std::vector<int>(10, 100));
            QByteArray bytes;
            QBuffer buf(&bytes);
            buf.open(QIODevice::WriteOnly);
            int calls = 0;
            auto stopSoon = [&](int, int) { return ++calls < 3; };
            bool ok = true;
            if (kind == 0) ok = writeGif(src, GifOptions{}, buf, stopSoon);
            else if (kind == 1) ok = writeApng(src, ApngOptions{}, buf, stopSoon);
            else ok = writeWebp(src, WebpOptions{}, buf, stopSoon);
            QVERIFY2(!ok, qPrintable(QString::number(kind)));
            QVERIFY(calls < 12);
        }
    }

    // ---- building the frames -----------------------------------------------------------------------------------

    void picturesAreFittedToTheCanvas()
    {
        Settings s;
        s.size = QSize(100, 100);
        s.background = QColor(255, 0, 0);
        const QImage wide = solid(200, 100, QColor(0, 0, 255));
        const QImage contain = fitToCanvas(wide, s);
        QCOMPARE(contain.size(), QSize(100, 100));
        QVERIFY(near(contain.pixel(50, 5), QColor(255, 0, 0), 2));   // the background above...
        QVERIFY(near(contain.pixel(50, 50), QColor(0, 0, 255), 2));  // ...the picture in the middle...
        QVERIFY(near(contain.pixel(50, 95), QColor(255, 0, 0), 2));  // ...the background below
        s.fit = Fit::Cover;
        const QImage cover = fitToCanvas(wide, s);
        QVERIFY(near(cover.pixel(2, 2), QColor(0, 0, 255), 2) && near(cover.pixel(97, 97), QColor(0, 0, 255), 2));
        s.fit = Fit::Stretch;
        QVERIFY(near(fitToCanvas(wide, s).pixel(2, 97), QColor(0, 0, 255), 2));
        s.fit = Fit::Contain;
        s.transparentBackground = true;
        QCOMPARE(qAlpha(fitToCanvas(wide, s).pixel(50, 5)), 0);
    }

    void thePlanFollowsTheSettings()
    {
        Settings s;
        const std::vector<int> hold{500, 1000, 250};
        auto plan = buildPlan(hold, s);
        QCOMPARE(int(plan.size()), 3);
        QCOMPARE(plan[1].delayMs, 1000);
        QVERIFY(plan[0].isStill());

        s.transition = Transition::Fade;
        s.transitionMs = 400;
        s.transitionSteps = 4;
        plan = buildPlan(hold, s);
        QCOMPARE(int(plan.size()), 3 + 3 * 4); // each picture, and a transition after each (the last wraps round)
        QCOMPARE(plan[1].a, 0);
        QCOMPARE(plan[1].b, 1);
        QCOMPARE(plan[1].delayMs, 100);
        QVERIFY(plan[1].t > 0 && plan[1].t < plan[2].t);
        QCOMPARE(plan.back().b, 0);
        s.transitionOnLoop = false;
        QCOMPARE(int(buildPlan(hold, s).size()), 3 + 2 * 4);

        s.transition = Transition::None;
        s.reverse = true;
        plan = buildPlan(hold, s);
        QCOMPARE(plan[0].a, 2);
        QCOMPARE(plan[2].a, 0);
        s.reverse = false;
        s.pingPong = true;
        plan = buildPlan({100, 100, 100, 100}, s);
        QCOMPARE(int(plan.size()), 6); // 0 1 2 3 2 1
        QCOMPARE(plan[4].a, 2);
        QCOMPARE(plan[5].a, 1);
        s.pingPong = false;
        s.speed = 2.0;
        QCOMPARE(buildPlan({400}, s)[0].delayMs, 200);
        s.speed = 10.0;
        QCOMPARE(buildPlan({100}, s)[0].delayMs, 20); // never shorter than a browser will play
        QVERIFY(buildPlan({}, s).empty());
    }

    void transitionsMixThePictures()
    {
        Settings s;
        s.size = QSize(100, 60);
        const QImage a = solid(100, 60, QColor(200, 0, 0)), b = solid(100, 60, QColor(0, 0, 200));
        PlanStep mid{0, 1, 0.5, 50};
        s.transition = Transition::Fade;
        const QImage fade = renderStep(a, b, mid, s);
        QVERIFY(near(fade.pixel(50, 30), QColor(100, 0, 100), 3));
        PlanStep still{0, 0, 0.0, 50};
        QVERIFY(near(renderStep(a, b, still, s).pixel(50, 30), QColor(200, 0, 0), 0));

        s.transition = Transition::SlideLeft; // at the half-way point: the old on the left half, the new on the right
        const QImage slide = renderStep(a, b, mid, s);
        QVERIFY(near(slide.pixel(10, 30), QColor(200, 0, 0), 2));
        QVERIFY(near(slide.pixel(90, 30), QColor(0, 0, 200), 2));
        s.transition = Transition::SlideRight;
        const QImage right = renderStep(a, b, mid, s);
        QVERIFY(near(right.pixel(10, 30), QColor(0, 0, 200), 2));
        QVERIFY(near(right.pixel(90, 30), QColor(200, 0, 0), 2));
        s.transition = Transition::SlideUp;
        const QImage up = renderStep(a, b, mid, s);
        QVERIFY(near(up.pixel(50, 5), QColor(200, 0, 0), 2));
        QVERIFY(near(up.pixel(50, 55), QColor(0, 0, 200), 2));
        s.transition = Transition::Zoom;
        const QImage zoom = renderStep(a, b, mid, s);
        QVERIFY(qRed(zoom.pixel(50, 30)) > 20 && qBlue(zoom.pixel(50, 30)) > 20);
        // transparency survives a fade
        s.transition = Transition::Fade;
        const QImage clear = solid(100, 60, QColor(0, 0, 0, 0));
        QCOMPARE(qAlpha(renderStep(clear, clear, mid, s).pixel(5, 5)), 0);
        const QImage half = renderStep(a, clear, mid, s);
        QVERIFY(qAlpha(half.pixel(5, 5)) > 100 && qAlpha(half.pixel(5, 5)) < 155);
        QVERIFY(near(half.pixel(5, 5), QColor(200, 0, 0), 3)); // the colour is the picture's, not darkened by the empty one
    }

    void aFrameStyleChangesTheEffectAndWritesTheText()
    {
        QImage base(200, 120, QImage::Format_RGBA8888);
        base.fill(QColor(200, 40, 40));

        FrameStyle plain;
        QVERIFY(plain.isPlain());
        QVERIFY(plain.signature().isEmpty());
        QCOMPARE(decorate(base, plain), base); // nothing to do: the same picture

        // an effect
        FrameStyle fx;
        fx.effectId = "negative";
        QVERIFY(effectUsableOnFrames("negative"));
        QVERIFY(!effectUsableOnFrames("nothing-like-this"));
        QVERIFY(!effectUsableOnFrames("lens"));     // driven by its own tool
        QVERIFY(!effectUsableOnFrames("stretch"));  // changes the size
        const QImage inverted = decorate(base, fx);
        QCOMPARE(inverted.size(), base.size());
        QVERIFY(near(inverted.pixel(100, 60), QColor(55, 215, 215), 3));
        fx.effectMix = 0.5; // half way between the two
        QVERIFY(near(decorate(base, fx).pixel(100, 60), QColor(127, 127, 127), 4));
        fx.effectMix = 0.0;
        QCOMPARE(decorate(base, fx), base);

        // text: pixels of its colour appear near the spot asked for, and nowhere far from it
        FrameStyle tx;
        tx.text.text = "Hola";
        tx.text.x = 0.5;
        tx.text.y = 0.5;
        tx.text.size = 30.0;
        tx.text.color = QColor(0, 255, 0);
        tx.text.outline = false;
        const QImage lettered = decorate(base, tx);
        int green = 0, farGreen = 0;
        for (int y = 0; y < lettered.height(); ++y)
            for (int x = 0; x < lettered.width(); ++x) {
                const QColor c = lettered.pixelColor(x, y);
                if (c.green() > 200 && c.red() < 60) {
                    ++green;
                    if (qAbs(y - 60) > 40)
                        ++farGreen;
                }
            }
        QVERIFY(green > 100);
        QCOMPARE(farGreen, 0);
        QVERIFY(lettered.pixelColor(2, 2) == base.pixelColor(2, 2)); // the rest is untouched

        // moved to the top it is no longer in the middle
        tx.text.y = 0.1;
        const QImage top = decorate(base, tx);
        int topGreen = 0, midGreen = 0;
        for (int y = 0; y < top.height(); ++y)
            for (int x = 0; x < top.width(); ++x)
                if (top.pixelColor(x, y).green() > 200 && top.pixelColor(x, y).red() < 60)
                    (y < 40 ? topGreen : midGreen)++;
        QVERIFY(topGreen > 100);
        QCOMPARE(midGreen, 0);

        // a text too wide for the picture is shrunk to fit, never cut
        tx.text.text = "Un texto muy muy largo para una imagen tan chica";
        tx.text.y = 0.5;
        tx.text.size = 40.0;
        const QImage fitted = decorate(base, tx);
        for (int y = 0; y < fitted.height(); ++y) {
            QVERIFY(fitted.pixelColor(0, y).green() < 200 || fitted.pixelColor(0, y).red() > 60);
            QVERIFY(fitted.pixelColor(fitted.width() - 1, y).green() < 200 || fitted.pixelColor(fitted.width() - 1, y).red() > 60);
        }

        // equal styles share a signature, different ones do not
        FrameStyle a = tx, b = tx;
        QCOMPARE(a.signature(), b.signature());
        b.text.size = 12.0;
        QVERIFY(a.signature() != b.signature());
    }

    void transparentPicturesShowTheBackgroundAndNotTheirHiddenColour()
    {
        // fully transparent pixels that hide pure green, and a half transparent ring around them
        QImage src(200, 150, QImage::Format_RGBA8888);
        src.fill(QColor(200, 40, 40, 255));
        for (int y = 0; y < src.height(); ++y)
            for (int x = 0; x < src.width(); ++x) {
                const int d2 = (x - 100) * (x - 100) + (y - 75) * (y - 75);
                if (d2 < 45 * 45)
                    src.setPixelColor(x, y, QColor(0, 255, 0, 0));
                else if (d2 < 55 * 55)
                    src.setPixelColor(x, y, QColor(200, 40, 40, 100));
            }
        Settings st;
        st.size = QSize(400, 300);
        st.background = QColor(10, 20, 30);
        const QImage fitted = fitToCanvas(src, st);
        const QColor hole = fitted.pixelColor(200, 150);
        QVERIFY2(qAbs(hole.red() - 10) <= 2 && qAbs(hole.green() - 20) <= 2 && qAbs(hole.blue() - 30) <= 2 && hole.alpha() == 255,
                 qPrintable(QString("hole is %1,%2,%3,%4").arg(hole.red()).arg(hole.green()).arg(hole.blue()).arg(hole.alpha())));
        // the ring is the picture's colour mixed with the background, never greener than either
        for (int x = 200 + 95; x < 200 + 115; ++x) {
            const QColor c = fitted.pixelColor(x, 150);
            QVERIFY2(c.green() <= 45, qPrintable(QString("x=%1 green %2").arg(x).arg(c.green())));
        }
        // and through the GIF writer
        VectorSource frames({fitted}, {100});
        QBuffer out;
        out.open(QIODevice::WriteOnly);
        QVERIFY(writeGif(frames, GifOptions{}, out));
        QImage back;
        QVERIFY(back.loadFromData(out.data(), "GIF"));
        const QColor gifHole = back.pixelColor(200, 150);
        QVERIFY2(gifHole.green() < 60 && gifHole.alpha() == 255, qPrintable(QString("gif hole is %1,%2,%3,%4").arg(gifHole.red()).arg(gifHole.green()).arg(gifHole.blue()).arg(gifHole.alpha())));
    }

    void aNeutralDarkGradientStaysNeutralInTheGif()
    {
        // a soft grey shadow over black (what a transparent picture's soft edge looks like once it is flattened),
        // in a GIF whose palette is shared with a colourful picture
        QImage shade(320, 240, QImage::Format_RGBA8888);
        for (int y = 0; y < shade.height(); ++y)
            for (int x = 0; x < shade.width(); ++x) {
                const double d = std::hypot(x - 160.0, y - 120.0) / 120.0;
                const int v = int(std::clamp(1.0 - d, 0.0, 1.0) * 90.0);
                shade.setPixelColor(x, y, QColor(v, v, v));
            }
        QImage colourful(320, 240, QImage::Format_RGBA8888);
        for (int y = 0; y < colourful.height(); ++y)
            for (int x = 0; x < colourful.width(); ++x)
                colourful.setPixelColor(x, y, QColor::fromHsv((x * 360 / 320 + y) % 360, 200, 120 + (y % 120)));
        for (bool dither : {true, false}) {
            VectorSource frames({shade, colourful}, {100, 100});
            GifOptions o;
            o.dither = dither;
            QBuffer out;
            out.open(QIODevice::WriteOnly);
            QVERIFY(writeGif(frames, o, out));
            QImage back;
            QVERIFY(back.loadFromData(out.data(), "GIF"));
            int worst = 0;
            double meanErr = 0.0;
            for (int y = 0; y < back.height(); ++y)
                for (int x = 0; x < back.width(); ++x) {
                    const QColor c = back.pixelColor(x, y), w = shade.pixelColor(x, y);
                    worst = std::max(worst, std::max({qAbs(c.red() - c.green()), qAbs(c.green() - c.blue()), qAbs(c.red() - c.blue())}));
                    meanErr += qAbs(c.red() - w.red()) + qAbs(c.green() - w.green()) + qAbs(c.blue() - w.blue());
                }
            meanErr /= back.width() * back.height() * 3.0;
            qInfo() << "dither" << dither << "worst chroma" << worst << "mean error" << meanErr;
            QVERIFY2(worst <= 8, qPrintable(QString("dither=%1: worst chroma %2").arg(dither).arg(worst)));
            QVERIFY2(meanErr < 3.0, qPrintable(QString("dither=%1: mean error %2").arg(dither).arg(meanErr)));
        }
    }
};

QTEST_MAIN(TestAnim)
#include "test_anim.moc"
