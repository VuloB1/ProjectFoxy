#include "Collage.h"
#include "edit/EffectsCommon.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QTransform>
#include <algorithm>
#include <cmath>

namespace core::collage {

namespace {

constexpr int kProbeSide = 512;

QRectF layoutArea(const Style &style, QSize canvas)
{
    const double s = std::min(canvas.width(), canvas.height());
    const double m = style.margin / 100.0 * s;
    return QRectF(m, m, std::max(1.0, canvas.width() - 2 * m), std::max(1.0, canvas.height() - 2 * m));
}

QPainterPath roundedPath(const QRectF &r, double radius)
{
    QPainterPath p;
    const double rr = std::min(radius, std::min(r.width(), r.height()) / 2.0);
    if (rr > 0.01)
        p.addRoundedRect(r, rr, rr);
    else
        p.addRect(r);
    return p;
}

// The picture of `content` drawn into `target` (the cell, in the painter's own coordinates): it covers the
// cell at zoom 1 whatever its turn, and is then moved, scaled and mirrored as asked.
void drawContent(QPainter &p, const QImage &img, const Content &c, const QRectF &target)
{
    const double iw = img.width(), ih = img.height();
    if (iw < 1 || ih < 1)
        return;
    const double a = c.rotation * M_PI / 180.0;
    const double cs = std::abs(std::cos(a)), sn = std::abs(std::sin(a));
    const double cover = std::max((target.width() * cs + target.height() * sn) / iw, (target.width() * sn + target.height() * cs) / ih);
    const double scale = cover * std::max(0.01, c.zoom);
    p.save();
    p.translate(target.center() + QPointF(c.panX * target.width(), c.panY * target.height()));
    p.rotate(c.rotation);
    p.scale(c.flipH ? -scale : scale, c.flipV ? -scale : scale);
    p.translate(-iw / 2.0, -ih / 2.0);
    p.drawImage(QPointF(0, 0), img);
    p.restore();
}

// How many pixels along its longer side the picture needs to look sharp in `cell`, judged from a small copy.
int neededSide(const QImage &probe, const Content &c, const QRectF &cell)
{
    const double a = c.rotation * M_PI / 180.0;
    const double cs = std::abs(std::cos(a)), sn = std::abs(std::sin(a));
    const double iw = probe.width(), ih = probe.height();
    const double cover = std::max((cell.width() * cs + cell.height() * sn) / iw, (cell.width() * sn + cell.height() * cs) / ih);
    const double scale = cover * std::max(0.01, c.zoom);
    return int(std::ceil(std::max(iw, ih) * scale * 1.1));
}

QImage loadFor(const ImageProvider &images, const Content &c, const QRectF &cell)
{
    if (c.path.isEmpty())
        return {};
    QImage probe = images(c.path, kProbeSide);
    if (probe.isNull())
        return {};
    const int want = neededSide(probe, c, cell);
    if (want <= std::max(probe.width(), probe.height()))
        return probe;
    const QImage sharp = images(c.path, want);
    return sharp.isNull() ? probe : sharp;
}

// A blurred, tinted copy of the cells' shapes (as a small image, to be scaled up under them).
QImage shadowLayer(const std::vector<QRectF> &rects, const std::vector<double> &radii, QPointF offset, double sigma, QSize canvas,
                   const QColor &color, const std::atomic<bool> *cancel)
{
    const double k = std::clamp(4.0 / std::max(sigma, 1.0), 0.04, 1.0);
    const int lw = std::max(8, int(std::ceil(canvas.width() * k))), lh = std::max(8, int(std::ceil(canvas.height() * k)));
    QImage mask(lw, lh, QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);
    {
        QPainter p(&mask);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.scale(double(lw) / canvas.width(), double(lh) / canvas.height());
        for (size_t i = 0; i < rects.size(); ++i)
            p.fillPath(roundedPath(rects[i].translated(offset), radii[i]), Qt::white);
    }
    edit::fxk::Plane alpha(lw, lh, 1);
    for (int y = 0; y < lh; ++y) {
        const uint32_t *s = reinterpret_cast<const uint32_t *>(mask.constScanLine(y));
        uint8_t *d = alpha.row(y);
        for (int x = 0; x < lw; ++x)
            d[x] = uint8_t(s[x] >> 24);
    }
    const edit::fxk::Job job{cancel};
    const edit::fxk::Plane blurred = edit::fxk::gaussianBlur(job, alpha, sigma * k);
    QImage layer(lw, lh, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < lh; ++y) {
        uint32_t *d = reinterpret_cast<uint32_t *>(layer.scanLine(y));
        const uint8_t *s = blurred.row(y);
        for (int x = 0; x < lw; ++x) {
            const double a = s[x] / 255.0 * color.alphaF();
            d[x] = (uint32_t(a * 255.0 + 0.5) << 24) | (uint32_t(color.red() * a + 0.5) << 16) | (uint32_t(color.green() * a + 0.5) << 8)
                   | uint32_t(color.blue() * a + 0.5);
        }
    }
    return layer;
}

void paintBackground(QImage &canvas, const std::vector<Cell> &cells, const Style &style, const ImageProvider &images,
                     const std::atomic<bool> *cancel)
{
    QPainter p(&canvas);
    const QRectF all(QPointF(0, 0), QSizeF(canvas.size()));
    if (style.background == BackgroundMode::Gradient) {
        const double a = style.gradientAngle * M_PI / 180.0;
        const QPointF dir(std::cos(a), std::sin(a));
        const double half = 0.5 * (std::abs(dir.x()) * all.width() + std::abs(dir.y()) * all.height());
        QLinearGradient g(all.center() - dir * half, all.center() + dir * half);
        g.setColorAt(0.0, style.color1);
        g.setColorAt(1.0, style.color2);
        p.fillRect(all, g);
        return;
    }
    if (style.background == BackgroundMode::Photo) {
        for (const Cell &c : cells) {
            if (c.content.path.isEmpty())
                continue;
            const QImage src = images(c.content.path, 640);
            if (src.isNull())
                continue;
            const double k = std::min(1.0, 640.0 / std::max(all.width(), all.height()));
            const int lw = std::max(8, int(std::ceil(all.width() * k))), lh = std::max(8, int(std::ceil(all.height() * k)));
            const double cover = std::max(double(lw) / src.width(), double(lh) / src.height());
            const QImage big = src.convertToFormat(QImage::Format_RGBA8888)
                                   .scaled(std::max(lw, int(std::ceil(src.width() * cover))), std::max(lh, int(std::ceil(src.height() * cover))),
                                           Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            edit::fxk::Plane plane(lw, lh, 4);
            const int cx = (big.width() - lw) / 2, cy = (big.height() - lh) / 2;
            for (int y = 0; y < lh; ++y) {
                const uint8_t *s = big.constScanLine(y + cy) + size_t(cx) * 4;
                std::copy_n(s, size_t(lw) * 4, plane.row(y));
                uint8_t *d = plane.row(y);
                for (int x = 0; x < lw; ++x) { // premultiplied, so a transparent pixel's colour does not bleed
                    const int al = d[x * 4 + 3];
                    d[x * 4] = uint8_t((d[x * 4] * al + 127) / 255);
                    d[x * 4 + 1] = uint8_t((d[x * 4 + 1] * al + 127) / 255);
                    d[x * 4 + 2] = uint8_t((d[x * 4 + 2] * al + 127) / 255);
                }
            }
            const double sigma = style.photoBlur / 100.0 * 0.25 * std::min(canvas.width(), canvas.height()) * k;
            const edit::fxk::Job job{cancel};
            const edit::fxk::Plane blurred = edit::fxk::gaussianBlur(job, plane, std::max(0.5, sigma));
            QImage layer(lw, lh, QImage::Format_ARGB32_Premultiplied);
            for (int y = 0; y < lh; ++y) {
                const uint8_t *s = blurred.row(y);
                uint32_t *d = reinterpret_cast<uint32_t *>(layer.scanLine(y));
                for (int x = 0; x < lw; ++x)
                    d[x] = (uint32_t(s[x * 4 + 3]) << 24) | (uint32_t(s[x * 4]) << 16) | (uint32_t(s[x * 4 + 1]) << 8) | uint32_t(s[x * 4 + 2]);
            }
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            p.drawImage(all, layer);
            return;
        }
    }
    p.fillRect(all, style.color1);
}

} // namespace

QRectF layoutAreaPx(const Style &style, QSize canvas)
{
    const double s = std::min(canvas.width(), canvas.height());
    const double gap = style.spacing / 100.0 * s;
    // grown by half the gap on every side so that, once each cell is pulled in by half the gap, the outer edges
    // are exactly the margin and the gaps between cells are exactly the spacing
    return layoutArea(style, canvas).adjusted(-gap / 2, -gap / 2, gap / 2, gap / 2);
}

QRectF cellPixelRect(const QRectF &cellRect, const Style &style, QSize canvas)
{
    const double s = std::min(canvas.width(), canvas.height());
    const double gap = style.spacing / 100.0 * s;
    const QRectF area = layoutAreaPx(style, canvas);
    const QRectF px(area.x() + cellRect.x() * area.width(), area.y() + cellRect.y() * area.height(), cellRect.width() * area.width(),
                    cellRect.height() * area.height());
    QRectF r = px.adjusted(gap / 2, gap / 2, -gap / 2, -gap / 2);
    if (r.width() < 1 || r.height() < 1)
        r = QRectF(px.center() - QPointF(0.5, 0.5), QSizeF(1, 1));
    return r;
}

QImage render(const std::vector<Cell> &cells, const Style &style, const ImageProvider &images, QSize outSize, const std::atomic<bool> *cancel)
{
    if (outSize.width() < 1 || outSize.height() < 1)
        return {};
    const double s = std::min(outSize.width(), outSize.height());
    QImage canvas(outSize, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);
    paintBackground(canvas, cells, style, images, cancel);

    std::vector<QRectF> rects;
    std::vector<double> radii;
    for (const Cell &c : cells) {
        const QRectF r = cellPixelRect(c.rect, style, outSize);
        rects.push_back(r);
        radii.push_back(style.radius / 100.0 * std::min(r.width(), r.height()) / 2.0);
    }

    // the shadow of every cell, under all of them
    if (style.shadow > 0.0 && !cells.empty()) {
        const double sigma = std::max(0.5, style.shadowBlur / 100.0 * s * 0.5);
        const double off = style.shadowOffset / 100.0 * s;
        QColor shadowColor(0, 0, 0);
        shadowColor.setAlphaF(std::clamp(style.shadow / 100.0, 0.0, 1.0));
        const QImage layer = shadowLayer(rects, radii, QPointF(off, off), sigma, outSize, shadowColor, cancel);
        QPainter p(&canvas);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(QRectF(QPointF(0, 0), QSizeF(outSize)), layer);
    }

    // the cells; those that spill over their edge go last, so that they lie over the others
    std::vector<size_t> order;
    for (size_t i = 0; i < cells.size(); ++i)
        if (!cells[i].content.overflow)
            order.push_back(i);
    for (size_t i = 0; i < cells.size(); ++i)
        if (cells[i].content.overflow)
            order.push_back(i);

    for (size_t i : order) {
        if (cancel && cancel->load())
            return canvas.convertToFormat(QImage::Format_RGBA8888);
        const Cell &cell = cells[i];
        const QRectF r = rects[i];
        const QImage img = loadFor(images, cell.content, r);

        // the cell: its fill and its picture, cut to its (rounded) shape
        const QRect box = r.toAlignedRect().adjusted(-1, -1, 1, 1);
        QImage layer(box.size(), QImage::Format_ARGB32_Premultiplied);
        layer.fill(style.cellFill);
        const QRectF local = r.translated(-box.topLeft());
        {
            QPainter p(&layer);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            if (!img.isNull())
                drawContent(p, img, cell.content, local);
            p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
            QPainterPath outside;
            outside.addRect(QRectF(QPointF(-2, -2), QSizeF(box.width() + 4, box.height() + 4)));
            outside.addPath(roundedPath(local, radii[i]));
            outside.setFillRule(Qt::OddEvenFill);
            p.fillPath(outside, Qt::black);
        }
        // the layer's fill reaches only to the edge of the cell: whatever the picture did past it was cut
        QPainter p(&canvas);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(box.topLeft(), layer);
        if (cell.content.overflow && !img.isNull())
            drawContent(p, img, cell.content, r); // the same picture again, this time not cut at the cell's edge
        if (style.borderWidth > 0.0) {
            const double bw = style.borderWidth / 100.0 * s;
            QPen pen(style.borderColor, bw);
            pen.setJoinStyle(Qt::MiterJoin);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawPath(roundedPath(r.adjusted(bw / 2, bw / 2, -bw / 2, -bw / 2), std::max(0.0, radii[i] - bw / 2)));
        }
    }
    return canvas.convertToFormat(QImage::Format_RGBA8888);
}

} // namespace core::collage
