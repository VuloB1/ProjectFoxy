#pragma once

#include <QImage>
#include <array>

namespace core {

// Brightness distribution across 256 buckets: perceived brightness (luma) plus
// the three color channels separately. The Info popup only draws luma; the
// Ajustes histogram, Levels and Curves editors also draw the channels.
struct Histogram {
    std::array<int, 256> luma{};
    std::array<int, 256> r{};
    std::array<int, 256> g{};
    std::array<int, 256> b{};
    int maxCount = 1; // luma's tallest bucket; never 0, so callers can safely divide by it
};

// Computes the histogram from `image`. Large images are subsampled (a
// histogram's shape doesn't need every pixel, just a representative
// distribution), so this stays fast enough to call synchronously on the GUI
// thread whenever the structural bake changes.
Histogram computeHistogram(const QImage &image);

} // namespace core
