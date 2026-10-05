#include "Collage.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace core::collage {

namespace {

using Rects = std::vector<QRectF>;

// Rows one above the other, `counts[i]` equal cells in row i (every row as tall as the others).
Rects rowsLayout(const std::vector<int> &counts)
{
    Rects out;
    const double rowH = 1.0 / counts.size();
    for (size_t r = 0; r < counts.size(); ++r)
        for (int c = 0; c < counts[r]; ++c)
            out.push_back(QRectF(double(c) / counts[r], r * rowH, 1.0 / counts[r], rowH));
    return out;
}

// Columns side by side, `counts[i]` equal cells one above the other in column i.
Rects colsLayout(const std::vector<int> &counts)
{
    Rects out;
    const double colW = 1.0 / counts.size();
    for (size_t c = 0; c < counts.size(); ++c)
        for (int r = 0; r < counts[c]; ++r)
            out.push_back(QRectF(c * colW, double(r) / counts[c], colW, 1.0 / counts[c]));
    return out;
}

// One big cell on a side (`side`: 0 left, 1 top, 2 right, 3 bottom) taking `frac` of the canvas, the others in
// equal slices of what is left.
Rects heroLayout(int n, int side, double frac)
{
    if (n == 1)
        return {QRectF(0, 0, 1, 1)};
    Rects out;
    const int others = n - 1;
    const bool horizontalSplit = side == 0 || side == 2; // the hero is a column
    const double rest = 1.0 - frac;
    QRectF hero, area;
    switch (side) {
    case 0: hero = QRectF(0, 0, frac, 1); area = QRectF(frac, 0, rest, 1); break;
    case 1: hero = QRectF(0, 0, 1, frac); area = QRectF(0, frac, 1, rest); break;
    case 2: hero = QRectF(rest, 0, frac, 1); area = QRectF(0, 0, rest, 1); break;
    default: hero = QRectF(0, rest, 1, frac); area = QRectF(0, 0, 1, rest); break;
    }
    out.push_back(hero);
    for (int i = 0; i < others; ++i) {
        if (horizontalSplit)
            out.push_back(QRectF(area.x(), area.y() + area.height() * i / others, area.width(), area.height() / others));
        else
            out.push_back(QRectF(area.x() + area.width() * i / others, area.y(), area.width() / others, area.height()));
    }
    return out;
}

// n spread over `parts` rows (or columns) as evenly as possible, the longer ones first or last.
std::vector<int> balanced(int n, int parts, bool longFirst)
{
    std::vector<int> counts(size_t(parts), n / parts);
    for (int i = 0; i < n % parts; ++i)
        counts[size_t(longFirst ? i : parts - 1 - i)]++;
    return counts;
}

QString countsName(const char *what, const std::vector<int> &counts)
{
    QStringList parts;
    for (int c : counts)
        parts << QString::number(c);
    return QStringLiteral("%1 %2").arg(QString::fromUtf8(what), parts.join(QStringLiteral(" · ")));
}

// Cuts `r` into `n` cells, a random cut at a time.
void splitRandomly(const QRectF &r, int n, std::mt19937 &rng, Rects &out)
{
    if (n <= 1) {
        out.push_back(r);
        return;
    }
    auto unit = [&]() { return double(rng() % 10000) / 10000.0; };
    int k = std::clamp(n / 2 + int(rng() % 3) - 1, 1, n - 1);
    // mostly cut across the longer side
    const bool vertical = r.width() * (0.75 + 0.5 * unit()) >= r.height();
    const double ratio = std::clamp(double(k) / n + (unit() - 0.5) * 0.14, 0.28, 0.72);
    QRectF a, b;
    if (vertical) {
        a = QRectF(r.x(), r.y(), r.width() * ratio, r.height());
        b = QRectF(r.x() + a.width(), r.y(), r.width() - a.width(), r.height());
    } else {
        a = QRectF(r.x(), r.y(), r.width(), r.height() * ratio);
        b = QRectF(r.x(), r.y() + a.height(), r.width(), r.height() - a.height());
    }
    splitRandomly(a, k, rng, out);
    splitRandomly(b, n - k, rng, out);
}

bool sameRects(const Rects &a, const Rects &b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::abs(a[i].x() - b[i].x()) > 1e-6 || std::abs(a[i].y() - b[i].y()) > 1e-6 || std::abs(a[i].width() - b[i].width()) > 1e-6
            || std::abs(a[i].height() - b[i].height()) > 1e-6)
            return false;
    return true;
}

} // namespace

Layout mosaicLayout(int count, unsigned seed)
{
    count = std::clamp(count, 1, 12);
    std::mt19937 rng(seed * 2654435761u + 12345u);
    Layout l;
    l.name = QStringLiteral("Mosaico");
    splitRandomly(QRectF(0, 0, 1, 1), count, rng, l.rects);
    return l;
}

std::vector<Layout> layoutsFor(int count)
{
    count = std::clamp(count, 1, 12);
    std::vector<Layout> out;
    auto add = [&](const QString &name, const Rects &rects) {
        if (int(rects.size()) != count)
            return;
        for (const Layout &l : out)
            if (sameRects(l.rects, rects))
                return;
        out.push_back({name, rects});
    };

    if (count == 1) {
        add(QStringLiteral("Una foto"), {QRectF(0, 0, 1, 1)});
        return out;
    }
    if (count == 2) {
        add(QStringLiteral("Lado a lado"), colsLayout({1, 1}));
        add(QStringLiteral("Una sobre otra"), rowsLayout({1, 1}));
        add(QStringLiteral("Grande a la izquierda"), heroLayout(2, 0, 0.66));
        add(QStringLiteral("Grande a la derecha"), heroLayout(2, 2, 0.66));
        add(QStringLiteral("Grande arriba"), heroLayout(2, 1, 0.66));
        add(QStringLiteral("Grande abajo"), heroLayout(2, 3, 0.66));
        return out;
    }

    // evenly spread rows and columns
    for (int parts = 2; parts <= std::min(4, count); ++parts) {
        if (count / parts < 1)
            continue;
        add(countsName("Filas", balanced(count, parts, true)), rowsLayout(balanced(count, parts, true)));
        add(countsName("Filas", balanced(count, parts, false)), rowsLayout(balanced(count, parts, false)));
        add(countsName("Columnas", balanced(count, parts, true)), colsLayout(balanced(count, parts, true)));
        add(countsName("Columnas", balanced(count, parts, false)), colsLayout(balanced(count, parts, false)));
    }
    if (count <= 4) {
        add(QStringLiteral("En fila"), colsLayout(std::vector<int>(size_t(count), 1)));
        add(QStringLiteral("En columna"), rowsLayout(std::vector<int>(size_t(count), 1)));
    }
    add(QStringLiteral("Grande a la izquierda"), heroLayout(count, 0, 0.6));
    add(QStringLiteral("Grande arriba"), heroLayout(count, 1, 0.6));
    add(QStringLiteral("Grande a la derecha"), heroLayout(count, 2, 0.6));
    add(QStringLiteral("Grande abajo"), heroLayout(count, 3, 0.6));
    if (count >= 4) {
        // a big picture on top, the rest in rows of up to three underneath
        Rects r{QRectF(0, 0, 1, 0.5)};
        const int rest = count - 1, rows = (rest + 2) / 3;
        const std::vector<int> counts = balanced(rest, rows, true);
        for (size_t i = 0; i < counts.size(); ++i)
            for (int c = 0; c < counts[i]; ++c)
                r.push_back(QRectF(double(c) / counts[i], 0.5 + 0.5 * double(i) / counts.size(), 1.0 / counts[i], 0.5 / counts.size()));
        add(QStringLiteral("Principal y filas"), r);
    }
    for (unsigned seed = 1; seed <= 4; ++seed) {
        Layout m = mosaicLayout(count, seed);
        add(QStringLiteral("Mosaico %1").arg(seed), m.rects);
    }
    return out;
}

// ---- dividing lines ---------------------------------------------------------------------------------------------------

namespace {

constexpr double kEps = 1e-3;

struct Edge {
    double pos;
    double lo, hi;
    int cell;
    bool ends; // the cell ends here (it is before the line), or starts here
};

} // namespace

std::vector<Divider> findDividers(const std::vector<Cell> &cells)
{
    std::vector<Divider> out;
    for (int orientation = 0; orientation < 2; ++orientation) {
        const bool vertical = orientation == 0;
        std::vector<Edge> edges;
        for (size_t i = 0; i < cells.size(); ++i) {
            const QRectF &r = cells[i].rect;
            const double start = vertical ? r.left() : r.top();
            const double end = vertical ? r.right() : r.bottom();
            const double lo = vertical ? r.top() : r.left();
            const double hi = vertical ? r.bottom() : r.right();
            if (start > kEps && start < 1.0 - kEps)
                edges.push_back({start, lo, hi, int(i), false});
            if (end > kEps && end < 1.0 - kEps)
                edges.push_back({end, lo, hi, int(i), true});
        }
        std::sort(edges.begin(), edges.end(), [](const Edge &a, const Edge &b) { return a.pos < b.pos; });
        for (size_t i = 0; i < edges.size();) {
            // every edge at (nearly) this position
            size_t j = i;
            while (j < edges.size() && edges[j].pos - edges[i].pos < kEps)
                ++j;
            std::vector<Edge> group(edges.begin() + long(i), edges.begin() + long(j));
            i = j;
            // the stretches this position has an edge along, joined where they touch
            std::vector<std::pair<double, double>> spans;
            for (const Edge &e : group)
                spans.push_back({e.lo, e.hi});
            std::sort(spans.begin(), spans.end());
            std::vector<std::pair<double, double>> merged;
            for (const auto &s : spans) {
                if (!merged.empty() && s.first <= merged.back().second + kEps)
                    merged.back().second = std::max(merged.back().second, s.second);
                else
                    merged.push_back(s);
            }
            for (const auto &m : merged) {
                Divider d;
                d.vertical = vertical;
                d.from = m.first;
                d.to = m.second;
                double sum = 0;
                int n = 0;
                for (const Edge &e : group) {
                    if (e.lo < m.first - kEps || e.hi > m.second + kEps)
                        continue;
                    (e.ends ? d.before : d.after).push_back(e.cell);
                    sum += e.pos;
                    ++n;
                }
                if (d.before.empty() || d.after.empty())
                    continue; // a line with cells on one side only is not something to drag
                d.pos = sum / n;
                out.push_back(std::move(d));
            }
        }
    }
    return out;
}

double moveDivider(std::vector<Cell> &cells, const Divider &divider, double pos, double minSize)
{
    double lo = 0.0, hi = 1.0;
    for (int i : divider.before) {
        const QRectF &r = cells[size_t(i)].rect;
        lo = std::max(lo, (divider.vertical ? r.left() : r.top()) + minSize);
    }
    for (int i : divider.after) {
        const QRectF &r = cells[size_t(i)].rect;
        hi = std::min(hi, (divider.vertical ? r.right() : r.bottom()) - minSize);
    }
    if (lo > hi)
        return divider.pos; // the cells are too small to move it
    pos = std::clamp(pos, lo, hi);
    for (int i : divider.before) {
        QRectF &r = cells[size_t(i)].rect;
        if (divider.vertical)
            r.setRight(pos);
        else
            r.setBottom(pos);
    }
    for (int i : divider.after) {
        QRectF &r = cells[size_t(i)].rect;
        if (divider.vertical)
            r.setLeft(pos);
        else
            r.setTop(pos);
    }
    return pos;
}

} // namespace core::collage
