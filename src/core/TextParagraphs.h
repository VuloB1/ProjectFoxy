#pragma once

#include <QRectF>
#include <algorithm>
#include <vector>

namespace core {

// Groups the lines of a recognised text (in reading order) into paragraphs: a line continues the paragraph of the one
// before it when the gap between them is small next to their height, they overlap sideways and their letters are about
// the same size. Returns the paragraph number of every line, from 0.
inline std::vector<int> paragraphsOf(const std::vector<QRectF> &lines)
{
    std::vector<int> out(lines.size(), 0);
    for (size_t i = 1; i < lines.size(); ++i) {
        const QRectF &p = lines[i - 1], &c = lines[i];
        const double h = std::min(p.height(), c.height());
        const double gap = c.top() - p.bottom();
        const bool near = h > 0 && gap < 0.9 * h && gap > -0.6 * h;
        const bool overlaps = c.left() < p.right() && c.right() > p.left();
        const double ratio = c.height() > 0 ? p.height() / c.height() : 0;
        const bool sameSize = ratio > 0.6 && ratio < 1.67;
        out[i] = out[i - 1] + (near && overlaps && sameSize ? 0 : 1);
    }
    return out;
}

} // namespace core
