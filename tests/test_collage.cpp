#include <QtTest>
#include "collage/Collage.h"

#include <cmath>
#include <map>

using namespace core::collage;

namespace {

QImage solid(int w, int h, QColor c)
{
    QImage img(w, h, QImage::Format_RGBA8888);
    img.fill(c);
    return img;
}

// Left half red, right half blue.
QImage halves(int w, int h)
{
    QImage img = solid(w, h, Qt::blue);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w / 2; ++x)
            img.setPixelColor(x, y, Qt::red);
    return img;
}

bool near(QRgb c, QColor want, int tol = 6)
{
    return std::abs(qRed(c) - want.red()) <= tol && std::abs(qGreen(c) - want.green()) <= tol && std::abs(qBlue(c) - want.blue()) <= tol;
}

struct Images {
    std::map<QString, QImage> byPath;
    std::vector<int> requests;
    ImageProvider provider()
    {
        return [this](const QString &path, int maxSide) {
            requests.push_back(maxSide);
            const auto it = byPath.find(path);
            if (it == byPath.end())
                return QImage();
            QImage img = it->second;
            if (maxSide > 0 && std::max(img.width(), img.height()) > maxSide)
                img = img.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            return img;
        };
    }
};

Cell cell(QRectF r, const QString &path = QString())
{
    Cell c;
    c.rect = r;
    c.content.path = path;
    return c;
}

// a clean style: nothing but the pictures
Style plainStyle(int side = 400)
{
    Style s;
    s.size = QSize(side, side);
    s.margin = 0;
    s.spacing = 0;
    s.color1 = QColor(255, 255, 255);
    return s;
}

} // namespace

class TestCollage : public QObject {
    Q_OBJECT

private slots:
    // ---- layouts -------------------------------------------------------------------------------------------------

    void everyLayoutTilesTheCanvas()
    {
        for (int n = 1; n <= 12; ++n) {
            const auto layouts = layoutsFor(n);
            QVERIFY2(!layouts.empty(), qPrintable(QString::number(n)));
            if (n >= 3)
                QVERIFY2(layouts.size() >= 6, qPrintable(QString("%1: %2").arg(n).arg(layouts.size())));
            for (const Layout &l : layouts) {
                QVERIFY2(int(l.rects.size()) == n, qPrintable(l.name));
                QVERIFY(!l.name.isEmpty());
                double area = 0;
                for (size_t i = 0; i < l.rects.size(); ++i) {
                    const QRectF &r = l.rects[i];
                    QVERIFY2(r.left() >= -1e-9 && r.top() >= -1e-9 && r.right() <= 1 + 1e-9 && r.bottom() <= 1 + 1e-9, qPrintable(l.name));
                    QVERIFY2(r.width() > 0.05 && r.height() > 0.05, qPrintable(QString("%1 %2: tiny cell").arg(n).arg(l.name)));
                    area += r.width() * r.height();
                    for (size_t j = i + 1; j < l.rects.size(); ++j) {
                        const QRectF both = r.intersected(l.rects[j]);
                        QVERIFY2(both.width() * both.height() < 1e-9, qPrintable(l.name));
                    }
                }
                QVERIFY2(std::abs(area - 1.0) < 1e-6, qPrintable(QString("%1 %2: %3").arg(n).arg(l.name).arg(area)));
            }
            // no two layouts are the same
            for (size_t a = 0; a < layouts.size(); ++a)
                for (size_t b = a + 1; b < layouts.size(); ++b)
                    QVERIFY2(layouts[a].rects != layouts[b].rects, qPrintable(layouts[a].name + " / " + layouts[b].name));
        }
        // a mosaic is the same for the same seed and another for another
        QCOMPARE(mosaicLayout(7, 3).rects, mosaicLayout(7, 3).rects);
        QVERIFY(mosaicLayout(7, 3).rects != mosaicLayout(7, 4).rects);
    }

    void cellsAreMarginAndSpacingAway()
    {
        Style s;
        s.margin = 5;   // 50 px of 1000
        s.spacing = 2;  // 20 px
        const QSize canvas(1000, 1000);
        const QRectF whole = cellPixelRect(QRectF(0, 0, 1, 1), s, canvas);
        QCOMPARE(whole, QRectF(50, 50, 900, 900));
        const QRectF left = cellPixelRect(QRectF(0, 0, 0.5, 1), s, canvas), right = cellPixelRect(QRectF(0.5, 0, 0.5, 1), s, canvas);
        QCOMPARE(left.left(), 50.0);
        QCOMPARE(right.right(), 950.0);
        QVERIFY(std::abs((right.left() - left.right()) - 20.0) < 1e-9); // exactly the spacing between them
        QCOMPARE(left.width(), right.width());
    }

    // ---- dividing lines ------------------------------------------------------------------------------------------

    void dividersAreFoundAndMoveTheCellsAroundThem()
    {
        std::vector<Cell> cells;
        for (const QRectF &r : layoutsFor(4).front().rects) // the first 4-picture layout
            cells.push_back(cell(r));
        // find the 2 x 2 grid among them
        std::vector<Cell> grid;
        for (const Layout &l : layoutsFor(4))
            if (l.name.startsWith("Filas 2 · 2")) {
                for (const QRectF &r : l.rects)
                    grid.push_back(cell(r));
                break;
            }
        QCOMPARE(int(grid.size()), 4);
        auto dividers = findDividers(grid);
        QCOMPARE(int(dividers.size()), 2); // one down the middle, one across it
        const auto vertical = std::find_if(dividers.begin(), dividers.end(), [](const Divider &d) { return d.vertical; });
        QVERIFY(vertical != dividers.end());
        QCOMPARE(vertical->pos, 0.5);
        QCOMPARE(vertical->from, 0.0);
        QCOMPARE(vertical->to, 1.0);
        QCOMPARE(int(vertical->before.size()), 2);
        QCOMPARE(int(vertical->after.size()), 2);

        // moving it moves the shared edge of all four cells, and nothing else
        const double got = moveDivider(grid, *vertical, 0.3);
        QCOMPARE(got, 0.3);
        for (const Cell &c : grid) {
            QVERIFY(std::abs(c.rect.left() - 0.3) < 1e-9 || std::abs(c.rect.right() - 0.3) < 1e-9 || c.rect.left() < 1e-9 || c.rect.right() > 1 - 1e-9);
        }
        QCOMPARE(grid[0].rect.right(), 0.3);
        QCOMPARE(grid[1].rect.left(), 0.3);
        QCOMPARE(grid[1].rect.right(), 1.0);
        QCOMPARE(grid[2].rect.right(), 0.3);
        // the cells keep their size to a minimum
        QVERIFY(moveDivider(grid, *vertical, 0.0) >= 0.06 - 1e-9);
        QVERIFY(moveDivider(grid, *vertical, 1.0) <= 0.94 + 1e-9);
        // the rest of the grid is still a tiling
        double area = 0;
        for (const Cell &c : grid)
            area += c.rect.width() * c.rect.height();
        QVERIFY(std::abs(area - 1.0) < 1e-9);
    }

    void aDividerOnlySpansWhereCellsMeetOnBothSides()
    {
        // one big cell on the left, two stacked on the right
        std::vector<Cell> cells{cell(QRectF(0, 0, 0.6, 1)), cell(QRectF(0.6, 0, 0.4, 0.5)), cell(QRectF(0.6, 0.5, 0.4, 0.5))};
        const auto dividers = findDividers(cells);
        QCOMPARE(int(dividers.size()), 2);
        for (const Divider &d : dividers) {
            if (d.vertical) {
                QCOMPARE(d.pos, 0.6);
                QCOMPARE(int(d.before.size()), 1);
                QCOMPARE(int(d.after.size()), 2);
            } else {
                QCOMPARE(d.pos, 0.5);
                QCOMPARE(d.from, 0.6); // only across the right-hand column
                QCOMPARE(d.to, 1.0);
            }
        }
        // a single cell has none
        QVERIFY(findDividers({cell(QRectF(0, 0, 1, 1))}).empty());
        // dragging the horizontal one touches only the two cells it separates
        for (const Divider &d : dividers)
            if (!d.vertical) {
                moveDivider(cells, d, 0.7);
                QCOMPARE(cells[0].rect, QRectF(0, 0, 0.6, 1));
                QCOMPARE(cells[1].rect.bottom(), 0.7);
                QCOMPARE(cells[2].rect.top(), 0.7);
            }
    }

    // ---- drawing -----------------------------------------------------------------------------------------------

    void picturesFillTheirCellsAndTheGapShowsTheBackground()
    {
        Images im;
        im.byPath["r"] = solid(100, 100, Qt::red);
        im.byPath["b"] = solid(100, 100, Qt::blue);
        Style s = plainStyle(400);
        s.margin = 5;   // 20 px
        s.spacing = 5;  // 20 px
        s.color1 = QColor(0, 255, 0);
        std::vector<Cell> cells{cell(QRectF(0, 0, 0.5, 1), "r"), cell(QRectF(0.5, 0, 0.5, 1), "b")};
        const QImage out = render(cells, s, im.provider(), QSize(400, 400));
        QCOMPARE(out.size(), QSize(400, 400));
        QVERIFY(near(out.pixel(100, 200), Qt::red, 3));
        QVERIFY(near(out.pixel(300, 200), Qt::blue, 3));
        QVERIFY(near(out.pixel(200, 200), QColor(0, 255, 0), 3)); // the gap between them
        QVERIFY(near(out.pixel(5, 5), QColor(0, 255, 0), 3));     // the margin
        QVERIFY(near(out.pixel(30, 30), Qt::red, 3));              // the cell starts right after the margin
    }

    void aPictureCanBeSmallerThanItsCell()
    {
        Images im;
        im.byPath["r"] = solid(100, 100, Qt::red);
        Style s = plainStyle(400);
        s.cellFill = QColor(0, 200, 0);
        Cell c = cell(QRectF(0, 0, 1, 1), "r");
        c.content.zoom = 0.5;
        const QImage out = render({c}, s, im.provider(), QSize(400, 400));
        QVERIFY(near(out.pixel(200, 200), Qt::red, 3));
        QVERIFY(near(out.pixel(20, 20), QColor(0, 200, 0), 3)); // not covered: the fill shows
        c.content.zoom = 1.0;
        QVERIFY(near(render({c}, s, im.provider(), QSize(400, 400)).pixel(20, 20), Qt::red, 3));
    }

    void aPictureCanBeMovedPastTheCellsEdge()
    {
        Images im;
        im.byPath["h"] = halves(100, 100);
        Style s = plainStyle(400);
        s.cellFill = QColor(0, 200, 0);
        Cell c = cell(QRectF(0, 0, 1, 1), "h");
        QVERIFY(near(render({c}, s, im.provider(), QSize(400, 400)).pixel(100, 200), Qt::red, 3));
        c.content.panX = 0.75; // three quarters of a cell to the right: the red half has left, the cell is left of the picture
        const QImage out = render({c}, s, im.provider(), QSize(400, 400));
        QVERIFY(near(out.pixel(20, 200), QColor(0, 200, 0), 3));   // nothing there now
        QVERIFY(near(out.pixel(380, 200), Qt::red, 3));            // the red half's edge has come in from the left
    }

    void mirroringAndTurningKeepTheCellCovered()
    {
        Images im;
        im.byPath["h"] = halves(100, 100);
        im.byPath["w"] = solid(160, 100, Qt::white);
        Style s = plainStyle(400);
        s.cellFill = QColor(0, 200, 0);
        Cell c = cell(QRectF(0, 0, 1, 1), "h");
        c.content.flipH = true;
        QVERIFY(near(render({c}, s, im.provider(), QSize(400, 400)).pixel(100, 200), Qt::blue, 3));
        QVERIFY(near(render({c}, s, im.provider(), QSize(400, 400)).pixel(300, 200), Qt::red, 3));
        // turned by 30 degrees a wide picture still covers the whole cell (corners included)
        Cell t = cell(QRectF(0, 0, 1, 1), "w");
        t.content.rotation = 30;
        const QImage out = render({t}, s, im.provider(), QSize(400, 400));
        for (QPoint p : {QPoint(2, 2), QPoint(397, 2), QPoint(2, 397), QPoint(397, 397)})
            QVERIFY2(near(out.pixel(p), Qt::white, 8), qPrintable(QString("%1,%2").arg(p.x()).arg(p.y())));
    }

    void aPictureMayOverflowItsCell()
    {
        Images im;
        im.byPath["r"] = solid(100, 100, Qt::red);
        im.byPath["b"] = solid(100, 100, Qt::blue);
        Style s = plainStyle(400);
        std::vector<Cell> cells{cell(QRectF(0, 0, 0.5, 1), "r"), cell(QRectF(0.5, 0, 0.5, 1), "b")};
        cells[0].content.zoom = 1.2;
        QVERIFY(near(render(cells, s, im.provider(), QSize(400, 400)).pixel(300, 200), Qt::blue, 3)); // cut at the cell's edge
        cells[0].content.overflow = true;
        const QImage out = render(cells, s, im.provider(), QSize(400, 400));
        QVERIFY(near(out.pixel(300, 200), Qt::red, 3));   // it now lies over the neighbour
        QVERIFY(near(out.pixel(390, 200), Qt::blue, 3));  // but not all the way: it is 480 px wide, centred on its cell
    }

    void cornersBordersAndShadows()
    {
        Images im;
        im.byPath["r"] = solid(100, 100, Qt::red);
        Style s = plainStyle(400);
        s.margin = 10;
        s.color1 = QColor(255, 255, 255);
        std::vector<Cell> cells{cell(QRectF(0, 0, 1, 1), "r")};
        // sharp corners
        QVERIFY(near(render(cells, s, im.provider(), QSize(400, 400)).pixel(42, 42), Qt::red, 3));
        // round ones: the very corner of the cell shows the background
        s.radius = 100;
        const QImage round = render(cells, s, im.provider(), QSize(400, 400));
        QVERIFY(near(round.pixel(42, 42), QColor(255, 255, 255), 8));
        QVERIFY(near(round.pixel(200, 200), Qt::red, 3));
        // an outline
        s.radius = 0;
        s.borderWidth = 5;      // 20 px
        s.borderColor = QColor(0, 0, 255);
        const QImage border = render(cells, s, im.provider(), QSize(400, 400));
        QVERIFY(near(border.pixel(50, 200), Qt::blue, 4));   // inside the 20 px band along the left
        QVERIFY(near(border.pixel(70, 200), Qt::red, 4));    // past it
        // a shadow down and to the right of the cell, none up and to the left
        s.borderWidth = 0;
        s.margin = 20;
        s.shadow = 100;
        s.shadowBlur = 0.5;
        s.shadowOffset = 3;
        const QImage shadow = render(cells, s, im.provider(), QSize(400, 400));
        QVERIFY(qRed(shadow.pixel(326, 326)) < 200 && qGreen(shadow.pixel(326, 326)) < 200); // darkened (white background)
        QVERIFY(near(shadow.pixel(10, 10), QColor(255, 255, 255), 3));
        QVERIFY(near(shadow.pixel(100, 100), Qt::red, 3));
    }

    void backgroundsCanBeGradientsOrTheBlurredPhoto()
    {
        Images im;
        im.byPath["r"] = solid(100, 100, QColor(200, 30, 30));
        Style s = plainStyle(400);
        s.margin = 25;
        std::vector<Cell> cells{cell(QRectF(0.3, 0.3, 0.4, 0.4), "r")};
        s.background = BackgroundMode::Gradient;
        s.color1 = QColor(255, 0, 0);
        s.color2 = QColor(0, 0, 255);
        s.gradientAngle = 0; // left to right
        const QImage g = render(cells, s, im.provider(), QSize(400, 400));
        QVERIFY(qRed(g.pixel(2, 200)) > 235 && qBlue(g.pixel(2, 200)) < 20);
        QVERIFY(qBlue(g.pixel(397, 200)) > 235 && qRed(g.pixel(397, 200)) < 20);
        s.background = BackgroundMode::Photo;
        const QImage p = render(cells, s, im.provider(), QSize(400, 400));
        QVERIFY(near(p.pixel(5, 5), QColor(200, 30, 30), 6)); // the photo's own colour fills the canvas
        QVERIFY(qAlpha(p.pixel(5, 5)) == 255);
    }

    void sharpPicturesAreAskedForWhenZoomedIn()
    {
        Images im;
        im.byPath["big"] = solid(4000, 3000, Qt::red);
        Style s = plainStyle(1000);
        Cell c = cell(QRectF(0, 0, 1, 1), "big");
        render({c}, s, im.provider(), QSize(1000, 1000));
        const int plain = *std::max_element(im.requests.begin(), im.requests.end());
        im.requests.clear();
        c.content.zoom = 4;
        render({c}, s, im.provider(), QSize(1000, 1000));
        const int zoomed = *std::max_element(im.requests.begin(), im.requests.end());
        QVERIFY2(zoomed > plain * 2, qPrintable(QString("%1 vs %2").arg(zoomed).arg(plain)));
        QVERIFY(plain >= 1000); // fitting a 4:3 picture to a square needs more than 1000 px along its long side
    }

    void anEmptyOrUnreadableCellKeepsItsFill()
    {
        Images im;
        Style s = plainStyle(200);
        s.cellFill = QColor(10, 120, 10);
        const QImage out = render({cell(QRectF(0, 0, 1, 1)), cell(QRectF(0, 0, 0.5, 0.5), "missing")}, s, im.provider(), QSize(200, 200));
        QVERIFY(near(out.pixel(150, 150), QColor(10, 120, 10), 3));
        QVERIFY(render({}, s, im.provider(), QSize(0, 10)).isNull());
    }

    void renderingCanBeCancelled()
    {
        Images im;
        im.byPath["r"] = solid(200, 200, Qt::red);
        std::vector<Cell> cells;
        for (int i = 0; i < 6; ++i)
            cells.push_back(cell(QRectF(i / 6.0, 0, 1 / 6.0, 1), "r"));
        std::atomic<bool> cancel{true};
        const QImage out = render(cells, plainStyle(300), im.provider(), QSize(300, 300), &cancel);
        QVERIFY(!out.isNull()); // an unfinished picture the caller drops
        QVERIFY(im.requests.empty()); // and no work was done for the cells
    }
};

QTEST_MAIN(TestCollage)
#include "test_collage.moc"
