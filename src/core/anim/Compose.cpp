#include "Anim.h"
#include "edit/Resample.h"

#include <QPainter>
#include <algorithm>
#include <cmath>

namespace core::anim {

namespace {

double smooth(double t)
{
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

QColor canvasColor(const Settings &s)
{
    return s.transparentBackground ? QColor(0, 0, 0, 0) : s.background;
}

QImage blank(const Settings &s)
{
    QImage out(s.size, QImage::Format_ARGB32_Premultiplied);
    out.fill(canvasColor(s));
    return out;
}

QImage toStraight(const QImage &img) { return img.convertToFormat(QImage::Format_RGBA8888); }

// The two pictures mixed: straight alpha in, straight alpha out, mixed as premultiplied colour so that
// a transparent pixel's hidden colour does not bleed into the other one.
QImage crossFade(const QImage &a, const QImage &b, double t)
{
    const QImage A = a.convertToFormat(QImage::Format_RGBA8888), B = b.convertToFormat(QImage::Format_RGBA8888);
    QImage out(A.size(), QImage::Format_RGBA8888);
    const float w = float(t);
    for (int y = 0; y < out.height(); ++y) {
        const uchar *pa = A.constScanLine(y);
        const uchar *pb = B.constScanLine(y);
        uchar *po = out.scanLine(y);
        for (int x = 0; x < out.width(); ++x, pa += 4, pb += 4, po += 4) {
            const float a1 = pa[3] / 255.f, a2 = pb[3] / 255.f;
            const float ao = a1 + (a2 - a1) * w;
            if (ao <= 0.0005f) {
                po[0] = po[1] = po[2] = po[3] = 0;
                continue;
            }
            for (int c = 0; c < 3; ++c) {
                const float p1 = pa[c] * a1, p2 = pb[c] * a2;
                const float v = (p1 + (p2 - p1) * w) / ao;
                po[c] = uchar(v < 0.f ? 0.f : (v > 255.f ? 255.f : v + 0.5f));
            }
            po[3] = uchar(ao * 255.f + 0.5f);
        }
    }
    return out;
}

} // namespace

QImage fitToCanvas(const QImage &source, const Settings &settings)
{
    QImage out = blank(settings);
    if (source.isNull() || source.width() < 1 || source.height() < 1)
        return toStraight(out);
    const QSize S = settings.size;
    const double sw = source.width(), sh = source.height();
    QPainter p(&out);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    if (settings.fit == Fit::Stretch) {
        p.drawImage(QPoint(0, 0), edit::resizeHighQuality(source, S, false, false));
    } else {
        const double scale = settings.fit == Fit::Contain ? std::min(S.width() / sw, S.height() / sh)
                                                          : std::max(S.width() / sw, S.height() / sh);
        const QSize target(std::max(1, int(std::lround(sw * scale))), std::max(1, int(std::lround(sh * scale))));
        const QImage scaled = target == source.size() ? toStraight(source) : edit::resizeHighQuality(source, target, false, false);
        // centred; for Cover the part outside the canvas is simply not drawn
        p.drawImage(QPoint((S.width() - target.width()) / 2, (S.height() - target.height()) / 2), scaled);
    }
    p.end();
    return toStraight(out);
}

QImage renderStep(const QImage &fittedA, const QImage &fittedB, const PlanStep &step, const Settings &settings)
{
    if (step.isStill() || settings.transition == Transition::None || fittedB.isNull())
        return fittedA;
    const double t = smooth(step.t);
    switch (settings.transition) {
    case Transition::Fade:
        return crossFade(fittedA, fittedB, t);
    case Transition::SlideLeft:
    case Transition::SlideRight:
    case Transition::SlideUp:
    case Transition::SlideDown: {
        QImage out = blank(settings);
        const int W = settings.size.width(), H = settings.size.height();
        int ax = 0, ay = 0, bx = 0, by = 0;
        const int dx = int(std::lround(t * W)), dy = int(std::lround(t * H));
        switch (settings.transition) {
        case Transition::SlideLeft: ax = -dx; bx = W - dx; break;
        case Transition::SlideRight: ax = dx; bx = dx - W; break;
        case Transition::SlideUp: ay = -dy; by = H - dy; break;
        default: ay = dy; by = dy - H; break;
        }
        QPainter p(&out);
        p.drawImage(QPoint(ax, ay), fittedA);
        p.drawImage(QPoint(bx, by), fittedB);
        p.end();
        return toStraight(out);
    }
    case Transition::Zoom: {
        QImage out = blank(settings);
        const QRectF all(QPointF(0, 0), QSizeF(settings.size));
        auto scaledRect = [&](double s) {
            const QSizeF sz(all.width() * s, all.height() * s);
            return QRectF(all.center() - QPointF(sz.width() / 2, sz.height() / 2), sz);
        };
        QPainter p(&out);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(scaledRect(0.82 + 0.18 * t), fittedB);
        p.setOpacity(1.0 - t);
        p.drawImage(scaledRect(1.0 + 0.35 * t), fittedA);
        p.end();
        return toStraight(out);
    }
    default:
        return fittedA;
    }
}

std::vector<PlanStep> buildPlan(const std::vector<int> &holdMs, const Settings &settings)
{
    std::vector<PlanStep> plan;
    const int n = int(holdMs.size());
    if (n == 0)
        return plan;
    const double speed = std::clamp(settings.speed, 0.1, 10.0);

    std::vector<int> order(n);
    for (int i = 0; i < n; ++i)
        order[size_t(i)] = settings.reverse ? n - 1 - i : i;
    if (settings.pingPong && n > 2)
        for (int i = n - 2; i >= 1; --i)
            order.push_back(order[size_t(i)]);

    const bool transitions = settings.transition != Transition::None && n >= 2;
    int steps = transitions ? std::clamp(settings.transitionSteps, 1, 60) : 0;
    if (steps > 0)
        steps = std::max(1, std::min(steps, int(settings.transitionMs / speed / 20.0))); // no frame shorter than 20 ms
    const int stepDelay = steps > 0 ? std::max(20, int(std::lround(settings.transitionMs / speed / steps))) : 0;

    for (size_t k = 0; k < order.size(); ++k) {
        const int a = order[k];
        plan.push_back({a, a, 0.0, std::max(20, int(std::lround(holdMs[size_t(a)] / speed)))});
        if (steps == 0)
            continue;
        int b = -1;
        if (k + 1 < order.size())
            b = order[k + 1];
        else if (settings.transitionOnLoop && order.size() >= 2)
            b = order[0];
        if (b < 0 || b == a)
            continue;
        for (int s = 1; s <= steps; ++s)
            plan.push_back({a, b, double(s) / (steps + 1), stepDelay});
    }
    return plan;
}

} // namespace core::anim
