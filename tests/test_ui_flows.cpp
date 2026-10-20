// A few journeys through the real interface, driven the way a person drives it: the real Main.qml, on the same wiring
// as the program (AppBootstrap), with synthetic mouse and key events. They are the net under any redesign of the
// interface, so they are written by FUNCTION, not by position or look: every control is found by its objectName
// ("nextButton", "editButton"...), never by where it sits or what it says. A redesign can move and restyle things
// freely as long as those names keep meaning the same control.
//
// No window and no GPU needed: Qt's offscreen platform and its software renderer (see CMakeLists.txt).
#include "AppBootstrap.h"
#include "AppController.h"
#include "AppSettings.h"
#include "FolderModel.h"

#include <QFileInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

namespace {

QImage picture(int w, int h, QColor c)
{
    QImage img(w, h, QImage::Format_RGBA8888);
    img.fill(c);
    return img;
}

} // namespace

class TestUiFlows : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true); // the settings and the cache of the tests are not the person's own
        QQuickStyle::setStyle("Basic");
    }

    // A fresh program for every journey: nothing carries over from the one before.
    void init()
    {
        QVERIFY(m_dir.isValid());
        m_engine = new QQmlApplicationEngine;
        qputenv("PROJECTFOXY_NO_UPDATE_CHECK", "1"); // no network in the tests
        qputenv("PROJECTFOXY_FESTIVE", "off");       // no holiday decorations over the window
        m_app = AppBootstrap::setup(*m_engine);
        m_app.settings->setSingleInstance(true);
        m_engine->loadFromModule("ImageViewerApp", "Main");
        QVERIFY(!m_engine->rootObjects().isEmpty());
        m_window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().first());
        QVERIFY(m_window);
        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window));
    }

    void cleanup()
    {
        delete m_engine; // (waits for what is still running)
        m_engine = nullptr;
        for (const QString &name : QDir(m_dir.path()).entryList(QDir::Files))
            QFile::remove(m_dir.filePath(name));
    }

    // ------------------------------------------------------------------------------------------------ opening a picture

    void aPictureOpenedAtStartIsShownInTheCanvas()
    {
        const QString a = file("a.png", picture(120, 80, Qt::red));
        m_app.controller->openPath(a);
        QTRY_COMPARE_WITH_TIMEOUT(m_app.controller->currentFileName(), QStringLiteral("a.png"), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(!m_app.controller->currentSource().isEmpty(), 5000);
        QQuickItem *canvas = item("imageCanvas");
        QVERIFY(canvas->isVisible());
        QCOMPARE(m_app.controller->currentImageSize(), QSize(120, 80));
    }

    // ------------------------------------------------------------------------------------------------ browsing

    void theNextAndPreviousButtonsWalkTheFolder()
    {
        openFolder({"a.png", "b.png", "c.png"});
        QCOMPARE(m_app.folderModel->count(), 3);

        click("nextButton");
        QTRY_COMPARE_WITH_TIMEOUT(m_app.controller->currentFileName(), QStringLiteral("b.png"), 10000);
        click("nextButton");
        QTRY_COMPARE_WITH_TIMEOUT(m_app.controller->currentFileName(), QStringLiteral("c.png"), 10000);
        click("previousButton");
        QTRY_COMPARE_WITH_TIMEOUT(m_app.controller->currentFileName(), QStringLiteral("b.png"), 10000);
    }

    void theArrowKeysWalkTheFolderToo()
    {
        openFolder({"a.png", "b.png", "c.png"});
        QTest::keyClick(m_window, Qt::Key_Right);
        QTRY_COMPARE_WITH_TIMEOUT(m_app.controller->currentFileName(), QStringLiteral("b.png"), 10000);
        QTest::keyClick(m_window, Qt::Key_Left);
        QTRY_COMPARE_WITH_TIMEOUT(m_app.controller->currentFileName(), QStringLiteral("a.png"), 10000);
    }

    // ------------------------------------------------------------------------------------------------ holidays

    void theHolidayDecorationsShowForAPreviewAndHideAgain()
    {
        QQuickItem *decor = findIn(m_window->contentItem(), "festiveDecor");
        QVERIFY(decor);
        QVERIFY(!decor->isVisible()); // the tests start with PROJECTFOXY_FESTIVE=off
        QObject *festive = m_engine->rootContext()->contextProperty("festive").value<QObject *>();
        QVERIFY(festive);
        festive->setProperty("preview", "navidad");
        QTRY_VERIFY_WITH_TIMEOUT(decor->isVisible(), 3000);
        QCOMPARE(festive->property("festival").toString(), QStringLiteral("navidad"));
        festive->setProperty("preview", "");
        QTRY_VERIFY_WITH_TIMEOUT(!decor->isVisible(), 3000);
    }

    void theSettingsHaveACardForTheDecorationsBelowTheWindowCard()
    {
        click("settingsButton");
        QQuickItem *card = item("festiveCard");
        QQuickItem *above = item("windowCard");
        // the cards of this page are placed by hand: the new one must not overlap the one before it
        QVERIFY(card->y() >= above->y() + above->height());
        m_app.settings->setFestive(true); // (the tests share their settings folder: leave it as it was found)
        // (it sits far down the page, out of the window, so it is not clicked here: just that it shows the setting)
        QCOMPARE(item("festiveSwitch")->property("checked").toBool(), true);
        m_app.settings->setFestive(false);
        QTRY_COMPARE_WITH_TIMEOUT(item("festiveSwitch")->property("checked").toBool(), false, 3000);
        m_app.settings->setFestive(true);
    }

    // ------------------------------------------------------------------------------------------------ video to GIF

    void aVideoOpenedInTheGifStudioBecomesTheFramesOfTheStrip()
    {
        click("gifButton");
        QQuickItem *studio = item("gifStudio");
        QVERIFY(QMetaObject::invokeMethod(studio, "openVideo", Q_ARG(QVariant, QStringLiteral(FIXTURES_DIR "/video_20frames_10fps.mp4"))));
        item("videoToGifPage");
        QObject *video = m_engine->rootContext()->contextProperty("videoToGif").value<QObject *>();
        QObject *anim = m_engine->rootContext()->contextProperty("animStudio").value<QObject *>();
        QVERIFY(video && anim);
        QTRY_VERIFY_WITH_TIMEOUT(video->property("loaded").toBool(), 15000);
        QCOMPARE(anim->property("count").toInt(), 0);

        click("createFramesButton"); // the whole video at the page's 10 frames per second
        QTRY_COMPARE_WITH_TIMEOUT(anim->property("count").toInt(), 20, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(itemOrNull("videoToGifPage") == nullptr, 5000); // the page closes itself
    }

    void whileOneStudioIsOpenTheButtonOfTheOtherIsOff()
    {
        click("gifButton");
        QTRY_VERIFY_WITH_TIMEOUT(!item("collageButton")->isEnabled(), 3000);
        QVERIFY(item("gifButton")->isEnabled()); // it closes its own studio
        click("gifButton");
        QTRY_VERIFY_WITH_TIMEOUT(item("collageButton")->isEnabled(), 3000);
        click("collageButton");
        QTRY_VERIFY_WITH_TIMEOUT(!item("gifButton")->isEnabled(), 3000);
        QVERIFY(item("collageButton")->isEnabled());
    }

    void theStudiosAddButtonTakesPicturesAndVideosAlike()
    {
        click("gifButton");
        QQuickItem *studio = item("gifStudio");
        const QString png = file("p.png", picture(40, 30, Qt::red));
        // what the add dialog or a drop hands over: a picture and a video at once
        const QVariantList both{png, QStringLiteral(FIXTURES_DIR "/video_20frames_10fps.mp4")};
        QVERIFY(QMetaObject::invokeMethod(studio, "addFiles", Q_ARG(QVariant, both)));
        QObject *anim = m_engine->rootContext()->contextProperty("animStudio").value<QObject *>();
        QTRY_COMPARE_WITH_TIMEOUT(anim->property("count").toInt(), 1, 5000); // the picture joined the strip
        item("videoToGifPage");                                              // and the video opened its page
    }

    void aVideoOpenedLikeAnyFileGoesToTheGifStudio()
    {
        QSignalSpy requested(m_app.controller, &AppController::videoRequested);
        m_app.controller->openPath(QStringLiteral(FIXTURES_DIR "/video_20frames_10fps.mp4")); // as Abrir, a drop or the command line do
        QCOMPARE(requested.size(), 1);
        item("gifStudio");
        item("videoToGifPage");
        QObject *video = m_engine->rootContext()->contextProperty("videoToGif").value<QObject *>();
        QTRY_VERIFY_WITH_TIMEOUT(video->property("loaded").toBool(), 15000);
        QVERIFY(m_app.controller->currentFileName().isEmpty()); // no picture was opened
    }

    // ------------------------------------------------------------------------------------------------ zoom

    void theZoomButtonsChangeTheZoomAndFitBringsItBack()
    {
        const QString a = file("big.png", picture(2400, 1600, Qt::blue)); // much bigger than the window: fits below 100 %
        m_app.controller->openPath(a);
        QTRY_COMPARE_WITH_TIMEOUT(m_app.controller->currentFileName(), QStringLiteral("big.png"), 15000);
        QQuickItem *canvas = item("imageCanvas");
        QTRY_VERIFY_WITH_TIMEOUT(zoom(canvas) > 0 && zoom(canvas) < 100, 5000);
        const int fitted = zoom(canvas);

        click("zoomInButton");
        QTRY_VERIFY_WITH_TIMEOUT(zoom(canvas) > fitted, 3000);
        const int once = zoom(canvas);
        click("zoomInButton");
        QTRY_VERIFY_WITH_TIMEOUT(zoom(canvas) > once, 3000);
        const int twice = zoom(canvas);
        click("zoomOutButton");
        QTRY_VERIFY_WITH_TIMEOUT(zoom(canvas) < twice, 3000);

        click("fitButton");
        QTRY_COMPARE_WITH_TIMEOUT(zoom(canvas), fitted, 3000);
    }

    // ------------------------------------------------------------------------------------------------ the title bar

    // The title bar is the toolbar. As the window narrows the optional tools move into the "Más herramientas" menu,
    // but Editar and Configuración never leave the bar.
    void theToolsMoveIntoTheMenuAsTheWindowNarrowsButEditAndSettingsStay()
    {
        // (a file name long enough that, at the narrowest, it keeps its share of the bar)
        m_app.controller->openPath(file("a-picture-with-a-fairly-long-name.png", picture(120, 80, Qt::red)));
        QTRY_VERIFY_WITH_TIMEOUT(!m_app.controller->currentFileName().isEmpty(), 10000);
        m_window->resize(1280, 700);
        QTRY_VERIFY_WITH_TIMEOUT(itemOrNull("gifButton") && itemOrNull("collageButton"), 3000);
        QVERIFY(itemOrNull("editButton") && itemOrNull("settingsButton"));

        m_window->resize(m_window->minimumWidth(), 700);
        QTRY_VERIFY_WITH_TIMEOUT(!itemOrNull("gifButton") && !itemOrNull("collageButton"), 3000);
        QVERIFY(itemOrNull("editButton"));
        QVERIFY(itemOrNull("settingsButton"));
        // Whatever the size, the bar keeps room to grab and drag.
        QVERIFY(item("titleDrag")->width() > 96);
    }

    // ------------------------------------------------------------------------------------------------ the reader

    // "Leer" turns the folder of the open picture into a book. Reading right to left (the default) the left side goes
    // FORWARD; "R" turns the direction around; Home and End jump; Esc gives the viewer back.
    void theReaderTurnsPagesInTheDirectionOfReading()
    {
        // portrait pages: landscape ones would stand alone in a double page
        QString first;
        for (const char *name : {"a.png", "b.png", "c.png"}) {
            const QString path = file(name, picture(60, 100, Qt::darkGreen));
            if (first.isEmpty())
                first = path;
        }
        m_app.controller->openPath(first);
        QTRY_COMPARE_WITH_TIMEOUT(m_app.controller->currentFileName(), QStringLiteral("a.png"), 15000);
        QTRY_COMPARE_WITH_TIMEOUT(m_app.folderModel->count(), 3, 10000);
        QObject *reader = m_engine->rootContext()->contextProperty("mangaReader").value<QObject *>();
        QVERIFY(reader);
        reader->setProperty("rightToLeft", true);
        reader->setProperty("doublePage", false);
        const auto page = [reader]() { return QQmlProperty::read(reader, "currentPage").toInt(); };

        click("readButton");
        QTRY_VERIFY_WITH_TIMEOUT(QQmlProperty::read(reader, "active").toBool(), 5000);
        QCOMPARE(QQmlProperty::read(reader, "pageCount").toInt(), 3);
        QCOMPARE(page(), 0);

        QTest::keyClick(m_window, Qt::Key_Left); // right to left: left is forward
        QTRY_COMPARE_WITH_TIMEOUT(page(), 1, 3000);
        QTest::keyClick(m_window, Qt::Key_Right);
        QTRY_COMPARE_WITH_TIMEOUT(page(), 0, 3000);

        // the click zones at both sides of the page: the left one is forward when reading right to left
        QTest::mouseClick(m_window, Qt::LeftButton, Qt::NoModifier, QPoint(m_window->width() / 10, m_window->height() / 2));
        QTRY_COMPARE_WITH_TIMEOUT(page(), 1, 3000);
        QTest::mouseClick(m_window, Qt::LeftButton, Qt::NoModifier, QPoint(m_window->width() * 9 / 10, m_window->height() / 2));
        QTRY_COMPARE_WITH_TIMEOUT(page(), 0, 3000);

        QTest::keyClick(m_window, Qt::Key_R); // left to right
        QVERIFY(!QQmlProperty::read(reader, "rightToLeft").toBool());
        QTest::keyClick(m_window, Qt::Key_Right);
        QTRY_COMPARE_WITH_TIMEOUT(page(), 1, 3000);
        QTest::keyClick(m_window, Qt::Key_End);
        QTRY_COMPARE_WITH_TIMEOUT(page(), 2, 3000);
        QTest::keyClick(m_window, Qt::Key_Right); // the end: nowhere to go
        QTest::keyClick(m_window, Qt::Key_Home);
        QTRY_COMPARE_WITH_TIMEOUT(page(), 0, 3000);

        QTest::keyClick(m_window, Qt::Key_D); // two pages at a time: the cover stays alone, then 2 and 3 go together
        QTRY_VERIFY_WITH_TIMEOUT(QQmlProperty::read(reader, "doublePage").toBool(), 3000);
        QTest::keyClick(m_window, Qt::Key_Right);
        QTRY_COMPARE_WITH_TIMEOUT(page(), 1, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(QQmlProperty::read(reader, "lastPage").toInt(), 2, 3000);

        QTest::keyClick(m_window, Qt::Key_Escape);
        QTRY_VERIFY_WITH_TIMEOUT(!QQmlProperty::read(reader, "active").toBool(), 3000);
        QVERIFY(item("imageCanvas")->isVisible());
        reader->setProperty("rightToLeft", true); // the tests share their settings folder
        reader->setProperty("doublePage", false);
    }

    // ------------------------------------------------------------------------------------------------ editing and saving

    // Edit mode, the crop tool, a quarter turn, Aplicar, Guardar and the overwrite question: the file on disk ends up
    // turned, and nothing is left unsaved.
    void aQuarterTurnIsAppliedAndSavedOverTheFile()
    {
        const QString a = file("turn.png", picture(80, 40, Qt::green));
        m_app.controller->openPath(a);
        QTRY_COMPARE_WITH_TIMEOUT(m_app.controller->currentFileName(), QStringLiteral("turn.png"), 10000);
        QVERIFY(!m_app.controller->isDirty());

        click("editButton");
        click("tool-crop");
        click("rotateRightButton"); // animated, then baked
        QTRY_COMPARE_WITH_TIMEOUT(m_app.controller->currentImageSize(), QSize(40, 80), 10000);
        QVERIFY(m_app.controller->hasEdits());
        click("applyToolButton");
        QTRY_VERIFY_WITH_TIMEOUT(!m_app.controller->toolSessionActive(), 5000);
        QVERIFY(m_app.controller->isDirty());

        QSignalSpy saved(m_app.controller, &AppController::saveFinished);
        click("saveButton");
        // "«turn.png» se va a reemplazar por la imagen editada": confirm it (unless the person asked not to be asked)
        if (QTest::qWaitFor([&]() { return itemOrNull("overwriteConfirmButton") != nullptr; }, 2000))
            click("overwriteConfirmButton");
        QTRY_VERIFY_WITH_TIMEOUT(saved.count() == 1, 15000);
        QVERIFY(saved.first().first().toBool());
        QVERIFY(!m_app.controller->isDirty());
        QCOMPARE(QImage(a).size(), QSize(40, 80));
    }

    void undoTakesTheTurnBack()
    {
        const QString a = file("undo.png", picture(80, 40, Qt::yellow));
        m_app.controller->openPath(a);
        QTRY_COMPARE_WITH_TIMEOUT(m_app.controller->currentFileName(), QStringLiteral("undo.png"), 10000);
        click("editButton");
        click("tool-crop");
        click("rotateRightButton");
        QTRY_COMPARE_WITH_TIMEOUT(m_app.controller->currentImageSize(), QSize(40, 80), 10000);
        click("applyToolButton");
        QTRY_VERIFY_WITH_TIMEOUT(!m_app.controller->toolSessionActive(), 5000);
        click("undoButton");
        QTRY_COMPARE_WITH_TIMEOUT(m_app.controller->currentImageSize(), QSize(80, 40), 5000);
        QVERIFY(!m_app.controller->isDirty());
    }

    // ------------------------------------------------------------------------------------------------ the unsaved question

    // Another picture while this one has unsaved edits: the question comes up. "Cancelar" stays; "Descartar" goes on.
    void aNavigationWithUnsavedEditsAsksAndBothAnswersWork()
    {
        openFolder({"a.png", "b.png"});
        m_app.controller->rotateEdit90(); // an edit, made without the panel (it is the question that is tested here)
        QVERIFY(m_app.controller->isDirty());

        click("nextButton");
        click("unsavedCancelButton"); // appears when the controller holds the request back
        QTest::qWait(300);
        QCOMPARE(m_app.controller->currentFileName(), QStringLiteral("a.png")); // stayed
        QVERIFY(m_app.controller->isDirty());                                   // and the edit is still there
        QCOMPARE(m_app.folderModel->currentIndex(), 0);                         // the strip is back on the open picture

        click("nextButton");
        click("unsavedDiscardButton");
        QTRY_COMPARE_WITH_TIMEOUT(m_app.controller->currentFileName(), QStringLiteral("b.png"), 10000);
        QVERIFY(!m_app.controller->isDirty());
    }

    // ------------------------------------------------------------------------------------------------ the studios

    // The button that opens a studio closes it again; with nothing made there is no question.
    void theStudioButtonOpensAndClosesItAndAsksNothingWhenEmpty()
    {
        click("gifButton");
        QVERIFY(item("gifStudio")->isVisible());
        click("gifButton");
        QTRY_VERIFY_WITH_TIMEOUT(itemOrNull("gifStudio") == nullptr, 5000);
        QVERIFY(itemOrNull("dialogConfirmButton") == nullptr);

        click("collageButton");
        QVERIFY(item("collageStudio")->isVisible());
        click("collageButton");
        QTRY_VERIFY_WITH_TIMEOUT(itemOrNull("collageStudio") == nullptr, 5000);
    }

    // With frames in the animation, leaving asks first: "Seguir aquí" stays, "Salir sin guardar" goes and empties it.
    void leavingTheGifStudioWithWorkAsksAndBothAnswersWork()
    {
        const QString a = file("f1.png", picture(60, 40, Qt::red));
        const QString b = file("f2.png", picture(60, 40, Qt::blue));
        click("gifButton");
        QObject *studio = m_engine->rootContext()->contextProperty(QStringLiteral("animStudio")).value<QObject *>();
        QVERIFY(studio);
        QVERIFY(QMetaObject::invokeMethod(studio, "addPaths", Q_ARG(QStringList, QStringList({a, b}))));
        QTRY_COMPARE_WITH_TIMEOUT(studio->property("count").toInt(), 2, 10000);

        click("gifButton");
        click("dialogCancelButton");
        QTest::qWait(300);
        QVERIFY(item("gifStudio")->isVisible()); // stayed
        QCOMPARE(studio->property("count").toInt(), 2);

        click("gifButton");
        click("dialogConfirmButton");
        QTRY_VERIFY_WITH_TIMEOUT(itemOrNull("gifStudio") == nullptr, 5000);
        QCOMPARE(studio->property("count").toInt(), 0);
    }

    // The information button in the bottom bar opens the popup with the picture's data.
    void theInformationButtonOfTheBottomBarOpensThePopup()
    {
        m_app.controller->openPath(file("info.png", picture(50, 30, Qt::cyan)));
        QTRY_COMPARE_WITH_TIMEOUT(m_app.controller->currentFileName(), QStringLiteral("info.png"), 10000);
        click("infoButton");
        QTRY_VERIFY_WITH_TIMEOUT(itemOrNull("infoPopupContent") != nullptr, 5000);
    }

private:
    QString file(const QString &name, const QImage &image)
    {
        const QString path = m_dir.filePath(name);
        [&] { QVERIFY(image.save(path)); }();
        return path;
    }

    // Three or two small pictures in the folder, the first one open.
    void openFolder(const QStringList &names)
    {
        QString first;
        int shade = 40;
        for (const QString &name : names) {
            const QString path = file(name, picture(100, 60, QColor(shade, 120, 200 - shade)));
            shade += 60;
            if (first.isEmpty())
                first = path;
        }
        m_app.controller->openPath(first);
        QTRY_COMPARE_WITH_TIMEOUT(m_app.controller->currentFileName(), names.first(), 15000);
        QTRY_COMPARE_WITH_TIMEOUT(m_app.folderModel->count(), int(names.size()), 10000);
    }

    // ---- finding and clicking controls by name
    static QQuickItem *findIn(QQuickItem *root, const QString &name)
    {
        if (root->objectName() == name)
            return root;
        for (QQuickItem *child : root->childItems())
            if (QQuickItem *found = findIn(child, name))
                return found;
        return nullptr;
    }

    // The visible control with that name (popups and dialogs live under the window's overlay, which is in the tree).
    QQuickItem *itemOrNull(const QString &name) const
    {
        QQuickItem *found = findIn(m_window->contentItem(), name);
        return found && found->isVisible() && found->window() == m_window ? found : nullptr;
    }

    QQuickItem *item(const QString &name)
    {
        QQuickItem *found = nullptr;
        [&] { QTRY_VERIFY2_WITH_TIMEOUT((found = itemOrNull(name)) != nullptr, qPrintable("no visible control named " + name), 10000); }();
        return found;
    }

    // A click in the middle of the control, once it is there, enabled and standing still (panels slide in and out: a
    // click on a moving target lands on nothing).
    void click(const QString &name)
    {
        QQuickItem *control = item(name);
        QTRY_VERIFY_WITH_TIMEOUT(control->isEnabled(), 5000);
        auto center = [control]() {
            return control->mapToScene(QPointF(control->width() / 2, control->height() / 2)).toPoint();
        };
        QPoint at = center();
        for (int i = 0; i < 50; ++i) { // up to ~3 s
            QTest::qWait(60);
            const QPoint now = center();
            if (now == at)
                break;
            at = now;
        }
        QTest::mouseMove(m_window, at);
        QTest::mouseClick(m_window, Qt::LeftButton, Qt::NoModifier, at);
        QTest::qWait(60); // let the click's consequences start
    }

    static int zoom(QQuickItem *canvas) { return QQmlProperty::read(canvas, "zoomPercent").toInt(); }

    QTemporaryDir m_dir;
    QQmlApplicationEngine *m_engine = nullptr;
    AppBootstrap::Objects m_app;
    QQuickWindow *m_window = nullptr;
};

QTEST_MAIN(TestUiFlows)
#include "test_ui_flows.moc"
