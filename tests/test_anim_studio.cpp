#include <QtTest>
#include <QBuffer>
#include <QImageReader>
#include "AnimStudio.h"
#include "PaneImageProvider.h"
#include "decoders/AnimatedDecoder.h"

namespace {

// Three frames of flat colour, each held longer than the one before.
class ColourSource : public core::anim::FrameSource {
public:
    int count() const override { return 3; }
    QSize size() const override { return QSize(40, 30); }
    QImage frame(int i, int *delayMs) override
    {
        *delayMs = 100 * (i + 1);
        QImage img(40, 30, QImage::Format_RGBA8888);
        img.fill(i == 0 ? QColor(Qt::red) : i == 1 ? QColor(Qt::green) : QColor(Qt::blue));
        return img;
    }
};

} // namespace

// The animation studio: the list of pictures, the options and their limits, the frames of the preview and
// writing the three formats in the background.
class TestAnimStudio : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

    QString makePng(const QString &name, QColor c, int w = 100, int h = 60)
    {
        QImage img(w, h, QImage::Format_RGBA8888);
        img.fill(c);
        const QString path = m_dir.filePath(name);
        img.save(path);
        return path;
    }

    QStringList names(AnimStudio &s)
    {
        QStringList out;
        for (int i = 0; i < s.rowCount(); ++i)
            out << s.data(s.index(i), AnimStudio::NameRole).toString();
        return out;
    }

    // Writes the animation and waits for it.
    bool exportAndWait(AnimStudio &s, const QString &path, QString *error = nullptr, QString *finalPath = nullptr)
    {
        QSignalSpy done(&s, &AnimStudio::exportFinished);
        if (!s.exportTo(QUrl::fromLocalFile(path)))
            return false;
        if (!done.wait(30000))
            return false;
        const auto args = done.first();
        if (error)
            *error = args.at(3).toString();
        if (finalPath)
            *finalPath = args.at(1).toString();
        return args.at(0).toBool();
    }

private slots:
    void initTestCase() { QVERIFY(m_dir.isValid()); }

    void picturesAreListedAndTheSizeFollowsTheFirst()
    {
        PaneImageStore store;
        AnimStudio s(&store);
        QCOMPARE(s.rowCount(), 0);
        QCOMPARE(s.planCount(), 0);
        const QStringList paths{makePng("a.png", Qt::red), makePng("b.png", Qt::green), makePng("c.png", Qt::blue)};
        QCOMPARE(s.addPaths(paths), 3);
        QCOMPARE(s.count(), 3);
        QCOMPARE(names(s), (QStringList{"a.png", "b.png", "c.png"}));
        // 100 x 60 -> a 640 px wide animation of the same proportions
        QCOMPARE(s.options().value("width").toInt(), 640);
        QCOMPARE(s.options().value("height").toInt(), 384);
        QCOMPARE(s.planCount(), 3);
        QCOMPARE(s.totalMs(), 3000);
        QCOMPARE(s.addPaths({m_dir.filePath("missing.png"), m_dir.filePath("notes")}), 0);
    }

    void aspectLockAndLimits()
    {
        PaneImageStore store;
        AnimStudio s(&store);
        s.addPaths({makePng("a.png", Qt::red)});
        s.setOption("width", 320);
        QCOMPARE(s.options().value("height").toInt(), 192); // locked: follows
        s.setOption("height", 96);
        QCOMPARE(s.options().value("width").toInt(), 160);
        s.setOption("lockAspect", false);
        s.setOption("width", 500);
        QCOMPARE(s.options().value("height").toInt(), 96);  // free now
        s.setOption("width", 99999);
        QCOMPARE(s.options().value("width").toInt(), 4096);
        s.setOption("width", 1);
        QCOMPARE(s.options().value("width").toInt(), 16);
        s.setOption("gifColors", 1);
        QCOMPARE(s.options().value("gifColors").toInt(), 2);
        s.setOption("speed", 100.0);
        QCOMPARE(s.options().value("speed").toDouble(), 10.0);
        s.setOption("noSuchOption", 3);
        QVERIFY(!s.options().contains("noSuchOption"));
        // turning the lock back on puts the proportions back
        s.setOption("width", 300);
        s.setOption("lockAspect", true);
        QCOMPARE(s.options().value("height").toInt(), 180);
        // once the size was chosen by hand, adding pictures does not change it
        s.addPaths({makePng("b.png", Qt::blue, 50, 100)});
        QCOMPARE(s.options().value("width").toInt(), 300);
    }

    void editingTheList()
    {
        PaneImageStore store;
        AnimStudio s(&store);
        s.addPaths({makePng("a.png", Qt::red), makePng("b.png", Qt::green), makePng("c.png", Qt::blue)});
        QSignalSpy plan(&s, &AnimStudio::planChanged);
        s.move(0, 2);
        QCOMPARE(names(s), (QStringList{"b.png", "c.png", "a.png"}));
        s.move(2, 0);
        QCOMPARE(names(s), (QStringList{"a.png", "b.png", "c.png"}));
        s.duplicateAt(1);
        QCOMPARE(names(s), (QStringList{"a.png", "b.png", "b.png", "c.png"}));
        QVERIFY(s.data(s.index(1), AnimStudio::IdRole) != s.data(s.index(2), AnimStudio::IdRole)); // each one has its own
        s.removeAt(1);
        s.reverseOrder();
        QCOMPARE(names(s), (QStringList{"c.png", "b.png", "a.png"}));
        s.setHold(0, 5);
        QCOMPARE(s.holdAt(0), 20);   // never shorter than a player will show
        s.setHold(0, 250);
        QCOMPARE(s.holdAt(0), 250);
        s.setAllHold(400);
        QCOMPARE(s.totalMs(), 1200);
        QCOMPARE(s.planIndexOf(2), 2);
        QCOMPARE(s.planIndexOf(7), -1);
        QVERIFY(plan.count() >= 6);
        s.clear();
        QCOMPARE(s.count(), 0);
        QCOMPARE(s.planCount(), 0);
        s.removeAt(3); // nothing happens
    }

    void theFramesFollowTheOptions()
    {
        PaneImageStore store;
        AnimStudio s(&store);
        s.addPaths({makePng("a.png", Qt::red), makePng("b.png", Qt::green), makePng("c.png", Qt::blue)});
        s.setOption("transition", 1);        // fade
        s.setOption("transitionMs", 400);
        s.setOption("transitionSteps", 4);
        QCOMPARE(s.planCount(), 3 + 3 * 4);
        QCOMPARE(s.totalMs(), 3000 + 3 * 400);
        s.setOption("transitionOnLoop", false);
        QCOMPARE(s.planCount(), 3 + 2 * 4);
        s.setOption("transition", 0);
        s.setOption("pingPong", true);
        QCOMPARE(s.planCount(), 4);          // a b c b
        s.setOption("speed", 2.0);
        QCOMPARE(s.totalMs(), 2000);
        QCOMPARE(s.planDelay(0), 500);
        QCOMPARE(s.planDelay(99), 100);
    }

    void thePreviewIsADownsizedCanvas()
    {
        PaneImageStore store;
        AnimStudio s(&store);
        s.addPaths({makePng("a.png", Qt::red, 200, 120), makePng("b.png", Qt::blue, 200, 120)});
        QCOMPARE(s.options().value("width").toInt(), 640);
        const QImage f = s.previewFrame(0);
        QCOMPARE(f.size(), QSize(480, 288)); // the same proportions, 480 px along the long side
        QCOMPARE(QColor(f.pixel(240, 144)).red(), 255);
        QVERIFY(s.previewFrame(5).isNull());
        s.setOption("transition", 1);
        QVERIFY(s.planCount() > 2);
        const QImage mid = s.previewFrame(3); // the middle of the six steps of the fade between red and blue
        QVERIFY(QColor(mid.pixel(240, 144)).red() > 20 && QColor(mid.pixel(240, 144)).blue() > 20);
        const QImage thumb = s.thumbnail(s.data(s.index(0), AnimStudio::IdRole).toInt(), 64);
        QVERIFY(!thumb.isNull() && std::max(thumb.width(), thumb.height()) == 64);
        QVERIFY(s.thumbnail(99999, 64).isNull());
    }

    void writesAllThreeFormats()
    {
        PaneImageStore store;
        AnimStudio s(&store);
        s.addPaths({makePng("a.png", Qt::red), makePng("b.png", Qt::green), makePng("c.png", Qt::blue)});
        s.setOption("width", 120);
        s.setOption("transition", 1);
        s.setOption("transitionSteps", 2);
        const int frames = s.planCount();
        QCOMPARE(frames, 3 + 3 * 2);

        // GIF: the suffix is added when it is missing
        QString error, written;
        s.setOption("format", 0);
        QVERIFY2(exportAndWait(s, m_dir.filePath("out_gif"), &error, &written), qPrintable(error));
        QCOMPARE(QFileInfo(written).fileName(), QString("out_gif.gif"));
        {
            QImageReader r(written, "gif");
            int n = 0;
            while (r.canRead() && !r.read().isNull())
                ++n;
            QCOMPARE(n, frames);
        }
        QVERIFY(!s.exporting());
        QCOMPARE(s.progress(), 1.0);

        // APNG
        s.setOption("format", 1);
        QVERIFY2(exportAndWait(s, m_dir.filePath("out_apng.png"), &error, &written), qPrintable(error));
        core::AnimatedDecoder decoder;
        QVERIFY(decoder.canDecode(written));
        QCOMPARE(int(decoder.decode(written).frames.size()), frames);

        // WebP
        s.setOption("format", 2);
        s.setOption("webpLossless", true);
        QVERIFY2(exportAndWait(s, m_dir.filePath("out_webp.webp"), &error, &written), qPrintable(error));
        QFile f(written);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray head = f.read(16);
        QVERIFY(head.startsWith("RIFF") && head.mid(8, 4) == "WEBP");
    }

    void anExportCanBeCancelledAndLeavesNoFile()
    {
        PaneImageStore store;
        AnimStudio s(&store);
        QStringList paths;
        for (int i = 0; i < 20; ++i)
            paths << makePng(QString("many%1.png").arg(i), QColor(i * 12, 100, 255 - i * 12), 400, 300);
        s.addPaths(paths);
        s.setOption("transition", 1);
        s.setOption("transitionSteps", 8);
        s.setOption("width", 400);
        const QString path = m_dir.filePath("cancelled.gif");
        QSignalSpy done(&s, &AnimStudio::exportFinished);
        QVERIFY(s.exportTo(QUrl::fromLocalFile(path)));
        QVERIFY(s.exporting());
        QVERIFY(!s.exportTo(QUrl::fromLocalFile(path))); // one at a time
        s.cancelExport();
        QVERIFY(done.wait(30000));
        QVERIFY(!done.first().at(0).toBool());
        QVERIFY(!QFileInfo::exists(path));
        QVERIFY(!s.exporting());
    }

    void exportingNothingIsRefused()
    {
        PaneImageStore store;
        AnimStudio s(&store);
        QVERIFY(!s.exportTo(QUrl::fromLocalFile(m_dir.filePath("none.gif"))));
    }

    void anAnimationIsTakenApartIntoItsFrames()
    {
        // an animated GIF written by the encoder itself, with three different delays
        ColourSource src;
        const QString path = m_dir.filePath("anim.gif");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(core::anim::writeGif(src, core::anim::GifOptions{}, file));
        file.close();

        PaneImageStore store;
        AnimStudio s(&store);
        QCOMPARE(s.addPaths({path}), 3);
        QCOMPARE(s.count(), 3);
        QCOMPARE(s.holdAt(0), 100);
        QCOMPARE(s.holdAt(1), 200);
        QCOMPARE(s.holdAt(2), 300);
        QVERIFY(s.data(s.index(0), AnimStudio::FromAnimationRole).toBool());
        const QImage f1 = s.previewFrame(1);
        QVERIFY(!f1.isNull());
        QVERIFY(QColor(f1.pixel(f1.width() / 2, f1.height() / 2)).green() > 200);
        QCOMPARE(s.options().value("width").toInt(), 640); // 40 x 30 -> 640 x 480
        QCOMPARE(s.options().value("height").toInt(), 480);
    }

    void eachPictureCanHaveItsOwnEffectAndText()
    {
        PaneImageStore store;
        AnimStudio s(&store);
        s.addPaths({makePng("e0.png", QColor(200, 40, 40)), makePng("e1.png", QColor(40, 200, 40))});
        QCOMPARE(s.count(), 2);
        QVERIFY(!s.index(0, 0).data(AnimStudio::StyledRole).toBool());

        const QVariantList effects = s.frameEffects();
        QVERIFY(effects.size() > 10);
        for (const QVariant &e : effects) {
            QVERIFY(e.toMap().value("id").toString() != "lens");
            QVERIFY(e.toMap().value("id").toString() != "frame");
        }
        QVERIFY(!s.fontFamilies().isEmpty());

        const QImage before0 = s.previewFrame(0), before1 = s.previewFrame(1);
        const int rev = s.revision();
        s.setStyleValue(0, "effectId", "negative");
        QVERIFY(s.revision() > rev);
        QVERIFY(s.index(0, 0).data(AnimStudio::StyledRole).toBool());
        QVERIFY(!s.index(1, 0).data(AnimStudio::StyledRole).toBool()); // only that picture
        QCOMPARE(s.styleOf(0).value("effectId").toString(), QString("negative"));
        QCOMPARE(s.styleOf(0).value("effectMix").toDouble(), 100.0);
        const QImage after0 = s.previewFrame(0);
        QVERIFY(after0.pixel(after0.width() / 2, after0.height() / 2) != before0.pixel(before0.width() / 2, before0.height() / 2));
        QCOMPARE(s.previewFrame(1), before1);

        // an effect that cannot work on a frame is refused
        s.setStyleValue(1, "effectId", "lens");
        QVERIFY(s.styleOf(1).value("effectId").toString().isEmpty());
        // limits
        s.setStyleValue(0, "effectMix", 500);
        QCOMPARE(s.styleOf(0).value("effectMix").toDouble(), 100.0);
        s.setStyleValue(0, "textSize", 0.0);
        QCOMPARE(s.styleOf(0).value("textSize").toDouble(), 1.0);

        // text, then copied to all and cleared
        s.setStyleValue(0, "text", "Hola");
        s.setStyleValue(0, "color", 0x00FF00);
        s.setStyleValue(0, "outline", false);
        s.setStyleValue(0, "textY", 50);
        s.copyStyleToAll(0);
        QCOMPARE(s.styleOf(1).value("text").toString(), QString("Hola"));
        QCOMPARE(s.styleOf(1).value("effectId").toString(), QString("negative"));
        QVERIFY(s.index(1, 0).data(AnimStudio::StyledRole).toBool());
        s.clearStyle(1);
        QVERIFY(!s.index(1, 0).data(AnimStudio::StyledRole).toBool());
        QVERIFY(s.styleOf(1).value("text").toString().isEmpty());

        // a duplicate keeps the style
        s.duplicateAt(0);
        QCOMPARE(s.styleOf(1).value("text").toString(), QString("Hola"));

        // the exported frames have it too: the written GIF's first frame is not the plain picture
        const QString out = m_dir.filePath("styled.png");
        s.setOption("format", 1);
        QSignalSpy done(&s, &AnimStudio::exportFinished);
        QVERIFY(s.exportTo(QUrl::fromLocalFile(out)));
        QVERIFY(done.wait(30000));
        QVERIFY(done.first().at(0).toBool());
        QImage written;
        QVERIFY(written.load(out));
        QVERIFY(written.pixelColor(written.width() / 8, written.height() / 2) != QColor(200, 40, 40)); // the effect is in the file
    }
};

QTEST_MAIN(TestAnimStudio)
#include "test_anim_studio.moc"
