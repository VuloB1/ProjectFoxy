// Characterisation of what AppController does today for the live edits (Ajustes, Filtros), the derived pictures it
// caches (structural result, histograms, look thumbnails) and the ORDER of the signals QML listens to. These rules
// were not covered before; they are fixed here so that the pieces can be moved out of AppController (see
// docs/PLAN_APPCONTROLLER.md) without changing what the interface sees.
#include "AppController.h"
#include "FolderModel.h"
#include "ImageProvider.h"
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

namespace {

// Deterministic busy picture, uneven enough that a crop or an adjustment visibly changes its histogram.
QImage busyImage(int w, int h)
{
    QImage img(w, h, QImage::Format_RGBA8888);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            img.setPixelColor(x, y, QColor((x * 7 + y * 13) % 256, (x * x + y * 3) % 256, (x * y) % 256));
    return img;
}

QVariantMap point(double x, double y)
{
    return {{QStringLiteral("x"), x}, {QStringLiteral("y"), y}};
}

} // namespace

class TestAppControllerEdits : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        QVERIFY(m_dir.isValid());
        m_provider = new ImageProvider;
        m_folder = new FolderModel;
        m_ctl = new AppController(m_provider, m_folder);
    }

    void cleanup()
    {
        delete m_ctl;
        delete m_folder;
        delete m_provider;
        for (const QString &name : QDir(m_dir.path()).entryList(QDir::Files))
            QFile::remove(m_dir.filePath(name));
    }

    // ================================================================ Ajustes

    void adjustParametersAreClampedToTheirRange()
    {
        open(file("a.png", busyImage(40, 30)));
        m_ctl->setAdjustParam(QStringLiteral("exposure"), 5);
        QCOMPARE(adjust(QStringLiteral("exposure")), 1.0);
        m_ctl->setAdjustParam(QStringLiteral("exposure"), -5);
        QCOMPARE(adjust(QStringLiteral("exposure")), -1.0);
        m_ctl->setAdjustParam(QStringLiteral("sharpness"), -0.5); // these two start at 0, not at -1
        QCOMPARE(adjust(QStringLiteral("sharpness")), 0.0);
        m_ctl->setAdjustParam(QStringLiteral("grain"), 3);
        QCOMPARE(adjust(QStringLiteral("grain")), 1.0);
    }

    void anUnknownParameterIsIgnored()
    {
        open(file("a.png", busyImage(40, 30)));
        QSignalSpy live(m_ctl, &AppController::liveAdjustChanged);
        const QVariantMap before = m_ctl->adjustMap();
        m_ctl->setAdjustParam(QStringLiteral("no-such-slider"), 0.5);
        QCOMPARE(m_ctl->adjustMap(), before);
        QCOMPARE(live.count(), 0);
        QVERIFY(!m_ctl->canUndoEdit());
    }

    void nothingIsAdjustedWithoutAPicture()
    {
        m_ctl->setAdjustParam(QStringLiteral("exposure"), 0.5);
        m_ctl->setLevels(0, 0.2, 1.0, 0.9);
        m_ctl->resetAdjust();
        QCOMPARE(adjust(QStringLiteral("exposure")), 0.0);
        QVERIFY(!m_ctl->canUndoEdit());
    }

    void aSliderMoveIsAnUndoableEditThatTurnsTheGradeStageOn()
    {
        open(file("a.png", busyImage(40, 30)));
        QVERIFY(!m_ctl->gradeActive());
        QVERIFY(!m_ctl->hasEdits());
        const QString lutBefore = m_ctl->adjustLutSource();

        m_ctl->setAdjustParam(QStringLiteral("exposure"), 0.3);
        QVERIFY(m_ctl->gradeActive());
        QVERIFY(m_ctl->hasEdits());
        QVERIFY(m_ctl->canUndoEdit());
        QVERIFY(m_ctl->isDirty());
        QVERIFY(m_ctl->adjustLutSource() != lutBefore); // the tone table is a new one (the canvas reloads it)

        m_ctl->undoEdit();
        QCOMPARE(adjust(QStringLiteral("exposure")), 0.0);
        QVERIFY(!m_ctl->gradeActive());
        QVERIFY(!m_ctl->isDirty());
        m_ctl->redoEdit();
        QCOMPARE(adjust(QStringLiteral("exposure")), 0.3);
    }

    // Clarity, sharpness, vignette and grain belong to the Detail stage, which always runs: they never need Grade.
    void detailControlsDoNotTurnTheGradeStageOn()
    {
        open(file("a.png", busyImage(40, 30)));
        m_ctl->setAdjustParam(QStringLiteral("sharpness"), 0.5);
        QVERIFY(m_ctl->hasEdits());
        QVERIFY(!m_ctl->gradeActive());
        m_ctl->setAdjustParam(QStringLiteral("contrast"), 0.2);
        QVERIFY(m_ctl->gradeActive());
    }

    void movesOfTheSameControlMergeIntoOneUndoStep()
    {
        open(file("a.png", busyImage(40, 30)));
        m_ctl->setAdjustParam(QStringLiteral("exposure"), 0.1);
        m_ctl->setAdjustParam(QStringLiteral("exposure"), 0.2);
        m_ctl->setAdjustParam(QStringLiteral("exposure"), 0.3);
        m_ctl->undoEdit(); // one entry for the whole drag
        QCOMPARE(adjust(QStringLiteral("exposure")), 0.0);
        QVERIFY(!m_ctl->canUndoEdit());
    }

    void movesOfDifferentControlsAreSeparateUndoSteps()
    {
        open(file("a.png", busyImage(40, 30)));
        m_ctl->setAdjustParam(QStringLiteral("exposure"), 0.3);
        m_ctl->setAdjustParam(QStringLiteral("contrast"), 0.4);
        m_ctl->undoEdit();
        QCOMPARE(adjust(QStringLiteral("contrast")), 0.0);
        QCOMPARE(adjust(QStringLiteral("exposure")), 0.3); // the first one is still there
        m_ctl->undoEdit();
        QCOMPARE(adjust(QStringLiteral("exposure")), 0.0);
    }

    void grainAndNegativeOptionsAreKept()
    {
        open(file("a.png", busyImage(40, 30)));
        m_ctl->setGrainType(9);
        QCOMPARE(m_ctl->adjustMap().value(QStringLiteral("grainType")).toInt(), 3); // clamped to 0..3
        m_ctl->setGrainType(-2);
        QCOMPARE(m_ctl->adjustMap().value(QStringLiteral("grainType")).toInt(), 0);
        m_ctl->setGrainMono(true);
        QVERIFY(m_ctl->adjustMap().value(QStringLiteral("grainMono")).toBool());
        m_ctl->setNegative(true);
        QVERIFY(m_ctl->adjustMap().value(QStringLiteral("negative")).toBool());
        QVERIFY(m_ctl->gradeActive()); // the negative is part of the tone stage
    }

    void levelsKeepTheirOrderAndResetPutsThemBack()
    {
        open(file("a.png", busyImage(40, 30)));
        QCOMPARE(m_ctl->adjustMap().value(QStringLiteral("levels")).toList().size(), 4); // RGB + the three channels
        m_ctl->setLevels(0, 0.5, 1.0, 0.4); // white below black: pushed above it
        const QVariantMap lv = m_ctl->adjustMap().value(QStringLiteral("levels")).toList().at(0).toMap();
        QCOMPARE(lv.value(QStringLiteral("inBlack")).toDouble(), 0.5);
        QVERIFY(lv.value(QStringLiteral("inWhite")).toDouble() >= 0.52 - 1e-9);
        m_ctl->setLevels(9, 0.1, 1.0, 0.9); // no such channel
        m_ctl->setOutputLevels(0.9, 0.2);
        QCOMPARE(adjust(QStringLiteral("outBlack")), 0.9);
        QVERIFY(adjust(QStringLiteral("outWhite")) >= 0.92 - 1e-9);

        m_ctl->resetLevels();
        QCOMPARE(adjust(QStringLiteral("outBlack")), 0.0);
        QCOMPARE(adjust(QStringLiteral("outWhite")), 1.0);
        const QVariantMap lv2 = m_ctl->adjustMap().value(QStringLiteral("levels")).toList().at(0).toMap();
        QCOMPARE(lv2.value(QStringLiteral("inBlack")).toDouble(), 0.0);
        QCOMPARE(lv2.value(QStringLiteral("inWhite")).toDouble(), 1.0);
    }

    void curvesAreSortedClampedAndAnIdentityCurveIsCleared()
    {
        open(file("a.png", busyImage(40, 30)));
        m_ctl->setCurvePoints(0, {point(0.8, 0.2), point(0.2, 0.9), point(2.0, -1.0)});
        QVariantList pts = curve(0);
        QCOMPARE(pts.size(), 3);
        QCOMPARE(pts.at(0).toMap().value(QStringLiteral("x")).toDouble(), 0.2); // sorted by x
        QCOMPARE(pts.at(2).toMap().value(QStringLiteral("x")).toDouble(), 1.0); // clamped to 0..1
        QCOMPARE(pts.at(2).toMap().value(QStringLiteral("y")).toDouble(), 0.0);

        m_ctl->setCurvePoints(0, {point(0, 0), point(1, 1)}); // the straight line is "no curve"
        QVERIFY(curve(0).isEmpty());
        m_ctl->setCurvePoints(7, {point(0.5, 0.2)}); // no such channel
        QCOMPARE(m_ctl->adjustMap().value(QStringLiteral("curves")).toList().size(), 4);

        QCOMPARE(m_ctl->curveSamples({point(0, 0), point(1, 1)}).size(), 256);
    }

    void autoAdjustKnowsItsKindsAndIsUndoable()
    {
        open(file("a.png", busyImage(40, 30)));
        m_ctl->autoAdjust(QStringLiteral("bogus"));
        QVERIFY(!m_ctl->canUndoEdit()); // an unknown kind does nothing
        for (const char *kind : {"contrast", "levels", "color"}) {
            m_ctl->autoAdjust(QString::fromLatin1(kind));
            QVERIFY2(m_ctl->canUndoEdit(), kind);
            m_ctl->undoEdit();
            QVERIFY2(!m_ctl->canUndoEdit(), kind);
        }
    }

    void resetAdjustReturnsEverythingToNeutralAndCanBeUndone()
    {
        open(file("a.png", busyImage(40, 30)));
        m_ctl->resetAdjust(); // already neutral: nothing is recorded
        QVERIFY(!m_ctl->canUndoEdit());
        m_ctl->setAdjustParam(QStringLiteral("exposure"), 0.4);
        m_ctl->setAdjustParam(QStringLiteral("saturation"), -0.3);
        m_ctl->resetAdjust();
        QCOMPARE(adjust(QStringLiteral("exposure")), 0.0);
        QCOMPARE(adjust(QStringLiteral("saturation")), 0.0);
        QVERIFY(!m_ctl->gradeActive());
        m_ctl->undoEdit();
        QCOMPARE(adjust(QStringLiteral("exposure")), 0.4);
        QCOMPARE(adjust(QStringLiteral("saturation")), -0.3);
    }

    // ================================================================ Filtros

    void thereAreLooksAndGroupsForTheList()
    {
        const QVariantList looks = m_ctl->lookList();
        QVERIFY(looks.size() > 10);
        const QVariantMap first = looks.first().toMap();
        QVERIFY(!first.value(QStringLiteral("id")).toString().isEmpty());
        QVERIFY(!first.value(QStringLiteral("name")).toString().isEmpty());
        QVERIFY(!first.value(QStringLiteral("group")).toString().isEmpty());
        QVERIFY(!m_ctl->lookGroups().isEmpty());
    }

    void aLookIsClampedAppliedAndUndoable()
    {
        open(file("a.png", busyImage(40, 30)));
        const QString id = m_ctl->lookList().first().toMap().value(QStringLiteral("id")).toString();
        QVERIFY(!m_ctl->lookActive());
        QSignalSpy filter(m_ctl, &AppController::liveFilterChanged);

        m_ctl->applyFilterPreset(id, 2.0); // above 1: clamped
        QCOMPARE(m_ctl->lookMap().value(QStringLiteral("id")).toString(), id);
        QCOMPARE(m_ctl->lookMap().value(QStringLiteral("amount")).toDouble(), 1.0);
        QVERIFY(m_ctl->lookActive());
        QVERIFY(m_ctl->hasEdits());
        QCOMPARE(filter.count(), 1);

        m_ctl->applyFilterPreset(id, 0.0); // amount 0: the look is picked but changes nothing
        QVERIFY(!m_ctl->lookActive());
        m_ctl->applyFilterPreset(QStringLiteral("no-such-look"), 0.5); // unknown = no look
        QVERIFY(m_ctl->lookMap().value(QStringLiteral("id")).toString().isEmpty());
        QVERIFY(!m_ctl->lookActive());
    }

    void undoBringsTheLookBack()
    {
        open(file("a.png", busyImage(40, 30)));
        const QString id = m_ctl->lookList().first().toMap().value(QStringLiteral("id")).toString();
        m_ctl->applyFilterPreset(id, 0.7);
        m_ctl->undoEdit();
        QVERIFY(m_ctl->lookMap().value(QStringLiteral("id")).toString().isEmpty());
        QVERIFY(!m_ctl->lookActive());
        m_ctl->redoEdit();
        QCOMPARE(m_ctl->lookMap().value(QStringLiteral("id")).toString(), id);
        QCOMPARE(m_ctl->lookMap().value(QStringLiteral("amount")).toDouble(), 0.7);
    }

    void aLookAndAnAdjustmentAreSeparateEntriesOfTheSameHistory()
    {
        open(file("a.png", busyImage(40, 30)));
        const QString id = m_ctl->lookList().first().toMap().value(QStringLiteral("id")).toString();
        m_ctl->applyFilterPreset(id, 0.7);
        m_ctl->setAdjustParam(QStringLiteral("exposure"), 0.3);
        m_ctl->undoEdit(); // the adjustment goes, the look stays
        QCOMPARE(adjust(QStringLiteral("exposure")), 0.0);
        QCOMPARE(m_ctl->lookMap().value(QStringLiteral("id")).toString(), id);
    }

    void lookThumbnailsAreMadeOncePerPictureAndAgainWhenItsPixelsChange()
    {
        open(file("a.png", busyImage(40, 30)));
        QSignalSpy thumbs(m_ctl, &AppController::lookThumbsChanged);
        const int rev0 = m_ctl->lookThumbRevision();
        m_ctl->ensureLookThumbnails();
        QCOMPARE(thumbs.count(), 1);
        QCOMPARE(m_ctl->lookThumbRevision(), rev0 + 1);
        m_ctl->ensureLookThumbnails(); // up to date: nothing
        QCOMPARE(thumbs.count(), 1);

        m_ctl->cropNormalized(0, 0, 0.5, 0.5); // the photo they are made from changed
        m_ctl->ensureLookThumbnails();
        QCOMPARE(thumbs.count(), 2);
    }

    // ================================================================ derived pictures

    void theHistogramFollowsStructuralEditsAndUndoDoesNotLeaveAStaleOne()
    {
        open(file("a.png", busyImage(60, 40)));
        const QVariantList whole = m_ctl->histogramChannels().value(QStringLiteral("luma")).toList();
        QCOMPARE(whole.size(), 256);
        QSignalSpy changed(m_ctl, &AppController::histogramChanged);

        m_ctl->cropNormalized(0, 0, 0.3, 0.3);
        QVERIFY(changed.count() >= 1);
        const QVariantList cropped = m_ctl->histogramChannels().value(QStringLiteral("luma")).toList();
        QVERIFY(cropped != whole);

        m_ctl->undoEdit();
        QCOMPARE(m_ctl->histogramChannels().value(QStringLiteral("luma")).toList(), whole);
        QCOMPARE(m_ctl->histogram().size(), 256);
    }

    void theAdjustedHistogramFollowsTheAdjustments()
    {
        open(file("a.png", busyImage(60, 40)));
        const QVariantList plain = m_ctl->adjustedHistogram().value(QStringLiteral("luma")).toList();
        QSignalSpy changed(m_ctl, &AppController::adjustedHistogramChanged);
        m_ctl->setAdjustParam(QStringLiteral("brightness"), 0.6);
        QVERIFY(changed.count() >= 1);
        QVERIFY(m_ctl->adjustedHistogram().value(QStringLiteral("luma")).toList() != plain);
        m_ctl->undoEdit();
        QCOMPARE(m_ctl->adjustedHistogram().value(QStringLiteral("luma")).toList(), plain);
    }

    void structuralEditsResizeThePictureAndUndoAndRedoWalkBack()
    {
        open(file("a.png", busyImage(60, 40)));
        QCOMPARE(m_ctl->currentImageSize(), QSize(60, 40));
        m_ctl->cropNormalized(0, 0, 0.5, 0.5);
        QCOMPARE(m_ctl->currentImageSize(), QSize(30, 20));
        m_ctl->rotateEdit90();
        QCOMPARE(m_ctl->currentImageSize(), QSize(20, 30));
        m_ctl->undoEdit();
        QCOMPARE(m_ctl->currentImageSize(), QSize(30, 20));
        m_ctl->undoEdit();
        QCOMPARE(m_ctl->currentImageSize(), QSize(60, 40));
        m_ctl->redoEdit();
        m_ctl->redoEdit();
        QCOMPARE(m_ctl->currentImageSize(), QSize(20, 30));
    }

    void pixelAtReadsThePictureAsEditedNotAsOpened()
    {
        const QImage original = busyImage(60, 40);
        open(file("a.png", original));
        const QVariantMap first = m_ctl->pixelAt(0, 0);
        QVERIFY(first.value(QStringLiteral("valid")).toBool());
        QCOMPARE(first.value(QStringLiteral("r")).toInt(), original.pixelColor(0, 0).red());

        m_ctl->flipEditHorizontal();
        const QVariantMap flipped = m_ctl->pixelAt(0, 0); // what was at the right edge
        QCOMPARE(flipped.value(QStringLiteral("r")).toInt(), original.pixelColor(59, 0).red());
        QCOMPARE(flipped.value(QStringLiteral("b")).toInt(), original.pixelColor(59, 0).blue());
        QVERIFY(!m_ctl->pixelAt(60, 0).value(QStringLiteral("valid")).toBool()); // outside
    }

    // ================================================================ the order of the signals QML listens to

    // The canvas lays the picture out from the SIZE, so the size must be announced before the source (and both
    // before the "new picture" handlers run). Fixed here: moving code must not reorder them.
    void openingAPictureAnnouncesTheSizeBeforeTheSource()
    {
        QStringList order;
        record(order);
        open(file("a.png", busyImage(60, 40)));
        const QStringList relevant = only(order, {"size", "source", "structural", "metadata", "editStack",
                                                  "liveAdjust", "liveFilter", "histogram", "newImage"});
        const QStringList expected{"size", "source", "structural", "metadata", "editStack",
                                   "liveAdjust", "liveFilter", "histogram", "newImage"};
        QCOMPARE(relevant, expected);
    }

    void aStructuralEditAnnouncesSizeSourceAndThenTheRefit()
    {
        open(file("a.png", busyImage(60, 40)));
        QStringList order;
        record(order);
        m_ctl->cropNormalized(0, 0, 0.5, 0.5);
        const QStringList relevant = only(order, {"size", "source", "structural", "editStack", "histogram"});
        QVERIFY(relevant.indexOf("size") < relevant.indexOf("source"));
        QVERIFY(relevant.indexOf("source") < relevant.indexOf("structural"));
        QVERIFY(relevant.contains("editStack"));
        QVERIFY(relevant.contains("histogram"));
    }

    // A slider move is applied by the GPU: it announces the live state, never a new picture.
    void aSliderMoveAnnouncesTheLiveStateAndNoNewPicture()
    {
        open(file("a.png", busyImage(60, 40)));
        QStringList order;
        record(order);
        m_ctl->setAdjustParam(QStringLiteral("exposure"), 0.3);
        const QStringList relevant = only(order, {"size", "source", "structural", "liveAdjust", "adjustedHistogram", "editStack"});
        QCOMPARE(relevant, (QStringList{"liveAdjust", "adjustedHistogram", "editStack"}));
    }

    void aLookAnnouncesTheLiveFilterAndNoNewPicture()
    {
        open(file("a.png", busyImage(60, 40)));
        const QString id = m_ctl->lookList().first().toMap().value(QStringLiteral("id")).toString();
        QStringList order;
        record(order);
        m_ctl->applyFilterPreset(id, 0.8);
        const QStringList relevant = only(order, {"size", "source", "structural", "liveFilter", "adjustedHistogram", "editStack"});
        QCOMPARE(relevant, (QStringList{"liveFilter", "adjustedHistogram", "editStack"}));
    }

private:
    double adjust(const QString &name) const { return m_ctl->adjustMap().value(name).toDouble(); }
    QVariantList curve(int channel) const
    {
        return m_ctl->adjustMap().value(QStringLiteral("curves")).toList().at(channel).toList();
    }

    // Names, in the order they fire, of the signals that matter to the canvas and the panels.
    void record(QStringList &into)
    {
        auto watch = [this, &into](auto signal, const char *name) {
            connect(m_ctl, signal, m_ctl, [&into, name]() { into << QLatin1String(name); });
        };
        watch(&AppController::currentImageSizeChanged, "size");
        watch(&AppController::currentSourceChanged, "source");
        watch(&AppController::structuralImageChanged, "structural");
        watch(&AppController::metadataChanged, "metadata");
        watch(&AppController::editStackChanged, "editStack");
        watch(&AppController::liveAdjustChanged, "liveAdjust");
        watch(&AppController::liveFilterChanged, "liveFilter");
        watch(&AppController::histogramChanged, "histogram");
        watch(&AppController::adjustedHistogramChanged, "adjustedHistogram");
        watch(&AppController::newImageLoaded, "newImage");
    }

    // `order` reduced to the first occurrence of each of `names`, in the order those first occurrences happened.
    static QStringList only(const QStringList &order, const QStringList &names)
    {
        QStringList result;
        for (const QString &n : order)
            if (names.contains(n) && !result.contains(n))
                result << n;
        return result;
    }

    QString file(const QString &name, const QImage &image)
    {
        const QString path = m_dir.filePath(name);
        [&] { QVERIFY(image.save(path)); }();
        return path;
    }

    void open(const QString &path)
    {
        m_ctl->openPath(path);
        QTRY_VERIFY_WITH_TIMEOUT(!m_ctl->isLoading() && !m_ctl->currentSource().isEmpty(), 5000);
        QCOMPARE(m_ctl->currentFileName(), QFileInfo(path).fileName());
    }

    QTemporaryDir m_dir;
    ImageProvider *m_provider = nullptr;
    FolderModel *m_folder = nullptr;
    AppController *m_ctl = nullptr;
};

QTEST_MAIN(TestAppControllerEdits)
#include "test_app_controller_edits.moc"
