#include "ColorPicker.h"

#include <QClipboard>
#include <QCursor>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace {

// The pipette is drawn on a 32 x 32 grid and enlarged so that it is easy to see on a big or high-resolution screen.
constexpr double kScale = 1.1;

// The shapes of the pipette in its own coordinates: the tip is the origin and the pipette lies along -y (the
// caller turns it). A hollow glass tube that narrows to the tip, a collar, and a solid bulb on top.
QPainterPath tubePath()
{
    QPainterPath p;
    p.moveTo(0, 0);
    p.lineTo(-1.5, -6.5);
    p.lineTo(-2.5, -17.0);
    p.lineTo(2.5, -17.0);
    p.lineTo(1.5, -6.5);
    p.closeSubpath();
    return p;
}
QPainterPath collarPath()
{
    QPainterPath p;
    p.addRoundedRect(QRectF(-4.2, -19.6, 8.4, 3.0), 1.2, 1.2);
    return p;
}
QPainterPath bulbPath()
{
    QPainterPath p;
    p.addRoundedRect(QRectF(-3.0, -28.6, 6.0, 9.4), 3.0, 3.0);
    return p;
}

} // namespace

QPixmap ColorPicker::eyedropperPixmap()
{
    QPixmap pm(int(32 * kScale), int(32 * kScale));
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(kScale, kScale);
    p.translate(3.0, 29.0);                // the tip
    p.rotate(45.0);                        // turns the pipette to point up and to the right
    const QColor dark(0, 0, 0, 235), light(250, 250, 250);

    // a dark halo first, so that it can be seen over any picture
    QPen halo(dark, 3.4);
    halo.setJoinStyle(Qt::RoundJoin);
    p.setPen(halo);
    p.setBrush(dark);
    p.drawPath(tubePath());
    p.drawPath(collarPath());
    p.drawPath(bulbPath());

    // then the pipette itself: a hollow tube, the collar and the bulb solid
    QPen line(light, 1.5);
    line.setJoinStyle(Qt::RoundJoin);
    p.setPen(line);
    p.setBrush(Qt::NoBrush);
    p.drawPath(tubePath());
    p.setPen(Qt::NoPen);
    p.setBrush(light);
    p.drawPath(collarPath());
    p.drawPath(bulbPath());
    p.end();
    return pm;
}

ColorPicker::~ColorPicker()
{
    setCursor(false);
}

bool ColorPicker::altDown() const
{
    return QGuiApplication::queryKeyboardModifiers().testFlag(Qt::AltModifier);
}

void ColorPicker::setCursor(bool eyedropper)
{
    if (eyedropper == m_cursorOn)
        return;
    m_cursorOn = eyedropper;
    if (eyedropper)
        QGuiApplication::setOverrideCursor(QCursor(eyedropperPixmap(), int(3 * kScale), int(29 * kScale)));
    else
        QGuiApplication::restoreOverrideCursor();
}

QString ColorPicker::hex(int r, int g, int b, int a) const
{
    auto h = [](int v) { return QStringLiteral("%1").arg(qBound(0, v, 255), 2, 16, QLatin1Char('0')).toUpper(); };
    return QStringLiteral("#") + h(r) + h(g) + h(b) + (a < 255 ? h(a) : QString());
}

void ColorPicker::copyText(const QString &text) const
{
    if (QClipboard *clipboard = QGuiApplication::clipboard())
        clipboard->setText(text);
}
