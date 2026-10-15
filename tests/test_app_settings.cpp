#include "AppPaths.h"
#include "AppSettings.h"
#include "WindowGeometry.h"
#include <QtTest>

// "Modo pixel": the switch and its five extras are remembered one by one, and whoever had the
// old all-in-one switch on keeps every extra on. The tests run against Qt's test-mode config
// folder, never the real settings.ini.
class TestAppSettings : public QObject {
    Q_OBJECT

private:
    static QString iniPath()
    {
        return AppPaths::configDir() + QStringLiteral("/settings.ini");
    }

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void init() { QFile::remove(iniPath()); }
    void cleanup() { QFile::remove(iniPath()); }

    // A "portable.txt" next to the executable moves what the program remembers into "datos".
    void portableModeKeepsEverythingNextToTheExecutable()
    {
        const QString marker = QCoreApplication::applicationDirPath() + QStringLiteral("/portable.txt");
        QVERIFY(!AppPaths::portable());
        QFile f(marker);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        const bool on = AppPaths::portable();
        const QString config = AppPaths::configDir(), cache = AppPaths::cacheDir();
        QFile::remove(marker);
        QVERIFY(on);
        QVERIFY(config.endsWith(QStringLiteral("/datos")));
        QVERIFY(cache.endsWith(QStringLiteral("/datos/cache")));
        QVERIFY(!AppPaths::portable());
    }

    void theThumbnailBarKeepsItsValuesWithinRange()
    {
        {
            AppSettings s;
            QCOMPARE(s.stripThumbSize(), 88);
            QCOMPARE(s.stripColumns(), 1);
            QCOMPARE(s.stripMode(), QStringLiteral("manual"));
            s.setStripThumbSize(5000);
            QCOMPARE(s.stripThumbSize(), 200);
            s.setStripThumbSize(3);
            QCOMPARE(s.stripThumbSize(), 48);
            s.setStripColumns(9);
            QCOMPARE(s.stripColumns(), 4);
            s.setStripColumns(0);
            QCOMPARE(s.stripColumns(), 1);
            s.setStripMode(QStringLiteral("nonsense"));
            QCOMPARE(s.stripMode(), QStringLiteral("manual"));
            s.setStripThumbSize(120);
            s.setStripColumns(3);
            s.setStripMode(QStringLiteral("auto"));
        }
        AppSettings again;
        QCOMPARE(again.stripThumbSize(), 120);
        QCOMPARE(again.stripColumns(), 3);
        QCOMPARE(again.stripMode(), QStringLiteral("auto"));
    }

    // The settings of the program's old name are taken over once, never over newer ones.
    void settingsFromTheOldNameAreCopiedOnce()
    {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/foxy_migrate");
        QDir(dir).removeRecursively();
        QDir().mkpath(dir + QStringLiteral("/old"));
        const QString oldFile = dir + QStringLiteral("/old/settings.ini");
        const QString newFile = dir + QStringLiteral("/new/Foxy/settings.ini");
        { QSettings s(oldFile, QSettings::IniFormat); s.setValue(QStringLiteral("view/pixelMode"), true); }

        QVERIFY(AppPaths::migrateSettings(newFile, oldFile));
        QVERIFY(QFileInfo::exists(newFile));
        QVERIFY(QSettings(newFile, QSettings::IniFormat).value(QStringLiteral("view/pixelMode")).toBool());

        { QSettings s(newFile, QSettings::IniFormat); s.setValue(QStringLiteral("view/pixelMode"), false); }
        QVERIFY(!AppPaths::migrateSettings(newFile, oldFile)); // already there: untouched
        QVERIFY(!QSettings(newFile, QSettings::IniFormat).value(QStringLiteral("view/pixelMode")).toBool());
        QVERIFY(!AppPaths::migrateSettings(dir + QStringLiteral("/x.ini"), dir + QStringLiteral("/missing.ini")));
        QDir(dir).removeRecursively();
    }

    void everythingStartsOff()
    {
        AppSettings s;
        QVERIFY(!s.pixelMode());
        QVERIFY(!s.pixelIntegerZoom());
        QVERIFY(!s.pixelCheckerboard());
        QVERIFY(!s.pixelGrid());
        QVERIFY(!s.pixelSharpEdit());
    }

    void verticalSyncIsOnUntilTurnedOff()
    {
        {
            AppSettings s;
            QVERIFY(s.verticalSync());
            s.setVerticalSync(false);
        }
        AppSettings again;
        QVERIFY(!again.verticalSync());
    }

    void eachSwitchIsRememberedOnItsOwn()
    {
        {
            AppSettings s;
            s.setPixelMode(true);
            s.setPixelGrid(true);
        }
        AppSettings again;
        QVERIFY(again.pixelMode());
        QVERIFY(again.pixelGrid());
        QVERIFY(!again.pixelIntegerZoom());
        QVERIFY(!again.pixelCheckerboard());
        QVERIFY(!again.pixelSharpEdit());
    }

    void aChangeIsAnnouncedOnceAndTheSameValueIsIgnored()
    {
        AppSettings s;
        QSignalSpy spy(&s, &AppSettings::pixelCheckerboardChanged);
        s.setPixelCheckerboard(true);
        s.setPixelCheckerboard(true);
        QCOMPARE(spy.count(), 1);
        s.setPixelCheckerboard(false);
        QCOMPARE(spy.count(), 2);
    }

    void turningTheModeOffLeavesTheExtrasAsTheyWere()
    {
        AppSettings s;
        s.setPixelMode(true);
        s.setPixelIntegerZoom(true);
        s.setPixelMode(false);
        QVERIFY(s.pixelIntegerZoom()); // they only stop counting; the choice is kept
    }

    // The mode used to be a single switch stored as view/pixelArt.
    void whoHadTheOldSwitchOnKeepsEveryExtra()
    {
        {
            QSettings old(iniPath(), QSettings::IniFormat);
            old.setValue(QStringLiteral("view/pixelArt"), true);
        }
        AppSettings s;
        QVERIFY(s.pixelMode());
        QVERIFY(s.pixelIntegerZoom());
        QVERIFY(s.pixelCheckerboard());
        QVERIFY(s.pixelGrid());
        QVERIFY(s.pixelSharpEdit());
    }

    void theOldSwitchOffChangesNothing()
    {
        {
            QSettings old(iniPath(), QSettings::IniFormat);
            old.setValue(QStringLiteral("view/pixelArt"), false);
        }
        AppSettings s;
        QVERIFY(!s.pixelMode());
        QVERIFY(!s.pixelGrid());
    }

    void anExtraChosenAfterTheMigrationWinsOverIt()
    {
        {
            QSettings old(iniPath(), QSettings::IniFormat);
            old.setValue(QStringLiteral("view/pixelArt"), true);
            old.setValue(QStringLiteral("view/pixelGrid"), false); // chosen later, on its own
            old.setValue(QStringLiteral("view/pixelMode"), false);
        }
        AppSettings s;
        QVERIFY(!s.pixelMode());
        QVERIFY(!s.pixelGrid());
        QVERIFY(s.pixelSharpEdit()); // never touched: follows the old switch
    }

    // The window opens where it was, and what was saved on a bigger screen is brought down to the one connected now.
    void theWindowIsBroughtDownToTheScreensThereAre()
    {
        const QSize minimum(480, 320);
        const QList<QRect> big{{0, 0, 2560, 1400}};
        const QList<QRect> small{{0, 0, 1920, 1040}};
        // It fits: untouched.
        QCOMPARE(fitWindowToScreens({100, 80, 1280, 800}, big, minimum), QRect(100, 80, 1280, 800));
        // Saved at nearly 1440p, now on a 1080p screen: no larger than the screen and not hanging off it.
        QCOMPARE(fitWindowToScreens({200, 100, 2400, 1300}, small, minimum), QRect(0, 0, 1920, 1040));
        QCOMPARE(fitWindowToScreens({1000, 500, 1200, 800}, small, minimum), QRect(720, 240, 1200, 800));
        // Its screen is gone (second monitor unplugged): centred on the primary one.
        QCOMPARE(fitWindowToScreens({3000, 200, 800, 600}, small, minimum), QRect(560, 220, 800, 600));
        // Found on the second screen: stays there.
        const QList<QRect> two{{0, 0, 1920, 1040}, {1920, 0, 1920, 1080}};
        QCOMPARE(fitWindowToScreens({2000, 50, 900, 700}, two, minimum), QRect(2000, 50, 900, 700));
        // Nothing saved: nothing to apply.
        QVERIFY(fitWindowToScreens({}, small, minimum).isNull());
    }

    void theWindowStateIsRemembered()
    {
        {
            AppSettings s;
            QVERIFY(s.windowRect().isNull());
            QVERIFY(!s.windowMaximized());
            s.saveWindowState({40, 30, 1000, 700}, true);
        }
        AppSettings s;
        QCOMPARE(s.windowRect(), QRect(40, 30, 1000, 700));
        QVERIFY(s.windowMaximized());
    }
};

QTEST_MAIN(TestAppSettings)
#include "test_app_settings.moc"
