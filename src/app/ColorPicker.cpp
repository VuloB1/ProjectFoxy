#include "ColorPicker.h"

#include <QClipboard>
#include <QCursor>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace {

// A pipette drawn pointing down-left, drawn on a 32 x 32 grid and enlarged (kScale) so that it is easy to see on
// a big or high-resolution screen; the tip is the hot spot.
constexpr double kScale = 1.9;
QCursor eyedropperCursor()
{
    QPixmap pm(int(32 * kScale), int(32 * kScale));
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(kScale, kScale);
    auto body = [&](const QColor &fill, qreal grow) {
        p.save();
        p.translate(3, 28);
        p.rotate(-45);                         // the pipette lies along the up-right diagonal
        QPainterPath path;
        path.moveTo(0, 0);                     // tip
        path.lineTo(-1.6 - grow, -4 - grow);
        path.lineTo(-2.4 - grow, -14 - grow);
        path.lineTo(2.4 + grow, -14 - grow);
        path.lineTo(1.6 + grow, -4 - grow);
        path.closeSubpath();
        path.addRoundedRect(QRectF(-4.2 - grow, -18 - grow, 8.4 + 2 * grow, 5 + 2 * grow), 1.5, 1.5);  // the bulb
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawPath(path);
        p.restore();
    };
    body(QColor(0, 0, 0, 230), 1.1);           // a dark outline, so it shows on any picture
    body(QColor(245, 245, 245), 0.0);
    p.end();
    return QCursor(pm, int(3 * kScale), int(28 * kScale));
}

} // namespace

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
        QGuiApplication::setOverrideCursor(eyedropperCursor());
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
