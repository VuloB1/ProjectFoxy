#pragma once

// The collage maker: a canvas divided into cells, a picture in each. What sets it apart from the usual
// templates is that nothing about a picture is clamped to its cell: it can be moved, zoomed (smaller than the
// cell too, which leaves the cell's background showing), turned, mirrored, and - when asked - allowed to spill
// over the cell's edge onto its neighbours. The cells themselves are plain rectangles, so a layout can be
// any of the presets, have its dividing lines dragged, or be laid out freely.

#include <QColor>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>
#include <atomic>
#include <functional>
#include <vector>

namespace core::collage {

struct Content {
    QString path;              // "" = an empty cell
    double zoom = 1.0;         // 1 = the picture just covers the cell; less leaves the cell's fill showing
    double panX = 0.0;         // how far it is moved from the middle, in cell widths
    double panY = 0.0;         // ... and in cell heights
    double rotation = 0.0;     // degrees
    bool flipH = false;
    bool flipV = false;
    bool overflow = false;     // not cut at the cell's edge
    bool operator==(const Content &) const = default;
};

struct Cell {
    QRectF rect;               // 0..1 of the area the layout covers
    Content content;
};

enum class BackgroundMode {
    Color,
    Gradient,
    Photo,   // the first picture, enlarged to cover the canvas and blurred
};

struct Style {
    QSize size = QSize(2000, 2000);
    BackgroundMode background = BackgroundMode::Color;
    QColor color1 = QColor(255, 255, 255);
    QColor color2 = QColor(190, 195, 210);
    double gradientAngle = 90.0;   // degrees, 90 = top to bottom
    double photoBlur = 8.0;        // % of the short side, for BackgroundMode::Photo
    double margin = 3.0;           // around the whole collage, % of the short side
    double spacing = 2.0;          // between cells, % of the short side
    double radius = 0.0;           // corner rounding of the cells, 0..100 (100 = as round as the cell allows)
    double borderWidth = 0.0;      // outline of each cell, % of the short side
    QColor borderColor = QColor(255, 255, 255);
    double shadow = 0.0;           // 0..100 opacity of a drop shadow under each cell
    double shadowBlur = 1.5;       // % of the short side
    double shadowOffset = 0.6;     // % of the short side, down and to the right
    QColor cellFill = QColor(0, 0, 0, 0);  // what shows where a picture does not reach (transparent: the background)
};

// Returns the picture of `path`, at most `maxSide` along its longer side (it may be smaller); null when it
// cannot be read. Called from the thread that renders.
using ImageProvider = std::function<QImage(const QString &path, int maxSide)>;

// The collage as one picture of `outSize` (straight RGBA, Format_RGBA8888). `cancel` is polled between cells.
QImage render(const std::vector<Cell> &cells, const Style &style, const ImageProvider &images, QSize outSize,
              const std::atomic<bool> *cancel = nullptr);

// Where a cell is on a canvas of `canvas` pixels: the layout area is the canvas less the margin, and each cell
// is pulled in by half the spacing on every side (so the gaps between cells are the spacing, and the outer edges
// are the margin).
QRectF cellPixelRect(const QRectF &cellRect, const Style &style, QSize canvas);
// The area the cells' 0..1 rectangles are measured in: the layout area grown by half the spacing on every side
// (a cell's own rectangle is what is left after it is pulled back in).
QRectF layoutAreaPx(const Style &style, QSize canvas);

// ---- layouts --------------------------------------------------------------------------------------------------------

struct Layout {
    QString name;
    std::vector<QRectF> rects;
};

// The ready-made layouts for `count` pictures (1 to 12), the most useful first.
std::vector<Layout> layoutsFor(int count);
// A mosaic made by cutting the canvas in two again and again; the same seed always gives the same one.
Layout mosaicLayout(int count, unsigned seed);

// ---- the dividing lines between cells -----------------------------------------------------------------------------------

// A straight line along which cells meet: those on one side all end at it, those on the other all start there.
// Moving it moves the shared edge of every one of them.
struct Divider {
    bool vertical = true;          // a vertical line (left | right), or a horizontal one (above / below)
    double pos = 0.0;              // where it is: x for a vertical one, y for a horizontal one (0..1)
    double from = 0.0, to = 1.0;   // how far it reaches along the other axis
    std::vector<int> before;       // cells ending at it (left of / above it)
    std::vector<int> after;        // cells starting at it
};
std::vector<Divider> findDividers(const std::vector<Cell> &cells);

// Moves a divider to `pos`, as far as every cell on both sides keeps at least `minSize` (0..1) across. Returns
// where it ended up.
double moveDivider(std::vector<Cell> &cells, const Divider &divider, double pos, double minSize = 0.06);

} // namespace core::collage
