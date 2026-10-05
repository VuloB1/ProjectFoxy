#include <QtTest>
#include <QImageReader>
#include "CollageStudio.h"
#include "PaneImageProvider.h"

// The collage maker's controller: what goes in the cells, the layouts and the dividing lines, moving a picture
// inside its cell without limits, the free layout, and writing the result.
class TestCollageStudio : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

    QString png(const QString &name, QColor c, int w = 120, int h = 80)
    {
        QImage img(w, h, QImage::Format_RGBA8888);
        img.fill(c);
        const QString path = m_dir.filePath(name);
        img.save(path);
        return path;
    }

    static QVariantMap cell(CollageStudio &s, int i) { return s.cells().at(i).toMap(); }

    bool exportAndWait(CollageStudio &s, const QString &path, QString *written = nullptr, QString *error = nullptr)
    {
        QSignalSpy done(&s, &CollageStudio::exportFinished);
        if (!s.exportTo(QUrl::fromLocalFile(path)))
            return false;
        if (!done.wait(30000))
            return false;
        if (written)
            *written = done.first().at(1).toString();
        if (error)
            *error = done.first().at(3).toString();
        return done.first().at(0).toBool();
    }

private slots:
    void initTestCase() { QVERIFY(m_dir.isValid()); }

    void picturesFillTheEmptyCellsAndThenAddCells()
    {
        PaneImageStore store;
        CollageStudio s(&store);
        QCOMPARE(s.cellCount(), 4);
        QStringList paths;
        for (int i = 0; i < 6; ++i)
            paths << png(QString("p%1.png").arg(i), QColor(i * 40, 100, 200));
        QCOMPARE(s.addPaths(paths), 6);
        QCOMPARE(s.cellCount(), 6); // two more cells were made for the extra pictures
        for (int i = 0; i < 6; ++i)
            QCOMPARE(cell(s, i).value("name").toString(), QString("p%1.png").arg(i));
        QCOMPARE(s.addPaths({m_dir.filePath("missing.png")}), 0);
        // never more than twelve
        QStringList many;
        for (int i = 0; i < 10; ++i)
            many << png(QString("m%1.png").arg(i), Qt::red);
        s.addPaths(many);
        QCOMPARE(s.cellCount(), 12);
    }

    void changingTheNumberOfCellsKeepsThePictures()
    {
        PaneImageStore store;
        CollageStudio s(&store);
        s.addPaths({png("a.png", Qt::red), png("b.png", Qt::green), png("c.png", Qt::blue), png("d.png", Qt::yellow)});
        s.setCellCount(3);
        QCOMPARE(s.cellCount(), 3);
        QCOMPARE(cell(s, 2).value("name").toString(), QString("c.png"));
        s.setCellCount(5);
        QCOMPARE(cell(s, 0).value("name").toString(), QString("a.png"));
        QVERIFY(cell(s, 4).value("empty").toBool());
        s.setCellCount(99);
        QCOMPARE(s.cellCount(), 12);
        s.setCellCount(0);
        QCOMPARE(s.cellCount(), 1);
        // the layouts on offer are those of the current number
        QCOMPARE(s.presets().size(), 1);
        s.setCellCount(6);
        QVERIFY(s.presets().size() >= 6);
        const QVariantMap first = s.presets().first().toMap();
        QCOMPARE(first.value("rects").toList().size(), 6);
        QCOMPARE(first.value("rects").toList().first().toList().size(), 4); // [x, y, w, h] each, not spliced into one list
    }

    void layoutsCanBePickedAndMosaicsMade()
    {
        PaneImageStore store;
        CollageStudio s(&store);
        s.setCellCount(5);
        const QVariantMap before = cell(s, 0);
        s.applyPreset(2);
        QCOMPARE(s.presetIndex(), 2);
        QVERIFY(cell(s, 0) != before || cell(s, 1) != cell(s, 0));
        const QVariantList a = s.cells();
        s.newMosaic();
        QCOMPARE(s.presetIndex(), -1);
        QVERIFY(s.cells() != a);
        QVERIFY(cell(s, 0).value("w").toDouble() > 0.05);
        s.applyPreset(99); // out of range: nothing
        QCOMPARE(s.presetIndex(), -1);
    }

    void aPictureMovesAndZoomsWithoutLimits()
    {
        PaneImageStore store;
        CollageStudio s(&store);
        s.addPaths({png("a.png", Qt::red)});
        const double w = cell(s, 0).value("w").toDouble();
        s.panCell(0, w * 0.5, 0);                                    // half a cell to the right
        QVERIFY(std::abs(cell(s, 0).value("panX").toDouble() - 0.5) < 1e-9);
        s.panCell(0, 100, 100);                                      // absurdly far
        QCOMPARE(cell(s, 0).value("panX").toDouble(), 3.0);         // never out of sight
        s.resetPicture(0);
        QCOMPARE(cell(s, 0).value("panX").toDouble(), 0.0);
        QCOMPARE(cell(s, 0).value("name").toString(), QString("a.png")); // still has its picture

        // smaller than the cell is allowed, bigger too, up to a point
        s.setCellProperty(0, "zoom", 0.3);
        QCOMPARE(cell(s, 0).value("zoom").toDouble(), 0.3);
        s.setCellProperty(0, "zoom", 99);
        QCOMPARE(cell(s, 0).value("zoom").toDouble(), 10.0);
        s.setCellProperty(0, "zoom", 0.0);
        QCOMPARE(cell(s, 0).value("zoom").toDouble(), 0.1);
        s.setCellProperty(0, "rotation", 400);
        QCOMPARE(cell(s, 0).value("rotation").toDouble(), 180.0);
        s.setCellProperty(0, "overflow", true);
        s.setCellProperty(0, "flipH", true);
        QVERIFY(cell(s, 0).value("overflow").toBool() && cell(s, 0).value("flipH").toBool());
        s.setCellProperty(0, "nonsense", 1); // ignored
    }

    void zoomingKeepsThePointUnderTheCursorWhere()
    {
        PaneImageStore store;
        CollageStudio s(&store);
        s.addPaths({png("a.png", Qt::red)});
        const QVariantMap c = cell(s, 0);
        const double cx = c.value("x").toDouble() + c.value("w").toDouble() / 2, cy = c.value("y").toDouble() + c.value("h").toDouble() / 2;
        // zooming at the middle of the cell leaves the picture where it is
        s.zoomCell(0, 2.0, cx, cy);
        QCOMPARE(cell(s, 0).value("zoom").toDouble(), 2.0);
        QVERIFY(std::abs(cell(s, 0).value("panX").toDouble()) < 1e-9);
        // at a point a quarter cell to the right of the middle: the picture shifts left by a quarter of the zoom step
        s.resetPicture(0);
        s.zoomCell(0, 2.0, cx + c.value("w").toDouble() * 0.25, cy);
        QVERIFY(std::abs(cell(s, 0).value("panX").toDouble() - (-0.25)) < 1e-9);
        QVERIFY(std::abs(cell(s, 0).value("panY").toDouble()) < 1e-9);
        // zoom limits
        for (int i = 0; i < 20; ++i)
            s.zoomCell(0, 2.0, cx, cy);
        QCOMPARE(cell(s, 0).value("zoom").toDouble(), 10.0);
    }

    void dividingLinesMoveTheCellsAroundThem()
    {
        PaneImageStore store;
        CollageStudio s(&store);
        // the 2 x 2 grid is the first layout of four
        QCOMPARE(s.presetIndex(), 0);
        const QVariantList dividers = s.dividers();
        QCOMPARE(dividers.size(), 2);
        int vertical = -1;
        for (int i = 0; i < dividers.size(); ++i)
            if (dividers[i].toMap().value("vertical").toBool())
                vertical = i;
        QVERIFY(vertical >= 0);
        QVERIFY(std::abs(dividers[vertical].toMap().value("pos").toDouble() - 0.5) < 0.01);
        QVERIFY(s.beginDividerDrag(vertical));
        s.dragDivider(0.3);
        QVERIFY(std::abs(s.dividers()[vertical].toMap().value("pos").toDouble() - 0.3) < 0.01);
        QVERIFY(cell(s, 0).value("w").toDouble() < cell(s, 1).value("w").toDouble()); // left cells narrower, right ones wider
        QVERIFY(cell(s, 2).value("w").toDouble() < cell(s, 3).value("w").toDouble());
        s.dragDivider(0.0);   // cannot squeeze a cell out of existence
        QVERIFY(cell(s, 0).value("w").toDouble() > 0.03);
        s.endDividerDrag();
        QCOMPARE(s.presetIndex(), -1); // no longer one of the ready-made layouts
        s.dragDivider(0.9);   // nothing is being dragged now
        QVERIFY(cell(s, 0).value("w").toDouble() < 0.3);
        QVERIFY(!s.beginDividerDrag(7));
    }

    void theFreeLayoutMovesAndResizesCellsOnTheirOwn()
    {
        PaneImageStore store;
        CollageStudio s(&store);
        s.setOption("free", true);
        QVERIFY(s.dividers().isEmpty());
        QVERIFY(!s.beginDividerDrag(0));
        const QVariantMap before = cell(s, 0), other = cell(s, 3);
        s.moveCell(0, 0.1, 0.05);
        QVERIFY(cell(s, 0).value("x").toDouble() > before.value("x").toDouble());
        QVERIFY(cell(s, 0).value("y").toDouble() > before.value("y").toDouble());
        QCOMPARE(cell(s, 3), other); // the others did not move
        s.moveCell(1, 10, 10);   // cannot leave the canvas
        QVERIFY(cell(s, 1).value("x").toDouble() + cell(s, 1).value("w").toDouble() <= 1.0 + 1e-9);
        // the bottom right handle (4) makes it bigger, the top left (0) smaller
        const double w = cell(s, 2).value("w").toDouble();
        s.resizeCell(2, 4, 0.1, 0.1);
        QVERIFY(cell(s, 2).value("w").toDouble() > w);
        const double w2 = cell(s, 2).value("w").toDouble();
        s.resizeCell(2, 0, 0.05, 0.05);
        QVERIFY(cell(s, 2).value("w").toDouble() < w2);
        s.resizeCell(2, 3, -5, 0); // cannot be made vanish: a few percent of the layout is the least
        QVERIFY(cell(s, 2).value("w").toDouble() > 0.015);
        // cells may overlap, and one can be brought in front of the others
        s.addPaths({png("z.png", Qt::red)});
        const QString first = cell(s, 0).value("name").toString();
        s.bringToFront(0);
        QCOMPARE(cell(s, s.cellCount() - 1).value("name").toString(), first);
        QCOMPARE(s.selected(), s.cellCount() - 1);
        // more cells in the free layout appear without disturbing the others
        const QVariantMap keep = cell(s, 1);
        s.setCellCount(6);
        QCOMPARE(cell(s, 1), keep);
    }

    void cellsAreFoundUnderThePoint()
    {
        PaneImageStore store;
        CollageStudio s(&store);
        QCOMPARE(s.cellAt(0.25, 0.25), 0);
        QCOMPARE(s.cellAt(0.75, 0.25), 1);
        QCOMPARE(s.cellAt(0.25, 0.75), 2);
        QCOMPARE(s.cellAt(0.75, 0.75), 3);
        QCOMPARE(s.cellAt(0.5, 0.5), -1); // in the gap
        QCOMPARE(s.cellAt(-0.2, 0.2), -1);
        s.select(2);
        QCOMPARE(s.selected(), 2);
        s.select(40);
        QCOMPARE(s.selected(), -1);
    }

    void picturesCanBeSwappedShuffledAndCleared()
    {
        PaneImageStore store;
        CollageStudio s(&store);
        s.addPaths({png("a.png", Qt::red), png("b.png", Qt::green), png("c.png", Qt::blue)});
        s.swapPictures(0, 2);
        QCOMPARE(cell(s, 0).value("name").toString(), QString("c.png"));
        QCOMPARE(cell(s, 2).value("name").toString(), QString("a.png"));
        QStringList before;
        for (int i = 0; i < 4; ++i)
            before << cell(s, i).value("name").toString();
        s.shufflePictures();
        QStringList after;
        for (int i = 0; i < 4; ++i)
            after << cell(s, i).value("name").toString();
        before.sort();
        after.sort();
        QCOMPARE(after, before); // the same pictures, in some other order
        s.setPicture(3, QUrl::fromLocalFile(png("d.png", Qt::yellow)).toString());
        QCOMPARE(cell(s, 3).value("name").toString(), QString("d.png"));
        s.clearPicture(3);
        QVERIFY(cell(s, 3).value("empty").toBool());
        s.clearPictures();
        for (int i = 0; i < 4; ++i)
            QVERIFY(cell(s, i).value("empty").toBool());
    }

    void optionsAreLimited()
    {
        PaneImageStore store;
        CollageStudio s(&store);
        s.setOption("sizeW", 99999);
        QCOMPARE(s.options().value("sizeW").toInt(), 8000);
        s.setOption("sizeH", 3);
        QCOMPARE(s.options().value("sizeH").toInt(), 100);
        s.setOption("margin", 90);
        QCOMPARE(s.options().value("margin").toDouble(), 25.0);
        s.setOption("radius", -4);
        QCOMPARE(s.options().value("radius").toDouble(), 0.0);
        s.setOption("format", 7);
        QCOMPARE(s.options().value("format").toInt(), 2);
        s.setOption("color1", 0x123456);
        QCOMPARE(s.options().value("color1").toInt(), 0x123456);
        s.setOption("nope", 1);
        QVERIFY(!s.options().contains("nope"));
        // the cells follow the margin and spacing
        s.setOption("sizeW", 1000);
        s.setOption("sizeH", 1000);
        s.setOption("margin", 0);
        s.setOption("spacing", 0);
        const double wide = cell(s, 0).value("w").toDouble();
        s.setOption("spacing", 10);
        QVERIFY(cell(s, 0).value("w").toDouble() < wide);
    }

    void thePreviewHasTheCanvasProportions()
    {
        PaneImageStore store;
        CollageStudio s(&store);
        s.setOption("sizeW", 1600);
        s.setOption("sizeH", 900);
        s.addPaths({png("a.png", Qt::red)});
        const QImage p = s.preview(400);
        QCOMPARE(p.size(), QSize(400, 225));
        QVERIFY(qRed(p.pixel(60, 60)) > 200 && qGreen(p.pixel(60, 60)) < 60); // the first cell holds the red picture
        QVERIFY(s.revision() > 0);
    }

    void writesPngJpegAndWebp()
    {
        PaneImageStore store;
        CollageStudio s(&store);
        s.setOption("sizeW", 600);
        s.setOption("sizeH", 400);
        s.addPaths({png("a.png", Qt::red), png("b.png", Qt::green)});
        QString written, error;

        s.setOption("format", 0);
        QVERIFY2(exportAndWait(s, m_dir.filePath("collage_png"), &written, &error), qPrintable(error));
        QCOMPARE(QFileInfo(written).fileName(), QString("collage_png.png"));
        QCOMPARE(QImage(written).size(), QSize(600, 400));

        s.setOption("format", 1);
        s.setOption("quality", 80);
        QVERIFY2(exportAndWait(s, m_dir.filePath("collage_jpg.jpg"), &written, &error), qPrintable(error));
        QCOMPARE(QImage(written).size(), QSize(600, 400));
        QVERIFY(!QImage(written).hasAlphaChannel());

        s.setOption("format", 2);
        QVERIFY2(exportAndWait(s, m_dir.filePath("collage_webp.webp"), &written, &error), qPrintable(error));
        QFile f(written);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray head = f.read(16);
        QVERIFY(head.startsWith("RIFF") && head.mid(8, 4) == "WEBP");
        QVERIFY(!s.exporting());
    }

    void anExportCanBeCancelled()
    {
        PaneImageStore store;
        CollageStudio s(&store);
        QStringList paths;
        for (int i = 0; i < 4; ++i)
            paths << png(QString("big%1.png").arg(i), QColor(i * 50, 90, 200), 1200, 900);
        s.addPaths(paths);
        s.setOption("sizeW", 8000);
        s.setOption("sizeH", 8000);
        s.setOption("shadow", 60);
        const QString path = m_dir.filePath("cancelled.png");
        QSignalSpy done(&s, &CollageStudio::exportFinished);
        QVERIFY(s.exportTo(QUrl::fromLocalFile(path)));
        QVERIFY(!s.exportTo(QUrl::fromLocalFile(path))); // one at a time
        s.cancelExport();
        QVERIFY(done.wait(60000));
        QVERIFY(!QFileInfo::exists(path)); // nothing half-written is left behind
    }

    void templatesKeepTheLayoutAndTheLookButNotThePictures()
    {
        const QString file = m_dir.filePath("templates.json");
        QFile::remove(file);
        PaneImageStore store;
        QVariantMap saved;
        QRectF secondCell;
        {
            CollageStudio s(&store);
            s.setTemplatesFile(file);
            QCOMPARE(s.templates().size(), 0);
            QVERIFY(!s.saveTemplate("   ")); // a name is needed
            s.setCellCount(3);
            s.applyPreset(1);
            s.setOption("spacing", 7.0);
            s.setOption("radius", 40.0);
            s.setOption("color1", 0x336699);
            s.setOption("format", 2);           // how the file is written is not part of the look
            s.setCellProperty(1, "rotation", 12.0);
            s.setCellProperty(1, "overflow", true);
            s.addPaths({png("t0.png", Qt::red)});
            secondCell = QRectF(cell(s, 1).value("x").toDouble(), cell(s, 1).value("y").toDouble(), cell(s, 1).value("w").toDouble(),
                                cell(s, 1).value("h").toDouble());
            QVERIFY(s.saveTemplate("Mi diseño"));
            QVERIFY(s.saveTemplate("mi DISEÑO")); // same name: replaced, not repeated
            QCOMPARE(s.templates().size(), 1);
            QVariantMap t = s.templates().at(0).toMap();
            QCOMPARE(t.value("count").toInt(), 3);
            QCOMPARE(t.value("rects").toList().size(), 3);
        }
        // a new session reads them back
        CollageStudio s(&store);
        s.setTemplatesFile(file);
        QCOMPARE(s.templates().size(), 1);
        const QString picture = png("t1.png", Qt::blue);
        s.addPaths({picture});
        QCOMPARE(s.cellCount(), 4);
        QVERIFY(s.applyTemplate(0));
        QCOMPARE(s.cellCount(), 3);
        QCOMPARE(s.options().value("spacing").toDouble(), 7.0);
        QCOMPARE(s.options().value("radius").toDouble(), 40.0);
        QCOMPARE(s.options().value("color1").toInt(), 0x336699);
        QCOMPARE(s.options().value("format").toInt(), 0); // untouched
        QCOMPARE(cell(s, 0).value("path").toString(), picture); // the picture stayed, in the template's first cell
        QVERIFY(cell(s, 1).value("empty").toBool());
        QCOMPARE(cell(s, 1).value("rotation").toDouble(), 12.0);
        QVERIFY(cell(s, 1).value("overflow").toBool());
        QVERIFY(qAbs(cell(s, 1).value("x").toDouble() - secondCell.x()) < 1e-6);
        QVERIFY(qAbs(cell(s, 1).value("w").toDouble() - secondCell.width()) < 1e-6);
        QVERIFY(!s.applyTemplate(5));
        s.deleteTemplate(0);
        QCOMPARE(s.templates().size(), 0);
        CollageStudio again(&store);
        again.setTemplatesFile(file);
        QCOMPARE(again.templates().size(), 0); // the deletion was written
    }

    void picturesChosenInTheFileDialogAreAdded()
    {
        PaneImageStore store;
        CollageStudio s(&store);
        const QString a = png("dlg a.png", Qt::red), b = png("dlg b.png", Qt::blue);
        QCOMPARE(s.addUrls({QUrl::fromLocalFile(a).toString(), QUrl::fromLocalFile(b).toString()}), 2);
        QCOMPARE(cell(s, 0).value("path").toString(), a);
        QCOMPARE(s.addUrls({QVariant::fromValue(QUrl::fromLocalFile(b))}), 1);
    }
};

QTEST_MAIN(TestCollageStudio)
#include "test_collage_studio.moc"
