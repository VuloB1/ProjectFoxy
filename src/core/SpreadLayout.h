#pragma once

#include <QSize>
#include <QVector>

namespace core {

// What is on the screen at one time in a book: one page, or two side by side.
struct Spread {
    int first = 0; // index of the first page
    int count = 1; // 1 or 2
    bool operator==(const Spread &) const = default;
};

// A landscape picture is a double-page spread already (or a page that wants the whole screen): it is shown alone.
inline bool isWidePage(const QSize &size) { return size.isValid() && size.width() > size.height(); }

// The spreads of a book, in order. `sizes` has one entry per page (invalid when not known yet, which counts as an
// ordinary portrait page). Single page: one page per spread. Double page: pages go in pairs; with `coverAlone` the
// first page (the cover) stands by itself, so the pairs start at the second; a wide page always stands alone and the
// pairing starts again after it.
inline QVector<Spread> buildSpreads(const QVector<QSize> &sizes, bool doublePage, bool coverAlone)
{
    QVector<Spread> out;
    const int n = int(sizes.size());
    for (int i = 0; i < n;) {
        const bool pair = doublePage && !(coverAlone && i == 0) && !isWidePage(sizes[i]) && i + 1 < n
                          && !isWidePage(sizes[i + 1]);
        out.append({i, pair ? 2 : 1});
        i += pair ? 2 : 1;
    }
    return out;
}

// The index in `spreads` that shows page `page`.
inline int spreadOfPage(const QVector<Spread> &spreads, int page)
{
    for (int s = 0; s < spreads.size(); ++s)
        if (page < spreads[s].first + spreads[s].count)
            return s;
    return qMax(0, int(spreads.size()) - 1);
}

} // namespace core
