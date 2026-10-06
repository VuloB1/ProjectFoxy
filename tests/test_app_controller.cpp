// The rules that keep "the picture on screen", "the file it belongs to" and "what
// has been saved" telling the same story. They live in AppController, so these
// tests drive the real one (with a stand-in decoder where a load has to be slow).
#include "AppController.h"
#include "DecoderRegistry.h"
#include "FolderModel.h"
#include "ImageProvider.h"
#include "color_reference.h"
#include <QColorSpace>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

using core::DecodeResult;
using core::IImageDecoder;

namespace {

// Takes over any file whose NAME starts with "slow_" (300 ms, solid green 8x8) or
// "bad_" (fails), whatever its extension - so the files can be real PNGs on disk
// that the application would normally be able to overwrite.
class StandInDecoder : public IImageDecoder {
public:
    bool canDecode(const QString &filePath) const override
    {
        const QString name = QFileInfo(filePath).fileName();
        return name.startsWith(QLatin1String("slow_")) || name.startsWith(QLatin1String("bad_"));
    }
    QStringList supportedExtensions() const override { return {}; }

    DecodeResult decode(const QString &filePath, QSize) override
    {
        const QString name = QFileInfo(filePath).fileName();
        DecodeResult result;
        if (name.startsWith(QLatin1String("bad_"))) {
            result.error = QStringLiteral("stand-in decoder refuses %1").arg(name);
            return result;
        }
        QThread::msleep(300);
        result.ok = true;
        result.image = QImage(8, 8, QImage::Format_RGBA8888);
        result.image.fill(QColor(0, 200, 0));
        result.sourceSize = result.image.size();
        return result;
    }
};

// Deterministic busy picture (so a blur or a threshold certainly changes it).
QImage busyImage(int w, int h)
{
    QImage img(w, h, QImage::Format_RGBA8888);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            img.setPixelColor(x, y, QColor((x * 7 + y * 13) % 256, (x * x + y * 3) % 256, (x * y) % 256));
    return img;
}

QByteArray fileBytes(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

} // namespace

class TestAppController : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        core::DecoderRegistry::instance().registerDecoder(std::make_shared<StandInDecoder>());
    }

    void init()
    {
        QVERIFY(m_dir.isValid());
        m_provider = new ImageProvider;
        m_folder = new FolderModel;
        m_ctl = new AppController(m_provider, m_folder);
    }

    void cleanup()
    {
        delete m_ctl; // waits for any worker still running
        delete m_folder;
        delete m_provider;
        // every test starts with its own files
        const auto files = QDir(m_dir.path()).entryList(QDir::Files);
        for (const QString &name : files)
            QFile::remove(m_dir.filePath(name));
    }

    // ---- loading -----------------------------------------------------------

    // A load that FAILS must leave the previous picture - and the file name it belongs
    // to - exactly as they were: a later Ctrl+S must never write A's pixels into B.
    void failedLoadKeepsTheCurrentDocument()
    {
        const QString a = file("a.png", busyImage(40, 30));
        const QString bad = file("bad_c.png", busyImage(16, 16));
        const QByteArray badBefore = fileBytes(bad);
        open(a);

        m_ctl->openPath(bad);
        QTRY_VERIFY_WITH_TIMEOUT(!m_ctl->isLoading(), 5000);

        QVERIFY(!m_ctl->errorString().isEmpty());
        QCOMPARE(m_ctl->currentFileName(), QStringLiteral("a.png"));
        QVERIFY(m_ctl->canSaveInPlace());

        m_ctl->cropNormalized(0, 0, 0.5, 0.5);
        QVERIFY(saveAndWait());
        QCOMPARE(QImage(a).size(), QSize(20, 15)); // went into a.png...
        QCOMPARE(fileBytes(bad), badBefore);       // ...and not into the file that failed
    }

    // While B is still being decoded, A is still "the document".
    void fileNameChangesOnlyWhenTheNewPictureArrives()
    {
        const QString a = file("a.png", busyImage(40, 30));
        const QString b = file("slow_b.png", busyImage(16, 16));
        open(a);

        m_ctl->openPath(b);
        QVERIFY(m_ctl->isLoading());
        QCOMPARE(m_ctl->currentFileName(), QStringLiteral("a.png"));

        QTRY_VERIFY_WITH_TIMEOUT(!m_ctl->isLoading(), 5000);
        QCOMPARE(m_ctl->currentFileName(), QStringLiteral("slow_b.png"));
    }

    // The release blocker: A is edited and saved (its operations stay active, so Save
    // still looks available), the user moves on to B, and presses Ctrl+S while B is
    // still decoding. A's pixels must not be written over B.
    void saveWhileAnotherPictureLoadsCannotTouchThatFile()
    {
        const QString a = file("a.png", busyImage(40, 30));
        const QString b = file("slow_b.png", busyImage(16, 16));
        const QByteArray bBefore = fileBytes(b);
        open(a);
        m_ctl->cropNormalized(0, 0, 0.5, 0.5);
        QVERIFY(saveAndWait());
        QVERIFY(!m_ctl->isDirty());

        m_ctl->openPath(b);
        QVERIFY(m_ctl->isLoading());
        QVERIFY(!m_ctl->saveEdited());
        QVERIFY(!m_ctl->saveEditedAs(QUrl::fromLocalFile(b)));
        QVERIFY(!m_ctl->isSaving()); // refused: nothing was started
        QCOMPARE(fileBytes(b), bBefore);

        QTRY_VERIFY_WITH_TIMEOUT(!m_ctl->isLoading(), 5000);
        QCOMPARE(fileBytes(b), bBefore);
    }

    // Two loads in a row: the last one asked for is the one that stays on screen, even
    // though the first one is the last to finish.
    void newestOfTwoLoadsStaysOnScreen()
    {
        const QString slow = file("slow_a.png", busyImage(16, 16));
        const QString b = file("b.png", busyImage(40, 30));

        m_ctl->openPath(slow);
        m_ctl->openPath(b);
        QTest::qWait(900);

        QCOMPARE(m_ctl->currentFileName(), QStringLiteral("b.png"));
        QCOMPARE(m_ctl->currentImageSize(), QSize(40, 30));
    }

    // Edits made to A while B is still decoding must not vanish silently when B lands:
    // the same "unsaved changes" question is asked instead.
    void editsMadeWhileLoadingAreNotThrownAway()
    {
        const QString a = file("a.png", busyImage(40, 30));
        const QString b = file("slow_b.png", busyImage(16, 16));
        open(a);
        QSignalSpy blocked(m_ctl, &AppController::unsavedChangesBlocked);

        m_ctl->openPath(b);
        m_ctl->rotateEdit90(); // the user keeps working on A meanwhile
        QTest::qWait(700);

        QCOMPARE(blocked.count(), 1);
        QCOMPARE(m_ctl->currentFileName(), QStringLiteral("a.png"));
        QVERIFY(m_ctl->isDirty());
        QVERIFY(!m_ctl->isLoading());

        m_ctl->resolveUnsaved(QStringLiteral("discard"));
        QTRY_VERIFY_WITH_TIMEOUT(!m_ctl->isLoading(), 5000);
        QCOMPARE(m_ctl->currentFileName(), QStringLiteral("slow_b.png"));
        QVERIFY(!m_ctl->isDirty());
    }

    // ---- what "unsaved" means ---------------------------------------------

    // The screen shows something the file does not have: that is unsaved work, even
    // though no operation is active any more.
    void undoAfterSaveIsUnsaved()
    {
        open(file("a.png", busyImage(40, 30)));
        m_ctl->cropNormalized(0, 0, 0.5, 0.5);
        QVERIFY(m_ctl->isDirty());
        QVERIFY(saveAndWait());
        QVERIFY(!m_ctl->isDirty());

        m_ctl->undoEdit();
        QVERIFY(m_ctl->isDirty());
    }

    // ...and going back to exactly what was saved is clean again.
    void redoBackToTheSavedStateIsClean()
    {
        open(file("a.png", busyImage(40, 30)));
        m_ctl->cropNormalized(0, 0, 0.5, 0.5);
        QVERIFY(saveAndWait());

        m_ctl->undoEdit();
        QVERIFY(m_ctl->isDirty());
        m_ctl->redoEdit();
        QVERIFY(!m_ctl->isDirty());
    }

    // A different edit made after going back is different content, whatever the
    // history position says.
    void newBranchAfterSaveAndUndoIsUnsaved()
    {
        open(file("a.png", busyImage(40, 30)));
        m_ctl->cropNormalized(0, 0, 0.5, 0.5);
        QVERIFY(saveAndWait());

        m_ctl->undoEdit();
        m_ctl->cropNormalized(0.5, 0.5, 0.5, 0.5); // same position in the history, other crop
        QVERIFY(m_ctl->isDirty());
    }

    // Edits that cancel each other out are not "unsaved changes" (nothing to lose).
    // The canvas drops the GPU texture of a stage that changes nothing, so these flags
    // must be true exactly when the stage has an effect (Detail sliders do not count).
    void stagesReportWhetherTheyChangeAnything()
    {
        open(file("a.png", busyImage(40, 30)));
        QVERIFY(!m_ctl->lookActive());
        QVERIFY(!m_ctl->gradeActive());

        m_ctl->setAdjustParam(QStringLiteral("sharpness"), 0.5);
        m_ctl->setAdjustParam(QStringLiteral("clarity"), 0.5);
        m_ctl->setAdjustParam(QStringLiteral("vignette"), 0.5);
        m_ctl->setAdjustParam(QStringLiteral("grain"), 0.5);
        QVERIFY(!m_ctl->gradeActive());

        m_ctl->setAdjustParam(QStringLiteral("exposure"), 0.3);
        QVERIFY(m_ctl->gradeActive());
        m_ctl->setAdjustParam(QStringLiteral("exposure"), 0.0);
        QVERIFY(!m_ctl->gradeActive());
        m_ctl->setNegative(true);
        QVERIFY(m_ctl->gradeActive());
        m_ctl->setNegative(false);

        m_ctl->applyFilterPreset(QStringLiteral("sepia"), 1.0);
        QVERIFY(m_ctl->lookActive());
        m_ctl->applyFilterPreset(QStringLiteral("sepia"), 0.0);
        QVERIFY(!m_ctl->lookActive());
        m_ctl->applyFilterPreset(QStringLiteral("no_such_look"), 1.0);
        QVERIFY(!m_ctl->lookActive());
    }

    // ---- tool sessions (Recortar, Ajustes... are opened, then left by Aplicar or Cancelar) ----

    // Cancelar undoes everything the tool did - pixels and sliders - and leaves nothing to
    // redo; edits made before the tool was opened stay.
    void cancellingAToolUndoesEverythingItDid()
    {
        open(file("a.png", busyImage(40, 30)));
        m_ctl->setAdjustParam(QStringLiteral("exposure"), 0.2); // before the tool
        const QSize before = m_ctl->currentImageSize();

        m_ctl->beginToolSession();
        QVERIFY(m_ctl->toolSessionActive());
        QVERIFY(!m_ctl->toolSessionDirty());
        m_ctl->cropNormalized(0, 0, 0.5, 0.5);
        m_ctl->setAdjustParam(QStringLiteral("exposure"), 0.9);
        m_ctl->setAdjustParam(QStringLiteral("contrast"), 0.4);
        QVERIFY(m_ctl->toolSessionDirty());
        QVERIFY(m_ctl->currentImageSize() != before);

        m_ctl->endToolSession(false);
        QVERIFY(!m_ctl->toolSessionActive());
        QCOMPARE(m_ctl->currentImageSize(), before);
        QCOMPARE(m_ctl->adjustMap().value(QStringLiteral("exposure")).toDouble(), 0.2);
        QCOMPARE(m_ctl->adjustMap().value(QStringLiteral("contrast")).toDouble(), 0.0);
        QVERIFY(!m_ctl->canRedoEdit());
        QVERIFY(m_ctl->canUndoEdit()); // the exposure from before is still there to undo
    }

    void cancellingAQuarterTurnLeavesNothingToRedo()
    {
        open(file("a.png", busyImage(40, 30)));
        m_ctl->beginToolSession();
        m_ctl->rotateEdit90();
        QVERIFY(m_ctl->toolSessionDirty());
        m_ctl->endToolSession(false);
        QVERIFY(!m_ctl->canRedoEdit());
        QVERIFY(!m_ctl->canUndoEdit());
        QVERIFY(!m_ctl->hasEdits());
    }

    void applyingAToolKeepsWhatItDid()
    {
        open(file("a.png", busyImage(40, 30)));
        m_ctl->beginToolSession();
        m_ctl->cropNormalized(0, 0, 0.5, 0.5);
        const QSize cropped = m_ctl->currentImageSize();
        m_ctl->endToolSession(true);
        QCOMPARE(m_ctl->currentImageSize(), cropped);
        QVERIFY(m_ctl->canUndoEdit());
        QVERIFY(m_ctl->isDirty());
    }

    // Deshacer inside a tool stops where the tool began: the way out is Aplicar / Cancelar.
    void undoInsideAToolStopsAtItsStart()
    {
        open(file("a.png", busyImage(40, 30)));
        m_ctl->rotateEdit90(); // before the tool
        const QSize rotated = m_ctl->currentImageSize();

        m_ctl->beginToolSession();
        QVERIFY(!m_ctl->canUndoEdit());
        m_ctl->undoEdit();
        QCOMPARE(m_ctl->currentImageSize(), rotated); // refused

        m_ctl->flipEditHorizontal();
        QVERIFY(m_ctl->canUndoEdit());
        m_ctl->undoEdit();
        QVERIFY(!m_ctl->canUndoEdit());
        QCOMPARE(m_ctl->currentImageSize(), rotated);
        m_ctl->endToolSession(true);
        QVERIFY(m_ctl->canUndoEdit()); // outside the tool the whole history is reachable again
    }

    // The first slider move of a tool must not merge into a slider change from before it
    // (which would make Cancelar unable to restore that earlier value).
    void aSliderMovedRightAfterOpeningAToolCanStillBeCancelled()
    {
        open(file("a.png", busyImage(40, 30)));
        m_ctl->setAdjustParam(QStringLiteral("exposure"), 0.3); // starts a coalescing window
        m_ctl->beginToolSession();
        m_ctl->setAdjustParam(QStringLiteral("exposure"), 0.8);
        m_ctl->endToolSession(false);
        QCOMPARE(m_ctl->adjustMap().value(QStringLiteral("exposure")).toDouble(), 0.3);
    }

    void aToolSessionEndsWhenAnotherPictureArrives()
    {
        open(file("a.png", busyImage(40, 30)));
        m_ctl->beginToolSession(); // nothing done in it, so the picture may be replaced
        QVERIFY(m_ctl->toolSessionActive());
        open(file("b.png", busyImage(20, 20)));
        QVERIFY(!m_ctl->toolSessionActive());
    }

    // The modo pixel read-out: the colour of the pixel on screen at a position, nothing outside.
    void pixelAtReportsTheColourUnderAPosition()
    {
        QImage img(4, 3, QImage::Format_RGBA8888);
        img.fill(QColor(10, 20, 30, 255));
        img.setPixelColor(2, 1, QColor(200, 100, 50, 128));
        open(file("px.png", img));

        const QVariantMap hit = m_ctl->pixelAt(2, 1);
        QVERIFY(hit.value("valid").toBool());
        QCOMPARE(hit.value("r").toInt(), 200);
        QCOMPARE(hit.value("g").toInt(), 100);
        QCOMPARE(hit.value("b").toInt(), 50);
        QCOMPARE(hit.value("a").toInt(), 128);
        QVERIFY(m_ctl->pixelAt(0, 0).value("valid").toBool());
        QVERIFY(!m_ctl->pixelAt(4, 0).value("valid").toBool());
        QVERIFY(!m_ctl->pixelAt(-1, 2).value("valid").toBool());
        QVERIFY(!m_ctl->pixelAt(1, 3).value("valid").toBool());
    }

    void aNearestResizeFromTheControllerKeepsHardEdges()
    {
        QImage img(2, 1, QImage::Format_RGBA8888);
        img.setPixelColor(0, 0, QColor(0, 0, 0));
        img.setPixelColor(1, 0, QColor(255, 255, 255));
        open(file("two.png", img));
        m_ctl->resizeImage(8, 4, false, false, QStringLiteral("nearest"));
        QCOMPARE(m_ctl->currentImageSize(), QSize(8, 4));
        QSize size;
        const QImage shown = m_provider->requestImage(QStringLiteral("current"), &size, QSize());
        for (int x = 0; x < 8; ++x)
            QCOMPARE(shown.pixelColor(x, 0).red(), x < 4 ? 0 : 255);
    }

    void editsThatCancelOutAreClean()
    {
        open(file("a.png", busyImage(40, 30)));
        QVERIFY(!m_ctl->isDirty());

        m_ctl->cropNormalized(0, 0, 0.5, 0.5);
        QVERIFY(m_ctl->isDirty());
        m_ctl->undoEdit();
        QVERIFY(!m_ctl->isDirty());

        m_ctl->setAdjustParam(QStringLiteral("exposure"), 0.3);
        QVERIFY(m_ctl->isDirty());
        m_ctl->setAdjustParam(QStringLiteral("exposure"), 0.0);
        QVERIFY(!m_ctl->isDirty());
    }

    // ---- saving with an effect that is only being previewed ------------------

    // The file must contain what the history contains: saving applies the pending
    // effect as a real, undoable step instead of writing something that "Cancelar"
    // could then contradict.
    void savingWithAPendingEffectCommitsIt()
    {
        const QString a = file("a.png", busyImage(48, 48));
        const QImage before(a);
        open(a);

        m_ctl->selectEffect(QStringLiteral("threshold"));
        QVERIFY(!m_ctl->effectId().isEmpty());
        QVERIFY(saveAndWait());

        QVERIFY(m_ctl->effectId().isEmpty()); // no longer pending: it is in the history
        QVERIFY(m_ctl->canUndoEdit());
        QVERIFY(!m_ctl->isDirty());
        // the file does have the effect (both sides in one format, so a format difference alone cannot pass this)
        QVERIFY(QImage(a).convertToFormat(QImage::Format_RGBA8888) != before.convertToFormat(QImage::Format_RGBA8888));

        m_ctl->undoEdit();                    // the screen goes back to the plain picture...
        QVERIFY(m_ctl->isDirty());            // ...which the file no longer matches
    }

    // ---- effects that change the size of the picture ---------------------------------

    // A stretch (or a perspective correction) makes a picture of another size: the preview must put
    // that size on screen and tell the canvas to refit, cancelling must bring the old one back, and the
    // history must hold the result.
    void anEffectThatChangesTheSizeResizesTheView()
    {
        const QString a = file("a.png", busyImage(400, 300));
        open(a);
        QCOMPARE(m_ctl->currentImageSize(), QSize(400, 300));

        QSignalSpy structural(m_ctl, &AppController::structuralImageChanged);
        m_ctl->selectEffect(QStringLiteral("stretch")); // the guides at 35% / 65%, the middle at 150%
        QTRY_VERIFY_WITH_TIMEOUT(!m_ctl->effectBusy(), 10000);
        QVERIFY(m_ctl->currentImageSize().width() > 400);
        QCOMPARE(m_ctl->currentImageSize().height(), 300);
        QVERIFY(structural.count() >= 1);

        m_ctl->cancelEffect();
        QCOMPARE(m_ctl->currentImageSize(), QSize(400, 300));

        m_ctl->selectEffect(QStringLiteral("perspective"));
        QTRY_VERIFY_WITH_TIMEOUT(!m_ctl->effectBusy(), 10000);
        QVERIFY(m_ctl->currentImageSize().height() < 300);
        const QSize preview = m_ctl->currentImageSize();
        m_ctl->commitEffect();
        QCOMPARE(m_ctl->currentImageSize(), preview);
        QVERIFY(m_ctl->canUndoEdit());
        QVERIFY(m_ctl->isDirty());

        m_ctl->undoEdit();
        QCOMPARE(m_ctl->currentImageSize(), QSize(400, 300));
        m_ctl->redoEdit();
        QCOMPARE(m_ctl->currentImageSize(), preview);
    }

    // The meme caption carries a text beside its sliders: the preview grows the picture by the band, the text is
    // part of the undo step, and a text of nothing leaves the picture alone.
    void theCaptionKeepsItsTextThroughUndoAndRedo()
    {
        const QString a = file("cap.png", busyImage(400, 300));
        open(a);
        m_ctl->selectEffect(QStringLiteral("caption"));
        QVERIFY(m_ctl->effectUsesText());
        QTRY_VERIFY_WITH_TIMEOUT(!m_ctl->effectBusy(), 10000);
        QCOMPARE(m_ctl->currentImageSize(), QSize(400, 300)); // no text yet
        m_ctl->setEffectText(QStringLiteral("Hola mundo"));
        QTRY_VERIFY_WITH_TIMEOUT(!m_ctl->effectBusy(), 10000);
        QVERIFY(m_ctl->currentImageSize().height() > 300);
        const QSize with = m_ctl->currentImageSize();
        m_ctl->commitEffect();
        QCOMPARE(m_ctl->currentImageSize(), with);
        m_ctl->undoEdit();
        QCOMPARE(m_ctl->currentImageSize(), QSize(400, 300));
        m_ctl->redoEdit();
        QCOMPARE(m_ctl->currentImageSize(), with);
        QVERIFY(!m_ctl->effectUsesText()); // nothing is being previewed any more
    }

    // The same, stretching lengthways (the Vertical slider and the guides above and below).
    void stretchingVerticallyResizesTheViewToo()
    {
        for (const QSize size : {QSize(400, 300), QSize(2400, 3200)}) { // the big one is calculated on a smaller copy first
            const QString a = file(QString("v%1.png").arg(size.width()), busyImage(size.width(), size.height()));
            open(a);
            QCOMPARE(m_ctl->currentImageSize(), size);
            m_ctl->selectEffect(QStringLiteral("stretch"));
            QTRY_VERIFY_WITH_TIMEOUT(!m_ctl->effectBusy(), 30000);
            m_ctl->setEffectValue(4, 100); // Horizontal: none
            m_ctl->setEffectValue(5, 200); // Vertical: the middle band twice as tall
            QTRY_VERIFY_WITH_TIMEOUT(!m_ctl->effectBusy(), 30000);
            QCOMPARE(m_ctl->currentImageSize().width(), size.width());
            QVERIFY2(m_ctl->currentImageSize().height() > size.height() * 1.25,
                     qPrintable(QString::number(m_ctl->currentImageSize().height())));
            const QSize preview = m_ctl->currentImageSize();
            m_ctl->commitEffect();
            QCOMPARE(m_ctl->currentImageSize(), preview);
            m_ctl->undoEdit();
            QCOMPARE(m_ctl->currentImageSize(), size);
        }
    }

    // The Marco page of Recortar is the hidden "frame" effect: the outline, margin and shadow make the picture
    // bigger (and "Mantener el tamaño" brings it back), it is one undo step, and it is not offered in the
    // effects grid.
    void theFrameGrowsThePictureAndUndoesCleanly()
    {
        const QString a = file("a.png", busyImage(400, 300));
        open(a);
        bool listedAsHidden = false;
        for (const QVariant &fx : m_ctl->effectList())
            if (fx.toMap().value(QStringLiteral("id")).toString() == QLatin1String("frame"))
                listedAsHidden = fx.toMap().value(QStringLiteral("hidden")).toBool();
        QVERIFY(listedAsHidden);

        m_ctl->selectEffect(QStringLiteral("frame")); // 3 % outline and a soft shadow around rounded corners
        QTRY_VERIFY_WITH_TIMEOUT(!m_ctl->effectBusy(), 10000);
        const QSize grown = m_ctl->currentImageSize();
        QVERIFY2(grown.width() > 400 && grown.height() > 300, qPrintable(QStringLiteral("%1x%2").arg(grown.width()).arg(grown.height())));

        m_ctl->setEffectValue(9, 1); // Mantener el tamaño
        QTRY_VERIFY_WITH_TIMEOUT(!m_ctl->effectBusy(), 10000);
        QCOMPARE(m_ctl->currentImageSize(), QSize(400, 300));
        m_ctl->setEffectValue(9, 0);
        QTRY_VERIFY_WITH_TIMEOUT(!m_ctl->effectBusy(), 10000);
        QCOMPARE(m_ctl->currentImageSize(), grown);

        m_ctl->commitEffect();
        QCOMPARE(m_ctl->currentImageSize(), grown);
        QVERIFY(m_ctl->isDirty());
        m_ctl->undoEdit();
        QCOMPARE(m_ctl->currentImageSize(), QSize(400, 300));
        QVERIFY(!m_ctl->isDirty());
        m_ctl->redoEdit();
        QCOMPARE(m_ctl->currentImageSize(), grown);
    }

    // ---- saving does not hold the interface -------------------------------------

    // Starts the save, then keeps the event loop running with a 10 ms ticker: the longest
    // gap between ticks is how long the interface was frozen. (A 12 MP JPEG with a look
    // and adjustments used to block the caller for ~600 ms.)
    void savingDoesNotFreezeTheInterface()
    {
        const QString big = file("big.jpg", busyImage(4000, 3000));
        open(big);
        m_ctl->setAdjustParam(QStringLiteral("exposure"), 0.3);
        m_ctl->setAdjustParam(QStringLiteral("clarity"), 0.4);
        m_ctl->setAdjustParam(QStringLiteral("sharpness"), 0.5);
        m_ctl->applyFilterPreset(QStringLiteral("cine"), 1.0);

        QSignalSpy finished(m_ctl, &AppController::saveFinished);
        QSignalSpy savingChanged(m_ctl, &AppController::isSavingChanged);
        QElapsedTimer call;
        call.start();
        QVERIFY(m_ctl->saveEdited());
        const qint64 startMs = call.elapsed();
        QVERIFY2(startMs < 150, qPrintable(QStringLiteral("starting the save took %1 ms").arg(startMs)));
        QVERIFY(m_ctl->isSaving());
        QVERIFY(!m_ctl->canSaveInPlace()); // nothing else may start meanwhile

        qint64 last = call.elapsed(), worstGap = 0;
        QTimer ticker;
        ticker.setInterval(10);
        connect(&ticker, &QTimer::timeout, this, [&]() {
            const qint64 now = call.elapsed();
            worstGap = std::max(worstGap, now - last);
            last = now;
        });
        ticker.start();
        QVERIFY(finished.wait(60000));
        ticker.stop();

        QVERIFY(finished.first().first().toBool());
        QVERIFY(!m_ctl->isSaving());
        QCOMPARE(savingChanged.count(), 2); // on, then off
        QVERIFY(!m_ctl->isDirty());
        QVERIFY(m_ctl->canSaveInPlace());
        qInfo() << "starting the save:" << startMs << "ms; longest event-loop stall during it:" << worstGap << "ms";
        QVERIFY2(worstGap < 250, qPrintable(QStringLiteral("the event loop stalled for %1 ms").arg(worstGap)));
    }

    // What is written is the picture AS IT WAS when Guardar was pressed; what is edited
    // meanwhile is not in the file and is still unsaved when the write ends.
    void editsMadeWhileSavingStayUnsaved()
    {
        const QString big = file("big.png", busyImage(2400, 1800));
        open(big);
        m_ctl->cropNormalized(0, 0, 0.5, 0.5); // 1200 x 900
        QSignalSpy finished(m_ctl, &AppController::saveFinished);
        QVERIFY(m_ctl->saveEdited());
        m_ctl->rotateEdit90();                 // meanwhile: 900 x 1200 on screen
        QVERIFY(finished.wait(60000));
        QVERIFY(finished.first().first().toBool());

        QCOMPARE(QImage(big).size(), QSize(1200, 900));
        QVERIFY(m_ctl->isDirty());
        QCOMPARE(m_ctl->currentImageSize(), QSize(900, 1200));

        QVERIFY(saveAndWait()); // and the next save takes the rotation too
        QCOMPARE(QImage(big).size(), QSize(900, 1200));
        QVERIFY(!m_ctl->isDirty());
    }

    void aSecondSaveWhileOneRunsIsRefused()
    {
        const QString big = file("big.png", busyImage(2400, 1800));
        open(big);
        m_ctl->cropNormalized(0, 0, 0.5, 0.5);
        QSignalSpy finished(m_ctl, &AppController::saveFinished);
        QVERIFY(m_ctl->saveEdited());
        QVERIFY(!m_ctl->saveEdited());
        QVERIFY(!m_ctl->saveEditedAs(QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("other.png")))));
        QVERIFY(!m_ctl->errorString().isEmpty());
        QVERIFY(finished.wait(60000));
        QCOMPARE(finished.count(), 1);
        QVERIFY(!QFileInfo::exists(m_dir.filePath(QStringLiteral("other.png"))));
    }

    // Asking for another picture while the file is being written waits for the write.
    void openingAnotherPictureWaitsForTheSave()
    {
        const QString a = file("a.png", busyImage(2400, 1800));
        const QString b = file("b.png", busyImage(40, 30));
        open(a);
        m_ctl->cropNormalized(0, 0, 0.5, 0.5);
        QSignalSpy finished(m_ctl, &AppController::saveFinished);
        QSignalSpy blocked(m_ctl, &AppController::unsavedChangesBlocked);
        QVERIFY(m_ctl->saveEdited());

        m_ctl->openPath(b);
        QVERIFY(m_ctl->isSaving());
        QVERIFY(!m_ctl->isLoading());                          // not started yet
        QCOMPARE(m_ctl->currentFileName(), QStringLiteral("a.png"));

        QTRY_COMPARE_WITH_TIMEOUT(m_ctl->currentFileName(), QStringLiteral("b.png"), 60000);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(blocked.count(), 0);                          // no spurious "unsaved changes" question
        QCOMPARE(QImage(a).size(), QSize(1200, 900));
    }

    // "Guardar" in the unsaved-changes question: the file is written, THEN the other
    // picture opens.
    void theUnsavedQuestionSaveThenOpens()
    {
        const QString a = file("a.png", busyImage(40, 30));
        const QString b = file("b.png", busyImage(20, 20));
        open(a);
        m_ctl->cropNormalized(0, 0, 0.5, 0.5);
        QSignalSpy blocked(m_ctl, &AppController::unsavedChangesBlocked);

        m_ctl->openPath(b);
        QCOMPARE(blocked.count(), 1);
        m_ctl->resolveUnsaved(QStringLiteral("save"));

        QTRY_COMPARE_WITH_TIMEOUT(m_ctl->currentFileName(), QStringLiteral("b.png"), 10000);
        QCOMPARE(QImage(a).size(), QSize(20, 15));
    }

    // A write that fails says so, keeps the edits unsaved, and does NOT carry on to the
    // picture that was waiting.
    void aFailedSaveKeepsTheEditsAndDropsWhatWasWaiting()
    {
        const QString a = file("a.png", busyImage(40, 30));
        const QString b = file("b.png", busyImage(20, 20));
        open(a);
        m_ctl->cropNormalized(0, 0, 0.5, 0.5);

        // a destination that cannot be created
        QSignalSpy finished(m_ctl, &AppController::saveFinished);
        QVERIFY(m_ctl->saveEditedAs(QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("no/such/folder/x.png")))));
        QVERIFY(finished.wait(10000));
        QVERIFY(!finished.first().first().toBool());
        QVERIFY(!m_ctl->errorString().isEmpty());
        QVERIFY(!m_ctl->isSaving());
        QVERIFY(m_ctl->isDirty());

        // and the same through the "save, then open b" answer, with the file made read-only
        QVERIFY(QFile::setPermissions(a, QFileDevice::ReadOwner));
        m_ctl->openPath(b);
        finished.clear();
        m_ctl->resolveUnsaved(QStringLiteral("save"));
        const bool ended = finished.wait(10000);
        QFile::setPermissions(a, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        QVERIFY(ended);
        qInfo() << "write over a read-only file reported ok =" << finished.first().first().toBool();
        if (!finished.first().first().toBool()) { // (a platform that lets the replace through is not a failure of the code)
            QTest::qWait(200);
            QCOMPARE(m_ctl->currentFileName(), QStringLiteral("a.png"));
            QVERIFY(m_ctl->isDirty());
        }
    }

    // The effect being previewed becomes a history step at once, and the worker (not the
    // interface) calculates it at full size.
    void aPendingEffectIsCalculatedByTheWorker()
    {
        const QString big = file("big.png", busyImage(2400, 1800));
        const QImage before(big);
        open(big);
        m_ctl->selectEffect(QStringLiteral("median")); // slow, and certainly not calculated yet
        QVERIFY(!m_ctl->effectId().isEmpty());

        QSignalSpy finished(m_ctl, &AppController::saveFinished);
        QElapsedTimer t;
        t.start();
        QVERIFY(m_ctl->saveEdited());
        const qint64 startMs = t.elapsed();
        qInfo() << "starting a save with a pending 'median' effect on 4.3 MP:" << startMs << "ms";
        QVERIFY2(startMs < 300, qPrintable(QStringLiteral("starting the save took %1 ms").arg(startMs)));
        QVERIFY(m_ctl->effectId().isEmpty()); // committed right away...
        QVERIFY(m_ctl->canUndoEdit());

        QVERIFY(finished.wait(120000));       // ...and calculated by the worker
        QVERIFY(finished.first().first().toBool());
        QVERIFY(!m_ctl->isDirty());
        QVERIFY(QImage(big).convertToFormat(QImage::Format_RGBA8888) != before.convertToFormat(QImage::Format_RGBA8888));

        m_ctl->undoEdit();                    // the picture the worker calculated is reused, undo still works
        QVERIFY(m_ctl->isDirty());
        QVERIFY(m_ctl->canRedoEdit());
    }

    void theFileCannotBeDeletedWhileItIsBeingSaved()
    {
        const QString a = file("a.png", busyImage(2400, 1800));
        open(a);
        m_ctl->cropNormalized(0, 0, 0.5, 0.5);
        QSignalSpy finished(m_ctl, &AppController::saveFinished);
        QVERIFY(m_ctl->saveEdited());
        QVERIFY(!m_ctl->deleteCurrentFile());
        QVERIFY(QFileInfo::exists(a));
        QVERIFY(finished.wait(60000));
    }

    // Closing the program while the file is being written lets the write finish.
    void destroyingTheControllerWhileSavingFinishesTheFile()
    {
        const QString big = file("big.png", busyImage(2400, 1800));
        open(big);
        m_ctl->cropNormalized(0, 0, 0.5, 0.5);
        QVERIFY(m_ctl->saveEdited());
        delete m_ctl;
        m_ctl = nullptr;
        QCOMPARE(QImage(big).size(), QSize(1200, 900)); // whole and final, not the original
        QCoreApplication::processEvents();
        m_ctl = new AppController(m_provider, m_folder);
    }

    // ---- colour ----------------------------------------------------------------------

    // A Display P3 photo is shown in sRGB (not with P3's numbers read as sRGB), and saving
    // it in place does not convert it a second time: the file says sRGB, and opening it
    // again gives the same colours.
    void aDisplayP3PictureIsShownInSrgbAndSavedWithoutConvertingTwice()
    {
        QImage p3(40, 30, QImage::Format_RGBA8888);
        p3.fill(QColor(200, 120, 60));
        p3.setColorSpace(QColorSpace(QColorSpace::DisplayP3));
        const QString path = m_dir.filePath(QStringLiteral("p3.png"));
        QVERIFY(p3.save(path));
        open(path);

        QSize size;
        const QImage shown = m_provider->requestImage(QStringLiteral("current"), &size, QSize());
        const colorref::Vec3 want = colorref::referenceToSrgb(colorref::kDisplayP3ToXyz, colorref::srgbToLinear, 200, 120, 60);
        const QRgb got = shown.pixel(10, 10);
        QVERIFY2(std::abs(qRed(got) - want[0]) <= 3 && std::abs(qGreen(got) - want[1]) <= 3 && std::abs(qBlue(got) - want[2]) <= 3,
                 qPrintable(QStringLiteral("shown %1,%2,%3").arg(qRed(got)).arg(qGreen(got)).arg(qBlue(got))));

        m_ctl->flipEditHorizontal();
        QVERIFY(saveAndWait());

        // reopen through the controller: the same colours (a second conversion would move them)
        m_ctl->openPath(file("other.png", busyImage(8, 8)));
        QTRY_VERIFY_WITH_TIMEOUT(!m_ctl->isLoading(), 5000);
        open(path);
        const QImage again = m_provider->requestImage(QStringLiteral("current"), &size, QSize());
        const QRgb back = again.pixel(10, 10);
        QVERIFY2(std::abs(qRed(back) - qRed(got)) <= 2 && std::abs(qGreen(back) - qGreen(got)) <= 2
                     && std::abs(qBlue(back) - qBlue(got)) <= 2,
                 qPrintable(QStringLiteral("after the save %1,%2,%3").arg(qRed(back)).arg(qGreen(back)).arg(qBlue(back))));
    }

    // ---- lifecycle ---------------------------------------------------------------

    // Closing while an effect is still being calculated: the workers hold the controller,
    // so it must wait for them (they stop at the cancel flag) and then go away cleanly.
    void destroyingTheControllerMidEffectIsSafeAndQuick()
    {
        const QString big = file("big.png", busyImage(1600, 1200));
        open(big);
        m_ctl->selectEffect(QStringLiteral("median")); // one of the slowest
        QVERIFY(m_ctl->effectBusy());

        QElapsedTimer timer;
        timer.start();
        delete m_ctl; // cleanup() would do it anyway; the time is what is being measured
        m_ctl = nullptr;
        QVERIFY2(timer.elapsed() < 2500, qPrintable(QStringLiteral("took %1 ms").arg(timer.elapsed())));
        // the events the workers posted before stopping are harmless: run them
        QCoreApplication::processEvents();
        m_ctl = new AppController(m_provider, m_folder); // so cleanup() has something to delete
    }

    // ---- a save that works but cannot keep everything ------------------------

    // The picture is saved; the user is told (not an error) what was left behind.
    void aSaveThatLosesMetadataTellsTheUser()
    {
        const QString heic = m_dir.filePath(QStringLiteral("phone.heic"));
        QVERIFY(QFile::copy(QStringLiteral(FIXTURES_DIR "/format_sample.heic"), heic));
        open(heic);
        QSignalSpy notice(m_ctl, &AppController::saveNotice);

        const QString out = m_dir.filePath(QStringLiteral("phone.jpg"));
        QVERIFY(saveAndWait(QUrl::fromLocalFile(out)));

        QVERIFY(QFileInfo::exists(out));
        QCOMPARE(notice.count(), 1);
        QVERIFY2(notice.first().first().toString().contains(QStringLiteral("heic")),
                 qPrintable(notice.first().first().toString()));
        QVERIFY(m_ctl->errorString().isEmpty()); // a notice, not an error
    }

private:
    // Starts a save (in place, or to `url`), waits for it to end and says whether it worked.
    bool saveAndWait(const QUrl &url = QUrl())
    {
        QSignalSpy finished(m_ctl, &AppController::saveFinished);
        const bool started = url.isEmpty() ? m_ctl->saveEdited() : m_ctl->saveEditedAs(url);
        if (!started)
            return false;
        if (finished.isEmpty() && !finished.wait(60000))
            return false;
        return finished.first().first().toBool();
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

QTEST_MAIN(TestAppController)
#include "test_app_controller.moc"
