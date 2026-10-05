// The picture frame of the Recortar tool ("Marco"): the picture is cut to a shape (a rectangle with
// round / soft / cut / hollow corners, an ellipse, a hexagon, a heart...), gets an outline, an extra
// margin, a drop shadow and a background, and the canvas grows to hold all of it. It is a hidden
// effect ("frame", driven by qml/FramePanel.qml) so that it previews, cancels, applies and undoes
// like the rest.
//
// Every distance is a percentage of the picture's SHORT side, so the same settings frame a 1600 px
// preview and the full-size photo the same way.
//
// How it is drawn (one canvas-sized layer, everything else is small):
//   1. the picture is copied into the canvas and everything outside the shape is erased (the edge is
//      anti-aliased by coverage, so a rounded corner has no jagged edge);
//   2. the outline inside the shape (if asked for) is painted over the picture only where it is;
//   3. then, each one BEHIND what is already there: the outline outside the shape, the shadow (drawn
//      small, blurred, scaled up) and the background. Painting behind (DestinationOver) is what keeps
//      the seam between picture and outline free of the light line that two half-covered edge pixels
//      would leave.

#include "EffectsCommon.h"
#include "Resample.h"

#include <QColor>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QTransform>

namespace core::edit::fxk {

namespace {

enum Param {
    kShape, kStyle, kRadius, kCorners, kStroke, kStrokeColor, kStrokeOpacity, kStrokeInside, kMargin, kKeepSize,
    kShadow, kShadowColor, kShadowDist, kShadowBlur, kShadowAngle, kBg, kBgColor, kBgColor2, kBgAngle, kBgOpacity,
    kBgBlur, kParamCount
};
enum Shape { kRect, kEllipse, kHexagon, kOctagon, kDiamond, kTriangle, kStar, kHeart };
enum Style { kRound, kSmooth, kChamfer, kConcave };
enum Background { kBgColorMode, kBgGradient, kBgTransparent, kBgPhoto };

// What the canvas is never allowed to exceed (a margin of 50% plus a shadow on a 40 MP photo would otherwise
// ask for a gigapixel): beyond it the whole composition is drawn smaller.
constexpr double kMaxCanvasPixels = 100e6;
constexpr double kMaxCanvasSide = 20000.0;
// How far (in outline widths) the mitred corner of an outline may stick out.
constexpr double kMaxMiter = 2.6;

using Ring = std::vector<QPointF>;

QPointF unitVector(QPointF v)
{
    const double len = std::hypot(v.x(), v.y());
    return len > 1e-12 ? QPointF(v.x() / len, v.y() / len) : QPointF(0, 0);
}
double dot(QPointF a, QPointF b) { return a.x() * b.x() + a.y() * b.y(); }
double lengthOf(QPointF v) { return std::hypot(v.x(), v.y()); }

// The outline of a shape that fills the rectangle (0,0)-(w,h), as a closed polygon.
Ring shapeVertices(int shape, double w, double h)
{
    Ring r;
    switch (shape) {
    case kEllipse: {
        const int n = clampi(int(kPi * (w + h) / 12.0), 96, 1440);
        for (int i = 0; i < n; ++i) {
            const double a = 2.0 * kPi * i / n;
            r.push_back({w * 0.5 + w * 0.5 * std::cos(a), h * 0.5 + h * 0.5 * std::sin(a)});
        }
        break;
    }
    case kHexagon:
        r = {{0, h * 0.5}, {w * 0.25, 0}, {w * 0.75, 0}, {w, h * 0.5}, {w * 0.75, h}, {w * 0.25, h}};
        break;
    case kOctagon: {
        const double c = 1.0 / (2.0 + std::sqrt(2.0)); // the corner cut of a regular octagon in a unit square
        r = {{w * c, 0}, {w * (1 - c), 0}, {w, h * c}, {w, h * (1 - c)}, {w * (1 - c), h}, {w * c, h}, {0, h * (1 - c)}, {0, h * c}};
        break;
    }
    case kDiamond:
        r = {{w * 0.5, 0}, {w, h * 0.5}, {w * 0.5, h}, {0, h * 0.5}};
        break;
    case kTriangle:
        r = {{w * 0.5, 0}, {w, h}, {0, h}};
        break;
    case kStar: {
        // five points, the inner radius 0.4 of the outer one; stretched to fill the rectangle
        constexpr double kInner = 0.4;
        const double x0 = -std::sin(2.0 * kPi / 5.0), x1 = -x0;      // leftmost / rightmost tip
        const double y0 = -1.0, y1 = -std::cos(kPi * 4.0 / 5.0);     // top tip / the lower tips
        for (int i = 0; i < 10; ++i) {
            const double a = -kPi / 2.0 + kPi * i / 5.0;
            const double rad = i % 2 == 0 ? 1.0 : kInner;
            const double x = rad * std::cos(a), y = rad * std::sin(a);
            r.push_back({(x - x0) / (x1 - x0) * w, (y - y0) / (y1 - y0) * h});
        }
        break;
    }
    case kHeart: {
        constexpr int n = 240;
        double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
        std::vector<QPointF> raw;
        for (int i = 0; i < n; ++i) {
            const double t = 2.0 * kPi * i / n;
            const double x = 16.0 * std::pow(std::sin(t), 3.0);
            const double y = 13.0 * std::cos(t) - 5.0 * std::cos(2 * t) - 2.0 * std::cos(3 * t) - std::cos(4 * t);
            raw.push_back({x, -y}); // y points down on screen
            minX = std::min(minX, x); maxX = std::max(maxX, x);
            minY = std::min(minY, -y); maxY = std::max(maxY, -y);
        }
        for (const QPointF &p : raw)
            r.push_back({(p.x() - minX) / (maxX - minX) * w, (p.y() - minY) / (maxY - minY) * h});
        break;
    }
    default:
        r = {{0, 0}, {w, 0}, {w, h}, {0, h}};
        break;
    }
    return r;
}

// +1 when the polygon runs one way (a rectangle TL, TR, BR, BL), -1 the other way.
double orientation(const Ring &r)
{
    double area = 0;
    for (size_t i = 0; i < r.size(); ++i) {
        const QPointF a = r[i], b = r[(i + 1) % r.size()];
        area += a.x() * b.y() - b.x() * a.y();
    }
    return area >= 0 ? 1.0 : -1.0;
}

// How far from each vertex its corner treatment starts along the two edges. `radius` (0..100) is the
// share of half the shorter of the two edges, so 100 rounds a corner as far as its neighbours allow
// (a square becomes a circle).
std::vector<double> cornerDistances(const Ring &r, int shape, double radius, int cornerMask)
{
    const size_t n = r.size();
    std::vector<double> d(n, 0.0);
    if (shape == kEllipse || shape == kHeart || radius <= 0.0)
        return d;
    for (size_t i = 0; i < n; ++i) {
        const double a = lengthOf(r[i] - r[(i + n - 1) % n]);
        const double b = lengthOf(r[(i + 1) % n] - r[i]);
        d[i] = radius / 100.0 * 0.5 * std::min(a, b);
        if (shape == kRect && !((cornerMask >> int(i)) & 1))
            d[i] = 0.0;
    }
    return d;
}

// The polygon moved outward by `dist` (inward when negative), corners kept sharp (mitred), with the corner
// distances adjusted so that a round corner stays a round corner of the right radius.
void offsetRing(const Ring &in, const std::vector<double> &d, int style, double dist, Ring &out, std::vector<double> &outD)
{
    const size_t n = in.size();
    out.assign(n, QPointF());
    outD.assign(n, 0.0);
    const double sgn = orientation(in);
    for (size_t i = 0; i < n; ++i) {
        const QPointF p = in[i], prev = in[(i + n - 1) % n], next = in[(i + 1) % n];
        const QPointF t1 = unitVector(p - prev), t2 = unitVector(next - p);
        const QPointF n1(t1.y() * sgn, -t1.x() * sgn), n2(t2.y() * sgn, -t2.x() * sgn);
        const double k = std::max(0.05, 1.0 + dot(n1, n2));
        QPointF move((n1.x() + n2.x()) * dist / k, (n1.y() + n2.y()) * dist / k);
        // a very sharp tip or notch would throw the mitred corner far away: cut it off
        const double longest = kMaxMiter * std::abs(dist);
        if (lengthOf(move) > longest)
            move *= longest / lengthOf(move);
        out[i] = p + move;

        if (d[i] <= 0.0) {
            outD[i] = 0.0;
            continue;
        }
        const double cosTheta = std::clamp(dot(unitVector(prev - p), unitVector(next - p)), -1.0, 1.0);
        const double theta = std::acos(cosTheta);
        const double half = std::max(theta * 0.5, 1e-3);
        const double cross = (t1.x() * t2.y() - t1.y() * t2.x()) * sgn;
        const double s = cross >= 0 ? 1.0 : -1.0; // a reflex vertex grows its fillet the other way
        double nd = d[i];
        switch (style) {
        case kChamfer:
            nd += s * dist * (1.0 / std::sin(half) - 1.0) / std::max(std::cos(half), 1e-3);
            break;
        case kConcave:
            nd -= s * dist;
            break;
        default: // round, soft: the fillet radius grows by dist
            nd += s * dist / std::tan(half);
            break;
        }
        outD[i] = std::max(0.0, nd);
    }
    // never past what the (new) neighbours allow
    for (size_t i = 0; i < n; ++i) {
        const double a = lengthOf(out[i] - out[(i + n - 1) % n]);
        const double b = lengthOf(out[(i + 1) % n] - out[i]);
        outD[i] = std::min(outD[i], 0.5 * std::min(a, b));
    }
}

QPainterPath buildPath(const Ring &v, const std::vector<double> &dist, int style)
{
    const size_t n = v.size();
    QPainterPath path;
    if (n < 3)
        return path;
    std::vector<QPointF> A(n), B(n), C1(n), C2(n);
    std::vector<char> kind(n, 0); // 0 sharp, 1 curve, 2 straight cut
    for (size_t i = 0; i < n; ++i) {
        const QPointF p = v[i], prev = v[(i + n - 1) % n], next = v[(i + 1) % n];
        A[i] = B[i] = p;
        double d = dist[i];
        if (d <= 0.01)
            continue;
        const QPointF u = unitVector(prev - p), w = unitVector(next - p);
        const double edge = 0.5 * std::min(lengthOf(prev - p), lengthOf(next - p));
        d = std::min(d, edge);
        const double theta = std::acos(std::clamp(dot(u, w), -1.0, 1.0));
        if (theta > kPi - 0.02 || theta < 0.02)
            continue; // a straight run (or a needle): nothing to round
        switch (style) {
        case kChamfer:
            A[i] = p + u * d;
            B[i] = p + w * d;
            kind[i] = 2;
            break;
        case kSmooth: {
            const double e = std::min(d * 1.28, edge);
            A[i] = p + u * e;
            B[i] = p + w * e;
            C1[i] = p + u * (0.22 * e);
            C2[i] = p + w * (0.22 * e);
            kind[i] = 1;
            break;
        }
        case kConcave: {
            const double h = 4.0 / 3.0 * std::tan(theta / 4.0) * d;
            A[i] = p + u * d;
            B[i] = p + w * d;
            const QPointF dir0 = unitVector(w - u * dot(w, u));
            const QPointF dir3 = unitVector(u - w * dot(u, w));
            C1[i] = A[i] + dir0 * h;
            C2[i] = B[i] + dir3 * h;
            kind[i] = 1;
            break;
        }
        default: { // a circular arc tangent to both edges
            const double radius = d * std::tan(theta * 0.5);
            const double h = 4.0 / 3.0 * std::tan((kPi - theta) * 0.25) * radius;
            A[i] = p + u * d;
            B[i] = p + w * d;
            C1[i] = A[i] - u * h;
            C2[i] = B[i] - w * h;
            kind[i] = 1;
            break;
        }
        }
    }
    auto corner = [&](size_t i) {
        if (kind[i] == 1)
            path.cubicTo(C1[i], C2[i], B[i]);
        else if (kind[i] == 2)
            path.lineTo(B[i]);
    };
    path.moveTo(B[0]);
    for (size_t i = 1; i < n; ++i) {
        path.lineTo(A[i]);
        corner(i);
    }
    path.lineTo(A[0]);
    corner(0);
    path.closeSubpath();
    return path;
}

QRectF boundsOf(const Ring &r)
{
    double x0 = 1e18, y0 = 1e18, x1 = -1e18, y1 = -1e18;
    for (const QPointF &p : r) {
        x0 = std::min(x0, p.x()); x1 = std::max(x1, p.x());
        y0 = std::min(y0, p.y()); y1 = std::max(y1, p.y());
    }
    return QRectF(QPointF(x0, y0), QPointF(x1, y1));
}

QColor colorOf(double packed, double opacityPercent)
{
    const Rgb c = unpackColor(packed);
    return QColor(int(c.r + 0.5), int(c.g + 0.5), int(c.b + 0.5), int(clampd(opacityPercent, 0.0, 100.0) * 2.55 + 0.5));
}

// Where everything goes. Lengths are in picture pixels ("natural" units); the canvas is then drawn at
// `scale` (1, unless the picture is being kept at its size or the canvas would be absurdly large).
struct Layout {
    double s = 1;           // the short side
    Ring ring;              // the shape
    std::vector<double> d;
    Ring outerRing;         // the shape plus the outline (== ring when the outline is inside or absent)
    std::vector<double> outerD;
    double stroke = 0;      // outline width, natural px
    bool strokeInside = false;
    QRectF frameBox;        // bounds of outerRing
    QPointF shadowOffset;
    double shadowBlur = 0;  // natural px (the blur's radius)
    bool hasShadow = false;
    QPointF origin;         // natural position of the canvas's top-left corner
    double scale = 1;
    QSize size;
};

Layout computeLayout(int w, int h, const EffectValues &v, int style)
{
    Layout L;
    L.s = std::min(w, h);
    const int shape = clampi(int(std::lround(v[kShape])), kRect, kHeart);
    L.ring = shapeVertices(shape, w, h);
    L.d = cornerDistances(L.ring, shape, v[kRadius], int(std::lround(v[kCorners])));
    L.stroke = v[kStroke] / 100.0 * L.s;
    L.strokeInside = v[kStrokeInside] >= 0.5;
    if (L.stroke > 0.01 && !L.strokeInside)
        offsetRing(L.ring, L.d, style, L.stroke, L.outerRing, L.outerD);
    else {
        L.outerRing = L.ring;
        L.outerD = L.d;
    }
    L.frameBox = boundsOf(L.outerRing);

    const double margin = v[kMargin] / 100.0 * L.s;
    QRectF canvas = L.frameBox.adjusted(-margin, -margin, margin, margin);

    L.hasShadow = v[kShadow] > 0.0;
    if (L.hasShadow) {
        const double dist = v[kShadowDist] / 100.0 * L.s;
        const double angle = v[kShadowAngle] * kPi / 180.0;
        L.shadowOffset = QPointF(std::cos(angle) * dist, std::sin(angle) * dist);
        L.shadowBlur = v[kShadowBlur] / 100.0 * L.s;
        const double e = 1.5 * L.shadowBlur; // three sigma, with sigma = half the radius
        canvas = canvas.united(L.frameBox.translated(L.shadowOffset).adjusted(-e, -e, e, e));
    }

    // (a hair of tolerance: a polygon's bounds that should be exactly the picture's come out a few ulps over)
    constexpr double kEps = 1e-4;
    const double left = std::floor(canvas.left() + kEps), top = std::floor(canvas.top() + kEps);
    double cw = std::ceil(canvas.right() - kEps) - left, ch = std::ceil(canvas.bottom() - kEps) - top;
    L.origin = QPointF(left, top);
    if (v[kKeepSize] >= 0.5) {
        // The whole composition is drawn at the picture's own size, with the picture shrunk to make room;
        // the canvas is widened (evenly, about its middle) to the picture's proportions first.
        L.scale = std::min(w / cw, h / ch);
        const double nw = w / L.scale, nh = h / L.scale;
        L.origin = QPointF(left - (nw - cw) * 0.5, top - (nh - ch) * 0.5);
        L.size = QSize(w, h);
    } else {
        L.scale = 1.0;
        if (cw * ch > kMaxCanvasPixels || std::max(cw, ch) > kMaxCanvasSide)
            L.scale = std::min(std::sqrt(kMaxCanvasPixels / (cw * ch)), kMaxCanvasSide / std::max(cw, ch));
        L.size = QSize(std::max(1, int(std::lround(cw * L.scale))), std::max(1, int(std::lround(ch * L.scale))));
    }
    return L;
}

int styleOf(const EffectValues &v) { return clampi(int(std::lround(v[kStyle])), kRound, kConcave); }

// Copies `src` (straight RGBA) into `out` (premultiplied ARGB) with its top-left at (ox, oy).
void blitPremultiplied(const Job &job, QImage &out, const QImage &src, int ox, int oy)
{
    rows(job, src.height(), [&](int y) {
        const int dy = y + oy;
        if (dy < 0 || dy >= out.height())
            return;
        const uint8_t *s = src.constScanLine(y);
        uint32_t *d = reinterpret_cast<uint32_t *>(out.scanLine(dy));
        for (int x = 0; x < src.width(); ++x) {
            const int dx = x + ox;
            if (dx < 0 || dx >= out.width())
                continue;
            const uint32_t a = s[x * 4 + 3];
            const uint32_t r = (s[x * 4] * a + 127) / 255, g = (s[x * 4 + 1] * a + 127) / 255, b = (s[x * 4 + 2] * a + 127) / 255;
            d[dx] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    });
}

// A blurred, tinted copy of an alpha mask as an image the size of `size`/`k`, ready to be scaled up.
QImage blurredShadowLayer(const Job &job, const QPainterPath &shape, const QTransform &toDevice, QSize size, double k,
                          double sigma, const QColor &color)
{
    const int lw = std::max(8, int(std::ceil(size.width() * k))), lh = std::max(8, int(std::ceil(size.height() * k)));
    QImage mask(lw, lh, QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);
    {
        QPainter p(&mask);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.scale(double(lw) / size.width(), double(lh) / size.height());
        p.fillPath(toDevice.map(shape), Qt::white);
    }
    Plane alpha(lw, lh, 1);
    for (int y = 0; y < lh; ++y) {
        const uint32_t *s = reinterpret_cast<const uint32_t *>(mask.constScanLine(y));
        uint8_t *d = alpha.row(y);
        for (int x = 0; x < lw; ++x)
            d[x] = uint8_t(s[x] >> 24);
    }
    const Plane blurred = gaussianBlur(job, alpha, sigma);
    QImage layer(lw, lh, QImage::Format_ARGB32_Premultiplied);
    const double cr = color.red(), cg = color.green(), cb = color.blue(), ca = color.alpha() / 255.0;
    for (int y = 0; y < lh; ++y) {
        uint32_t *d = reinterpret_cast<uint32_t *>(layer.scanLine(y));
        const uint8_t *s = blurred.row(y);
        for (int x = 0; x < lw; ++x) {
            const double a = s[x] / 255.0 * ca;
            d[x] = (uint32_t(a * 255.0 + 0.5) << 24) | (uint32_t(cr * a + 0.5) << 16) | (uint32_t(cg * a + 0.5) << 8)
                   | uint32_t(cb * a + 0.5);
        }
    }
    return layer;
}

QImage fxFrame(const Job &job, const QImage &src, const EffectValues &v)
{
    const int w = src.width(), h = src.height();
    const int style = styleOf(v);
    const Layout L = computeLayout(w, h, v, style);

    QImage out(L.size, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);

    // natural coordinates -> canvas pixels
    QTransform toDevice;
    toDevice.scale(L.scale, L.scale);
    toDevice.translate(-L.origin.x(), -L.origin.y());

    // 1. the picture, cut to the shape
    const bool unscaled = std::abs(L.scale - 1.0) < 1e-9 && L.origin.x() == std::floor(L.origin.x())
                          && L.origin.y() == std::floor(L.origin.y());
    if (unscaled) {
        blitPremultiplied(job, out, src, int(-L.origin.x()), int(-L.origin.y()));
    } else {
        const QSize scaled(std::max(1, int(std::lround(w * L.scale))), std::max(1, int(std::lround(h * L.scale))));
        const QImage small = resizeHighQuality(src, scaled, false, false);
        QPainter p(&out);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(QRectF(toDevice.map(QPointF(0, 0)), QSizeF(w * L.scale, h * L.scale)), small);
    }
    if (job.cancelled())
        return out;

    const QPainterPath shape = buildPath(L.ring, L.d, style);
    {
        QPainter p(&out);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
        QPainterPath outside;
        outside.addRect(QRectF(-4, -4, w + 8.0, h + 8.0));
        outside.addPath(shape);
        outside.setFillRule(Qt::OddEvenFill);
        p.fillPath(toDevice.map(outside), Qt::black);
    }

    // 2. an outline inside the shape: over the picture, only where the picture is
    if (L.stroke > 0.01 && L.strokeInside) {
        Ring inner;
        std::vector<double> innerD;
        offsetRing(L.ring, L.d, style, -L.stroke, inner, innerD);
        QPainterPath around;
        around.addRect(QRectF(-4, -4, w + 8.0, h + 8.0));
        around.addPath(buildPath(inner, innerD, style));
        around.setFillRule(Qt::OddEvenFill);
        QPainter p(&out);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setCompositionMode(QPainter::CompositionMode_SourceAtop);
        p.fillPath(toDevice.map(around), colorOf(v[kStrokeColor], v[kStrokeOpacity]));
    }

    // 3a. an outline outside the shape, behind the picture
    if (L.stroke > 0.01 && !L.strokeInside) {
        // the ring between the outer outline and a hair inside the shape, so that the picture's soft edge
        // sits on the outline instead of on whatever is behind it
        Ring inner;
        std::vector<double> innerD;
        offsetRing(L.ring, L.d, style, -1.0 / std::max(L.scale, 1e-3), inner, innerD);
        QPainterPath band;
        band.addPath(buildPath(L.outerRing, L.outerD, style));
        band.addPath(buildPath(inner, innerD, style));
        band.setFillRule(Qt::OddEvenFill);
        QPainter p(&out);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setCompositionMode(QPainter::CompositionMode_DestinationOver);
        p.fillPath(toDevice.map(band), colorOf(v[kStrokeColor], v[kStrokeOpacity]));
    }
    if (job.cancelled())
        return out;

    // 3b. the shadow of the shape together with its outline
    if (L.hasShadow && v[kShadow] > 0.0) {
        const QColor shadowColor = colorOf(v[kShadowColor], v[kShadow]);
        QPainterPath outer = buildPath(L.outerRing, L.outerD, style);
        outer.translate(L.shadowOffset);
        const double sigma = L.shadowBlur * L.scale * 0.5; // device pixels
        QPainter p(&out);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.setCompositionMode(QPainter::CompositionMode_DestinationOver);
        if (sigma < 1.0) {
            p.fillPath(toDevice.map(outer), shadowColor);
        } else {
            // drawn and blurred small (a blur this wide does not need the pixels), then scaled up
            const double k = clampd(4.0 / sigma, 0.04, 1.0);
            const QImage layer = blurredShadowLayer(job, outer, toDevice, L.size, k, sigma * k, shadowColor);
            p.drawImage(QRectF(0, 0, L.size.width(), L.size.height()), layer);
        }
    }
    if (job.cancelled())
        return out;

    // 3c. the background
    const int bg = clampi(int(std::lround(v[kBg])), kBgColorMode, kBgPhoto);
    {
        QPainter p(&out);
        p.setCompositionMode(QPainter::CompositionMode_DestinationOver);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const QRectF all(0, 0, L.size.width(), L.size.height());
        if (bg == kBgColorMode) {
            p.fillRect(all, colorOf(v[kBgColor], v[kBgOpacity]));
        } else if (bg == kBgGradient) {
            const double a = v[kBgAngle] * kPi / 180.0;
            const QPointF dir(std::cos(a), std::sin(a));
            const double half = 0.5 * (std::abs(dir.x()) * all.width() + std::abs(dir.y()) * all.height());
            const QPointF mid = all.center();
            QLinearGradient g(mid - dir * half, mid + dir * half);
            g.setColorAt(0.0, colorOf(v[kBgColor], v[kBgOpacity]));
            g.setColorAt(1.0, colorOf(v[kBgColor2], v[kBgOpacity]));
            p.fillRect(all, g);
        } else if (bg == kBgPhoto) {
            // the picture itself, enlarged to cover the canvas and blurred
            const double k = std::min(1.0, 640.0 / std::max(all.width(), all.height()));
            const int lw = std::max(8, int(std::ceil(all.width() * k))), lh = std::max(8, int(std::ceil(all.height() * k)));
            const double cover = std::max(double(lw) / w, double(lh) / h);
            const QSize big(std::max(lw, int(std::ceil(w * cover))), std::max(lh, int(std::ceil(h * cover))));
            const QImage small = src.scaled(big, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            Plane plane(lw, lh, 4);
            const int cx = (big.width() - lw) / 2, cy = (big.height() - lh) / 2;
            for (int y = 0; y < lh; ++y) {
                const uint8_t *s = small.constScanLine(y + cy) + size_t(cx) * 4;
                std::copy_n(s, size_t(lw) * 4, plane.row(y));
            }
            // blur premultiplied colour, so a transparent pixel's hidden colour does not bleed
            for (int y = 0; y < lh; ++y) {
                uint8_t *d = plane.row(y);
                for (int x = 0; x < lw; ++x) {
                    const int a = d[x * 4 + 3];
                    d[x * 4] = uint8_t((d[x * 4] * a + 127) / 255);
                    d[x * 4 + 1] = uint8_t((d[x * 4 + 1] * a + 127) / 255);
                    d[x * 4 + 2] = uint8_t((d[x * 4 + 2] * a + 127) / 255);
                }
            }
            const double sigma = v[kBgBlur] / 100.0 * 0.25 * L.s * L.scale * k;
            const Plane blurred = gaussianBlur(job, plane, std::max(0.5, sigma));
            QImage layer(lw, lh, QImage::Format_ARGB32_Premultiplied);
            for (int y = 0; y < lh; ++y) {
                const uint8_t *s = blurred.row(y);
                uint32_t *d = reinterpret_cast<uint32_t *>(layer.scanLine(y));
                for (int x = 0; x < lw; ++x)
                    d[x] = (uint32_t(s[x * 4 + 3]) << 24) | (uint32_t(s[x * 4]) << 16) | (uint32_t(s[x * 4 + 1]) << 8)
                           | uint32_t(s[x * 4 + 2]);
            }
            p.drawImage(all, layer);
        }
    }
    return out;
}

} // namespace

EffectSpec frameSpec()
{
    const std::initializer_list<int> kCornered = {kRect, kHexagon, kOctagon, kDiamond, kTriangle, kStar};
    std::vector<EffectParam> params(kParamCount);
    params[kShape] = choice("Forma", {"Rectángulo", "Elipse", "Hexágono", "Octágono", "Rombo", "Triángulo", "Estrella", "Corazón"}, kRect);
    params[kStyle] = shownWhen(choice("Esquinas", {"Redondas", "Suaves", "Cortadas", "Cóncavas"}, kRound), kShape, kCornered);
    params[kRadius] = shownWhen(withHint(slider("Redondez", 0, 100, 14),
                                         "100 % llega hasta donde dejan los lados vecinos (un cuadrado pasa a círculo)"),
                                kShape, kCornered);
    params[kCorners] = shownWhen(slider("Esquinas activas", 0, 15, 15, "", true), kShape, {kRect});
    params[kStroke] = withHint(slider("Contorno", 0, 40, 3), "Ancho del contorno, en % del lado corto");
    params[kStrokeColor] = colorParam("Color del contorno", 255, 255, 255);
    params[kStrokeOpacity] = slider("Opacidad del contorno", 0, 100, 100);
    params[kStrokeInside] = withHint(toggle("Contorno hacia adentro", false), "Lo dibuja sobre la foto en vez de agrandar la imagen");
    params[kMargin] = withHint(slider("Margen", 0, 50, 0), "Espacio de fondo alrededor, en % del lado corto");
    params[kKeepSize] = withHint(toggle("Mantener el tamaño", false), "Achica la foto para que todo entre en el tamaño de origen");
    params[kShadow] = slider("Sombra", 0, 100, 35);
    params[kShadowColor] = colorParam("Color de la sombra", 0, 0, 0);
    params[kShadowDist] = slider("Distancia de la sombra", 0, 20, 3);
    params[kShadowBlur] = slider("Desenfoque de la sombra", 0, 30, 4);
    params[kShadowAngle] = slider("Ángulo de la sombra", -180, 180, 45, "°", true);
    params[kBg] = choice("Fondo", {"Color", "Degradado", "Transparente", "Foto desenfocada"}, kBgTransparent);
    params[kBgColor] = shownWhen(colorParam("Color de fondo", 255, 255, 255), kBg, {kBgColorMode, kBgGradient});
    params[kBgColor2] = shownWhen(colorParam("Segundo color", 120, 120, 130), kBg, {kBgGradient});
    params[kBgAngle] = shownWhen(slider("Ángulo del degradado", -180, 180, 90, "°", true), kBg, {kBgGradient});
    params[kBgOpacity] = shownWhen(slider("Opacidad del fondo", 0, 100, 100), kBg, {kBgColorMode, kBgGradient});
    params[kBgBlur] = shownWhen(slider("Desenfoque del fondo", 0, 30, 6), kBg, {kBgPhoto});

    EffectSpec s = makeSpec("frame", "Marco", "distort", {});
    s.params = std::move(params);
    s.changesSize = true;
    s.hidden = true; // driven by the Marco page of the Recortar tool
    s.presets = {
        {"Esquinas redondas", {{kShape, kRect}, {kStyle, kRound}, {kRadius, 14}, {kCorners, 15}, {kStroke, 0}, {kShadow, 0}, {kMargin, 0}}},
        {"Pegatina", {{kShape, kRect}, {kStyle, kRound}, {kRadius, 16}, {kStroke, 4}, {kStrokeColor, packColor(255, 255, 255)},
                      {kShadow, 45}, {kShadowDist, 2.5}, {kShadowBlur, 5}, {kMargin, 0}}},
        {"Marco blanco", {{kShape, kRect}, {kRadius, 0}, {kStroke, 5}, {kStrokeColor, packColor(255, 255, 255)}, {kShadow, 30},
                          {kShadowDist, 2}, {kShadowBlur, 4}, {kBg, kBgColorMode}, {kBgColor, packColor(235, 235, 235)}, {kMargin, 6}}},
        {"Círculo", {{kShape, kEllipse}, {kStroke, 2}, {kStrokeColor, packColor(255, 255, 255)}, {kShadow, 40}, {kMargin, 0}}},
        {"Insignia", {{kShape, kHexagon}, {kStyle, kRound}, {kRadius, 25}, {kStroke, 4}, {kStrokeColor, packColor(255, 255, 255)}, {kShadow, 40}}},
        {"Entrada", {{kShape, kRect}, {kStyle, kConcave}, {kRadius, 12}, {kStroke, 0}, {kShadow, 35}, {kMargin, 4}}},
        {"Con la foto de fondo", {{kShape, kRect}, {kRadius, 10}, {kStroke, 0}, {kShadow, 50}, {kMargin, 12}, {kBg, kBgPhoto}}},
    };
    return s;
}

QImage fxFrameRender(const Job &job, const QImage &src, const EffectValues &v)
{
    QImage out = fxFrame(job, src, v);
    if (job.cancelled())
        return out;
    return out.convertToFormat(QImage::Format_RGBA8888);
}

QSize frameOutputSize(const EffectValues &v, QSize input)
{
    return computeLayout(input.width(), input.height(), v, styleOf(v)).size;
}

} // namespace core::edit::fxk
