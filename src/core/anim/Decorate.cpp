#include "Anim.h"
#include "edit/Effects.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QStringList>
#include <algorithm>
#include <cmath>

namespace core::anim {

QString FrameStyle::signature() const
{
    if (isPlain())
        return {};
    QString s;
    if (!effectId.isEmpty() && effectMix > 0.0)
        s += QStringLiteral("fx:%1/%2/%3;").arg(effectId).arg(effectPreset).arg(effectMix, 0, 'f', 3);
    if (!text.text.trimmed().isEmpty())
        s += QStringLiteral("tx:%1|%2|%3,%4|%5|%6|%7|%8|%9;").arg(text.text, text.family).arg(text.x, 0, 'f', 4).arg(text.y, 0, 'f', 4)
                 .arg(text.size, 0, 'f', 3).arg(text.bold).arg(text.color.rgba(), 0, 16).arg(text.outline).arg(text.outlineColor.rgba(), 0, 16);
    return s;
}

bool effectUsableOnFrames(const QString &effectId)
{
    const edit::EffectSpec *spec = edit::findEffect(effectId);
    return spec && !spec->hidden && !spec->changesSize && !spec->fullSize;
}

namespace {

QImage withEffect(const QImage &img, const FrameStyle &style)
{
    if (style.effectId.isEmpty() || style.effectMix <= 0.0 || !effectUsableOnFrames(style.effectId))
        return img;
    const edit::EffectSpec *spec = edit::findEffect(style.effectId);
    edit::EffectValues values = edit::defaultEffectValues(*spec);
    if (style.effectPreset >= 0 && size_t(style.effectPreset) < spec->presets.size())
        values = edit::applyEffectPreset(*spec, spec->presets[size_t(style.effectPreset)], values);
    const QImage out = edit::applyEffect(img, style.effectId, values, std::clamp(style.effectMix, 0.0, 1.0));
    return out.size() == img.size() ? out : img;
}

QImage withText(const QImage &img, const TextOverlay &t)
{
    const QStringList lines = t.text.split(QLatin1Char('\n'));
    bool any = false;
    for (const QString &l : lines)
        any = any || !l.trimmed().isEmpty();
    if (!any || img.isNull())
        return img;

    const double W = img.width(), H = img.height();
    QFont font = t.family.isEmpty() ? QFont() : QFont(t.family);
    font.setBold(t.bold);
    const double shortSide = std::min(W, H);
    double px = std::max(6.0, t.size / 100.0 * shortSide);
    font.setPixelSize(int(std::lround(px)));

    // one path for the whole block, each line centred on the middle; shrunk when it is wider than the picture
    auto build = [&](double pixel) {
        QFont f = font;
        f.setPixelSize(std::max(4, int(std::lround(pixel))));
        const QFontMetricsF fm(f);
        const double lineH = fm.height() * 1.05;
        QPainterPath path;
        double y = -lineH * lines.size() / 2.0 + fm.ascent();
        for (const QString &l : lines) {
            QPainterPath line;
            line.addText(0, 0, f, l);
            const QRectF b = line.boundingRect();
            line.translate(-(b.left() + b.width() / 2.0), y);
            path.addPath(line);
            y += lineH;
        }
        return path;
    };
    QPainterPath path = build(px);
    const double room = W * 0.96;
    if (path.boundingRect().width() > room) {
        px *= room / path.boundingRect().width();
        path = build(px);
    }
    const QRectF box = path.boundingRect();
    // keep the whole block inside the picture whatever x, y say
    double cx = std::clamp(t.x, 0.0, 1.0) * W, cy = std::clamp(t.y, 0.0, 1.0) * H;
    cx = std::clamp(cx, box.width() / 2.0, std::max(box.width() / 2.0, W - box.width() / 2.0));
    cy = std::clamp(cy, box.height() / 2.0, std::max(box.height() / 2.0, H - box.height() / 2.0));
    path.translate(cx, cy - (box.top() + box.height() / 2.0));

    QImage out = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (t.outline) {
        QPen pen(t.outlineColor, std::max(1.0, px * 0.14));
        pen.setJoinStyle(Qt::RoundJoin);
        p.strokePath(path, pen);
    }
    p.fillPath(path, t.color);
    p.end();
    return out.convertToFormat(QImage::Format_RGBA8888);
}

} // namespace

QImage decorate(const QImage &fitted, const FrameStyle &style)
{
    if (style.isPlain() || fitted.isNull())
        return fitted;
    return withText(withEffect(fitted, style), style.text);
}

} // namespace core::anim
