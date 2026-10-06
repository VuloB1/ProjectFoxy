// "Leyenda" (the meme caption): a band of colour above or below the picture with a text in it - the classic
// white strip with black text - or the text over the picture itself with an outline (the Impact look).
//
// It is an effect that needs a text, which the other effects do not: the text travels beside the sliders (see
// applyEffect's `text`). Every size is a percentage of the picture's WIDTH, so the same settings caption a
// 1600 px preview and the full-size photo the same way, and the band's height follows from the text (long
// text wraps to the width), which is why the effect changes the size of the picture.

#include "EffectsCommon.h"

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPointF>
#include <QTextLayout>
#include <QTextLine>
#include <QTextOption>

namespace core::edit::fxk {

namespace {

enum Param { kPosition, kSize, kPad, kAlign, kFont, kBold, kTextColor, kBandColor, kOutline, kOutlineColor, kOutlineWidth, kCount };
enum Position { kBandTop, kBandBottom, kOverTop, kOverBottom };

const QStringList &fontFamilies()
{
    static const QStringList families = {QStringLiteral("Arial"), QStringLiteral("Impact"), QStringLiteral("Segoe UI"),
                                         QStringLiteral("Georgia"), QStringLiteral("Times New Roman"),
                                         QStringLiteral("Comic Sans MS"), QStringLiteral("Courier New")};
    return families;
}

QColor colorOf(double packed)
{
    const Rgb c = unpackColor(packed);
    return QColor(int(c.r + 0.5), int(c.g + 0.5), int(c.b + 0.5));
}

bool hasText(const QString &text)
{
    for (const QChar ch : text)
        if (!ch.isSpace())
            return true;
    return false;
}

// The text laid out for a picture `width` pixels wide.
struct Laid {
    QTextLayout layout;
    double pixel = 0;     // the font's pixel size
    double pad = 0;       // the margin around the text, in pixels
    double textH = 0;     // height of the whole text block
    int band = 0;         // how many pixels the picture grows (0 when the text goes over it)
};

void lay(Laid &out, const EffectValues &v, const QString &text, int width)
{
    QFont font(fontFamilies().value(clampi(int(v[kFont] + 0.5), 0, int(fontFamilies().size()) - 1)));
    font.setBold(v[kBold] >= 0.5);
    out.pixel = std::max(4.0, v[kSize] / 100.0 * width);
    font.setPixelSize(int(std::lround(out.pixel)));
    out.pad = v[kPad] / 100.0 * width;

    QString body = text;
    body.replace(QLatin1Char('\n'), QChar(QChar::LineSeparator));
    out.layout.setText(body);
    out.layout.setFont(font);
    QTextOption option;
    option.setWrapMode(QTextOption::WordWrap);
    const int align = clampi(int(v[kAlign] + 0.5), 0, 2);
    option.setAlignment(align == 0 ? Qt::AlignHCenter : (align == 1 ? Qt::AlignLeft : Qt::AlignRight));
    out.layout.setTextOption(option);

    const double room = std::max(8.0, width - 2.0 * out.pad);
    double y = 0.0;
    out.layout.beginLayout();
    for (;;) {
        QTextLine line = out.layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(room);
        line.setPosition(QPointF(0.0, y));
        y += line.height();
    }
    out.layout.endLayout();
    out.textH = y;
    const int mode = clampi(int(v[kPosition] + 0.5), 0, 3);
    out.band = mode <= kBandBottom ? int(std::ceil(out.textH + 2.0 * out.pad)) : 0;
}

} // namespace

QSize captionOutputSize(const EffectValues &v, const QString &text, QSize input)
{
    if (!hasText(text))
        return input;
    Laid laid;
    lay(laid, v, text, input.width());
    return {input.width(), input.height() + laid.band};
}

QImage fxCaption(const Job &job, const QImage &src, const EffectValues &v, const QString &text)
{
    if (!hasText(text) || src.isNull())
        return src;
    Laid laid;
    lay(laid, v, text, src.width());
    const int mode = clampi(int(v[kPosition] + 0.5), 0, 3);
    const int W = src.width(), H = src.height() + laid.band;
    if (job.cancelled())
        return src;

    QImage canvas(W, H, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);
    QPainter p(&canvas);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const int pictureTop = mode == kBandTop ? laid.band : 0;
    if (laid.band > 0) {
        const QRect band(0, mode == kBandTop ? 0 : src.height(), W, laid.band);
        p.fillRect(band, colorOf(v[kBandColor]));
    }
    p.drawImage(QPoint(0, pictureTop), src);

    // where the text block starts
    double top;
    switch (mode) {
    case kBandTop: top = laid.pad; break;
    case kBandBottom: top = src.height() + laid.pad; break;
    case kOverTop: top = laid.pad; break;
    default: top = H - laid.pad - laid.textH; break;
    }
    const QPointF origin(laid.pad, std::max(0.0, top));

    if (v[kOutline] >= 0.5) {
        const double r = std::max(1.0, v[kOutlineWidth] / 100.0 * laid.pixel);
        const QColor oc = colorOf(v[kOutlineColor]);
        const int steps = 24;
        p.setPen(oc);
        for (int i = 0; i < steps; ++i) {
            const double a = 2.0 * kPi * i / steps;
            laid.layout.draw(&p, origin + QPointF(std::cos(a) * r, std::sin(a) * r));
        }
    }
    p.setPen(colorOf(v[kTextColor]));
    laid.layout.draw(&p, origin);
    p.end();
    return canvas.convertToFormat(QImage::Format_RGBA8888);
}

EffectSpec captionSpec()
{
    EffectSpec s = makeSpec(
        "caption", "Leyenda (meme)", "finish",
        {choice("Posición", {"Franja arriba", "Franja abajo", "Sobre la imagen, arriba", "Sobre la imagen, abajo"}, 0),
         slider("Tamaño del texto", 1, 30, 7),
         slider("Margen", 0, 15, 3),
         choice("Alineación", {"Centrado", "Izquierda", "Derecha"}, 0),
         choice("Letra", {"Arial", "Impact", "Segoe UI", "Georgia", "Times New Roman", "Comic Sans MS", "Courier New"}, 0),
         toggle("Negrita", true),
         colorParam("Color del texto", 0, 0, 0),
         shownWhen(colorParam("Color de la franja", 255, 255, 255), kPosition, {0, 1}),
         toggle("Contorno", false),
         shownWhen(colorParam("Color del contorno", 0, 0, 0), kOutline, {1}),
         shownWhen(slider("Grosor del contorno", 2, 30, 8), kOutline, {1})});
    s.changesSize = true;
    s.usesText = true;
    const double white = packColor(255, 255, 255), black = packColor(0, 0, 0);
    s.presets = {
        {"Meme: franja blanca arriba", {{kPosition, 0}, {kSize, 7}, {kFont, 0}, {kBold, 1}, {kTextColor, black}, {kBandColor, white}, {kOutline, 0}}},
        {"Franja negra abajo", {{kPosition, 1}, {kSize, 6}, {kFont, 0}, {kBold, 1}, {kTextColor, white}, {kBandColor, black}, {kOutline, 0}}},
        {"Texto clásico sobre la imagen", {{kPosition, 2}, {kSize, 9}, {kFont, 1}, {kBold, 0}, {kTextColor, white}, {kOutline, 1}, {kOutlineColor, black}, {kOutlineWidth, 7}}},
        {"Subtítulo", {{kPosition, 3}, {kSize, 5}, {kFont, 0}, {kBold, 1}, {kTextColor, white}, {kOutline, 1}, {kOutlineColor, black}, {kOutlineWidth, 8}}}};
    return s;
}

} // namespace core::edit::fxk
