#include "AppPaths.h"
#include "AppSettings.h"
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

    void everythingStartsOff()
    {
        AppSettings s;
        QVERIFY(!s.pixelMode());
        QVERIFY(!s.pixelIntegerZoom());
        QVERIFY(!s.pixelCheckerboard());
        QVERIFY(!s.pixelGrid());
        QVERIFY(!s.pixelReadout());
        QVERIFY(!s.pixelSharpEdit());
    }

    void eachSwitchIsRememberedOnItsOwn()
    {
        {
            AppSettings s;
            s.setPixelMode(true);
            s.setPixelGrid(true);
            s.setPixelReadout(true);
        }
        AppSettings again;
        QVERIFY(again.pixelMode());
        QVERIFY(again.pixelGrid());
        QVERIFY(again.pixelReadout());
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
        QVERIFY(s.pixelReadout());
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
        QVERIFY(s.pixelReadout()); // never touched: follows the old switch
    }
};

QTEST_MAIN(TestAppSettings)
#include "test_app_settings.moc"
