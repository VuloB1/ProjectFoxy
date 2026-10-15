#pragma once

#include <QList>
#include <QRect>
#include <QSize>

#include <algorithm>

// Where the window opens, from what was saved the last time. `screens` are the available geometries of the screens
// that are connected now (in Qt's units), the first one being the primary. The window never comes out larger than the
// screen it lands on, nor hanging off it: a size saved on a bigger screen (or a screen that has gone) is brought down
// to what fits. With nothing saved (a null rect) it returns a null rect and the window keeps its default.
inline QRect fitWindowToScreens(const QRect &saved, const QList<QRect> &screens, const QSize &minimum)
{
    if (saved.isNull() || screens.isEmpty())
        return {};
    QRect screen = screens.first();
    for (const QRect &candidate : screens)
        if (candidate.contains(saved.center())) {
            screen = candidate;
            break;
        }
    const int w = std::clamp(saved.width(), std::min(minimum.width(), screen.width()), screen.width());
    const int h = std::clamp(saved.height(), std::min(minimum.height(), screen.height()), screen.height());
    // Saved on a screen that is no longer there: centred on the primary one.
    const bool known = screen.contains(saved.center());
    const int x = known ? std::clamp(saved.x(), screen.left(), screen.right() + 1 - w) : screen.x() + (screen.width() - w) / 2;
    const int y = known ? std::clamp(saved.y(), screen.top(), screen.bottom() + 1 - h) : screen.y() + (screen.height() - h) / 2;
    return {x, y, w, h};
}
