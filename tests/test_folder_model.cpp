#include "FolderModel.h"
#include <QtTest>

namespace {

QString makePng(const QDir &dir, const QString &name, int shade)
{
    QImage img(8, 8, QImage::Format_RGBA8888);
    img.fill(QColor(shade, shade, shade));
    const QString path = dir.filePath(name);
    img.save(path);
    return path;
}

QStringList namesIn(const QDir &dir)
{
    return dir.entryList(QDir::Files, QDir::Name);
}

} // namespace

// Renaming the files someone picked (not the whole folder).
class TestFolderModel : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmp;
    QDir dir() const { return QDir(m_tmp.path()); }

private slots:
    void init()
    {
        for (const QString &f : dir().entryList(QDir::Files))
            dir().remove(f);
    }

    // "FOTO.PNG" is a picture as much as "foto.png": on Linux a name filter would not match it.
    void picturesAreFoundWhateverTheCaseOfTheirExtension()
    {
        FolderModel model;
        const QString first = makePng(dir(), "FOTO.PNG", 10);
        makePng(dir(), "otra.Png", 20);
        QFile notes(dir().filePath("notas.txt"));
        QVERIFY(notes.open(QIODevice::WriteOnly));
        model.openFolderForFile(first);
        QCOMPARE(model.count(), 2);
    }

    // Going from one picture to the next in the same folder only moves the marker: the list is not
    // listed, sorted and reset again (that was a visible pause on big folders). A file added since
    // is still noticed, because the folder's modification time moved.
    void movingWithinTheFolderDoesNotRescanItButAChangedFolderIs()
    {
        FolderModel model;
        const QString a = makePng(dir(), "a.png", 10);
        const QString b = makePng(dir(), "b.png", 20);
        model.openFolderForFile(a);
        QSignalSpy resets(&model, &QAbstractItemModel::modelReset);

        model.openFolderForFile(b);
        QCOMPARE(resets.count(), 0);
        QCOMPARE(model.currentIndex(), 1);

        QTest::qWait(50); // let the folder's timestamp tick on coarse file systems
        makePng(dir(), "c.png", 30);
        model.openFolderForFile(b);
        QCOMPARE(resets.count(), 1);
        QCOMPARE(model.count(), 3);
        QCOMPARE(model.currentIndex(), 1);
    }

#ifdef Q_OS_LINUX
    // Linux keeps "A.png" and "a.png" apart: both are listed, and neither is taken for the other.
    void namesThatDifferOnlyInCaseAreTwoFilesOnLinux()
    {
        FolderModel model;
        const QString upper = makePng(dir(), "A.png", 10);
        makePng(dir(), "a.png", 20);
        model.openFolderForFile(upper);
        QCOMPARE(model.count(), 2);
    }
#endif

    void checkRenameBuildsTheNamesInTheGivenOrder()
    {
        FolderModel model;
        const QStringList paths = {makePng(dir(), "b.png", 10), makePng(dir(), "a.png", 20)};
        const QVariantMap r = model.checkRename(paths, QStringLiteral("viaje_"), 7, 3);
        QVERIFY(r.value("ok").toBool());
        QCOMPARE(r.value("names").toStringList(), QStringList({"viaje_007.png", "viaje_008.png"}));
    }

    void badBaseNamesAreRefused()
    {
        FolderModel model;
        const QStringList paths = {makePng(dir(), "a.png", 10)};
        QVERIFY(!model.checkRename(paths, QString(), 1, 1).value("ok").toBool());
        QVERIFY(!model.checkRename(paths, QStringLiteral("  "), 1, 1).value("ok").toBool());
        QVERIFY(!model.checkRename(paths, QStringLiteral("a/b"), 1, 1).value("ok").toBool());
        QVERIFY(!model.checkRename(paths, QStringLiteral("a?"), 1, 1).value("ok").toBool());
        QVERIFY(!model.checkRename(paths, QStringLiteral("a."), 1, 1).value("ok").toBool());
        QVERIFY(!model.checkRename({}, QStringLiteral("x"), 1, 1).value("ok").toBool());
    }

    // A new name that belongs to a file that is not being renamed is never overwritten.
    void aNameTakenByAnotherFileIsRefusedAndNothingMoves()
    {
        FolderModel model;
        const QString a = makePng(dir(), "a.png", 10);
        makePng(dir(), "x1.png", 99); // not chosen
        const QVariantMap r = model.renameFiles({a}, QStringLiteral("x"), 1, 1);
        QVERIFY(!r.value("ok").toBool());
        QVERIFY(!r.value("error").toString().isEmpty());
        QCOMPARE(namesIn(dir()), QStringList({"a.png", "x1.png"}));
    }

    // Two chosen files whose NEW names are each other's OLD names: nobody is overwritten.
    void newNamesMayEqualOtherChosenFilesOldNames()
    {
        FolderModel model;
        const QString second = makePng(dir(), "x2.png", 200);
        const QString first = makePng(dir(), "x1.png", 100);
        const QVariantMap r = model.renameFiles({second, first}, QStringLiteral("x"), 1, 1);
        QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("error").toString()));
        QCOMPARE(namesIn(dir()), QStringList({"x1.png", "x2.png"}));
        // the file that was x2 (shade 200) is now x1
        QCOMPARE(QImage(dir().filePath("x1.png")).pixelColor(0, 0).red(), 200);
        QCOMPARE(QImage(dir().filePath("x2.png")).pixelColor(0, 0).red(), 100);
    }

    void onlyTheChosenFilesAreTouchedAndTheListFollows()
    {
        FolderModel model;
        const QString a = makePng(dir(), "a.png", 1);
        const QString b = makePng(dir(), "b.png", 2);
        makePng(dir(), "c.png", 3);
        model.openFolderForFile(a);
        QSignalSpy newPath(&model, &FolderModel::currentFilePathChanged);

        const QVariantMap r = model.renameFiles({a, b}, QStringLiteral("foto"), 1, 2);
        QVERIFY(r.value("ok").toBool());
        QCOMPARE(r.value("renamed").toInt(), 2);
        QCOMPARE(namesIn(dir()), QStringList({"c.png", "foto01.png", "foto02.png"}));
        QCOMPARE(model.count(), 3);
        // the open picture (a.png) was renamed: the controller is told its new path
        QCOMPARE(newPath.size(), 1);
        QCOMPARE(QFileInfo(newPath.first().first().toString()).fileName(), QStringLiteral("foto01.png"));
    }

    // If one file cannot be renamed (open elsewhere), all that already moved is put back.
    void aFailureLeavesEverythingAsItWas()
    {
        FolderModel model;
        const QString a = makePng(dir(), "a.png", 1);
        const QString b = makePng(dir(), "b.png", 2);
        QFile lock(b);
        QVERIFY(lock.open(QIODevice::ReadOnly)); // Qt opens without delete-sharing: renaming it fails

        const QVariantMap r = model.renameFiles({a, b}, QStringLiteral("z"), 1, 1);
        if (r.value("ok").toBool())
            QSKIP("this system lets an open file be renamed");
        QVERIFY(!r.value("error").toString().isEmpty());
        lock.close();
        QCOMPARE(namesIn(dir()), QStringList({"a.png", "b.png"}));
        QCOMPARE(QImage(a).pixelColor(0, 0).red(), 1);
    }
};

QTEST_MAIN(TestFolderModel)
#include "test_folder_model.moc"
