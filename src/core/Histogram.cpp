#include "Histogram.h"

#include <algorithm>
#include <cmath>

namespace core {

Histogram computeHistogram(const QImage &image)
{
    Histogram h;
    if (image.isNull())
        return h;

    const QImage img = image.format() == QImage::Format_RGBA8888
        ? image
        : image.convertToFormat(QImage::Format_RGBA8888);
    const int width = img.width();
    const int height = img.height();
    if (width <= 0 || height <= 0)
        return h;

    // Subsample toward ~1 megapixel worth of samples for a big image instead
    // of walking every pixel - the histogram's shape converges well before
    // that, and this runs synchronously on the GUI thread.
    const double totalPixels = double(width) * double(height);
    const int step = std::max(1, static_cast<int>(std::sqrt(totalPixels / 1'000'000.0)));

    for (int y = 0; y < height; y += step) {
        const auto *line = img.scanLine(y);
        for (int x = 0; x < width; x += step) {
            const quint8 *px = line + x * 4;
            const int luma = qRound(0.299 * px[0] + 0.587 * px[1] + 0.114 * px[2]);
            ++h.luma[std::clamp(luma, 0, 255)];
            ++h.r[px[0]];
            ++h.g[px[1]];
            ++h.b[px[2]];
        }
    }

    h.maxCount = *std::max_element(h.luma.begin(), h.luma.end());
    if (h.maxCount <= 0)
        h.maxCount = 1;
    return h;
}

} // namespace core
