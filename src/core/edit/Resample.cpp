#include "Resample.h"
#include "ParallelRows.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace core::edit {

namespace {

constexpr double kPi = 3.14159265358979323846;

double sinc(double x)
{
    if (std::abs(x) < 1e-9)
        return 1.0;
    return std::sin(kPi * x) / (kPi * x);
}

double lanczos3(double x)
{
    x = std::abs(x);
    return x >= 3.0 ? 0.0 : sinc(x) * sinc(x / 3.0);
}

// For every output index: the run of source indices that feed it and their
// weights (normalized to sum to 1). When shrinking, the kernel is stretched by
// the shrink factor so it averages the whole footprint of the output pixel.
struct Taps {
    int stride = 0;
    std::vector<int> start;
    std::vector<int> count;
    std::vector<float> weights; // `stride` slots per output index

    const float *weightsAt(int i) const { return &weights[size_t(i) * stride]; }
    int lastSource(int i) const { return start[i] + count[i] - 1; }
};

Taps buildTaps(int srcLen, int dstLen)
{
    Taps taps;
    const double scale = double(dstLen) / srcLen;
    const double kernelScale = std::max(1.0, 1.0 / scale);
    const double support = 3.0 * kernelScale;
    taps.stride = int(std::ceil(support * 2.0)) + 2;
    taps.start.assign(dstLen, 0);
    taps.count.assign(dstLen, 1);
    taps.weights.assign(size_t(dstLen) * taps.stride, 0.0f);

    std::vector<double> w(taps.stride);
    for (int i = 0; i < dstLen; ++i) {
        const double center = (i + 0.5) / scale - 0.5;
        const int first = std::max(0, static_cast<int>(std::ceil(center - support)));
        const int last = std::min(srcLen - 1, static_cast<int>(std::floor(center + support)));
        float *out = &taps.weights[size_t(i) * taps.stride];
        if (last < first) {
            taps.start[i] = std::clamp(static_cast<int>(std::lround(center)), 0, srcLen - 1);
            taps.count[i] = 1;
            out[0] = 1.0f;
            continue;
        }
        const int n = last - first + 1;
        double sum = 0.0;
        for (int k = 0; k < n; ++k) {
            w[k] = lanczos3((first + k - center) / kernelScale);
            sum += w[k];
        }
        taps.start[i] = first;
        taps.count[i] = n;
        if (std::abs(sum) < 1e-9) {
            taps.start[i] = std::clamp(static_cast<int>(std::lround(center)), 0, srcLen - 1);
            taps.count[i] = 1;
            out[0] = 1.0f;
            continue;
        }
        for (int k = 0; k < n; ++k)
            out[k] = static_cast<float>(w[k] / sum);
    }
    return taps;
}

// Sharpens the luminance of the rows [firstOut, lastOut) of `v` (a band of
// premultiplied RGBA floats, 0..255, `rows` rows of `width` pixels). Rows
// outside that range are context only: the blur and the neighborhood limit
// need them, and they belong to the neighboring bands.
void enhanceBand(std::vector<float> &v, int rows, int width, int firstOut, int lastOut, const EnhanceParams &params)
{
    const int radius = std::max(1, static_cast<int>(std::ceil(3.0 * params.sigma)));
    std::vector<float> kernel(2 * radius + 1);
    float kernelSum = 0.0f;
    for (int k = -radius; k <= radius; ++k) {
        kernel[k + radius] = static_cast<float>(std::exp(-(k * k) / (2.0 * params.sigma * params.sigma)));
        kernelSum += kernel[k + radius];
    }
    for (float &k : kernel)
        k /= kernelSum;

    const size_t plane = size_t(rows) * width;
    std::vector<float> luma(plane);
    std::vector<float> blurH(plane);
    for (size_t i = 0; i < plane; ++i) {
        const float *p = &v[i * 4];
        luma[i] = 0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2];
    }

    for (int row = 0; row < rows; ++row) {
        const float *src = &luma[size_t(row) * width];
        float *dst = &blurH[size_t(row) * width];
        for (int x = 0; x < width; ++x) {
            float sum = 0.0f;
            for (int k = -radius; k <= radius; ++k)
                sum += kernel[k + radius] * src[std::clamp(x + k, 0, width - 1)];
            dst[x] = sum;
        }
    }

    for (int row = firstOut; row < lastOut; ++row) {
        const int rowUp = std::max(0, row - 1);
        const int rowDown = std::min(rows - 1, row + 1);
        for (int x = 0; x < width; ++x) {
            float blur = 0.0f;
            for (int k = -radius; k <= radius; ++k)
                blur += kernel[k + radius] * blurH[size_t(std::clamp(row + k, 0, rows - 1)) * width + x];

            const float y = luma[size_t(row) * width + x];
            // Ignore the tiniest differences (interpolation noise) and let the
            // real edges through almost untouched.
            float d = y - blur;
            d *= std::abs(d) / (std::abs(d) + 1.5f);
            float sharpened = y + float(params.amount) * d;

            // Never leave the range of the 3x3 neighborhood: that is what
            // keeps the sharpening from drawing halos next to an edge.
            float lo = y;
            float hi = y;
            const int xl = std::max(0, x - 1);
            const int xr = std::min(width - 1, x + 1);
            for (int r : {rowUp, row, rowDown}) {
                const float *line = &luma[size_t(r) * width];
                lo = std::min({lo, line[xl], line[x], line[xr]});
                hi = std::max({hi, line[xl], line[x], line[xr]});
            }
            sharpened = std::clamp(sharpened, lo, hi);

            const float delta = sharpened - y;
            float *p = &v[(size_t(row) * width + x) * 4];
            const float alpha = p[3];
            for (int c = 0; c < 3; ++c)
                p[c] = std::clamp(p[c] + delta, 0.0f, alpha);
        }
    }
}

} // namespace

EnhanceParams enhanceParamsFor(double enlargement)
{
    EnhanceParams p;
    // Fitted by enlarging photos that had been shrunk on purpose and
    // measuring which settings land closest to the originals: the more the
    // picture grows, the wider the blur that is sharpened and the stronger the
    // boost (the neighborhood limit below keeps a strong boost from ringing).
    p.sigma = std::clamp(0.35 + 0.30 * enlargement, 0.8, 2.0);
    p.amount = std::clamp(0.6 + 0.4 * enlargement, 0.8, 2.2);
    return p;
}

QImage resizeNearest(const QImage &source, QSize targetSize, bool keepAspectRatio)
{
    if (source.isNull() || targetSize.isEmpty())
        return source;
    QSize size = keepAspectRatio ? source.size().scaled(targetSize, Qt::KeepAspectRatio) : targetSize;
    size = size.expandedTo(QSize(1, 1));
    if (size == source.size())
        return source;

    const QImage src = source.format() == QImage::Format_RGBA8888 ? source : source.convertToFormat(QImage::Format_RGBA8888);
    const int sw = src.width(), sh = src.height(), dw = size.width(), dh = size.height();
    // The source column / row under the centre of every output one, worked out once.
    std::vector<int> columns(static_cast<size_t>(dw));
    std::vector<int> rowsMap(static_cast<size_t>(dh));
    for (int x = 0; x < dw; ++x)
        columns[size_t(x)] = std::min(sw - 1, int((double(x) + 0.5) * sw / dw));
    for (int y = 0; y < dh; ++y)
        rowsMap[size_t(y)] = std::min(sh - 1, int((double(y) + 0.5) * sh / dh));

    QImage out(size, QImage::Format_RGBA8888);
    out.bits(); // allocate before the worker threads touch it
    forEachRowParallel(dh, [&](int y) {
        const quint32 *s = reinterpret_cast<const quint32 *>(src.constScanLine(rowsMap[size_t(y)]));
        quint32 *d = reinterpret_cast<quint32 *>(out.scanLine(y));
        for (int x = 0; x < dw; ++x)
            d[x] = s[columns[size_t(x)]];
    });
    return out;
}

QImage resizeHighQuality(const QImage &source, QSize targetSize, bool keepAspectRatio, bool enhanceDetail)
{
    if (source.isNull() || targetSize.isEmpty())
        return source;
    QSize size = keepAspectRatio ? source.size().scaled(targetSize, Qt::KeepAspectRatio) : targetSize;
    size = size.expandedTo(QSize(1, 1));
    if (size == source.size())
        return source;

    // Filtering happens on premultiplied color so a transparent pixel's
    // (arbitrary) color can't bleed into its opaque neighbors.
    const bool premultiplied = source.hasAlphaChannel();
    const QImage src = source.convertToFormat(premultiplied ? QImage::Format_RGBA8888_Premultiplied
                                                            : QImage::Format_RGBA8888);
    const int sw = src.width();
    const int sh = src.height();
    const int dw = size.width();
    const int dh = size.height();

    const double enlargement = std::sqrt((double(dw) / sw) * (double(dh) / sh));
    const bool enhance = enhanceDetail && enlargement > 1.05;
    const EnhanceParams params = enhanceParamsFor(enlargement);
    const int halo = enhance ? static_cast<int>(std::ceil(3.0 * params.sigma)) + 2 : 0;

    const Taps tapsX = buildTaps(sw, dw);
    const Taps tapsY = buildTaps(sh, dh);

    QImage out(size, src.format());
    out.bits(); // allocate before the worker threads touch it

    constexpr int kBandRows = 32;
    forEachBandParallel(dh, kBandRows, [&](int y0, int y1) {
        const int ya = std::max(0, y0 - halo);
        const int yb = std::min(dh, y1 + halo);
        const int rows = yb - ya;

        int srcFirst = tapsY.start[ya];
        int srcLast = tapsY.lastSource(ya);
        for (int y = ya; y < yb; ++y) {
            srcFirst = std::min(srcFirst, tapsY.start[y]);
            srcLast = std::max(srcLast, tapsY.lastSource(y));
        }
        const int srcRows = srcLast - srcFirst + 1;

        // Horizontal pass over just the source rows this band needs.
        std::vector<float> h(size_t(srcRows) * dw * 4);
        for (int r = 0; r < srcRows; ++r) {
            const quint8 *line = src.constScanLine(srcFirst + r);
            float *dst = &h[size_t(r) * dw * 4];
            for (int x = 0; x < dw; ++x) {
                const float *w = tapsX.weightsAt(x);
                const quint8 *p = line + size_t(tapsX.start[x]) * 4;
                float a0 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
                for (int k = 0, n = tapsX.count[x]; k < n; ++k, p += 4) {
                    a0 += w[k] * p[0];
                    a1 += w[k] * p[1];
                    a2 += w[k] * p[2];
                    a3 += w[k] * p[3];
                }
                dst[x * 4] = a0;
                dst[x * 4 + 1] = a1;
                dst[x * 4 + 2] = a2;
                dst[x * 4 + 3] = a3;
            }
        }

        // Vertical pass: each output row is a weighted sum of whole rows.
        std::vector<float> v(size_t(rows) * dw * 4, 0.0f);
        for (int y = ya; y < yb; ++y) {
            float *acc = &v[size_t(y - ya) * dw * 4];
            const float *w = tapsY.weightsAt(y);
            for (int k = 0, n = tapsY.count[y]; k < n; ++k) {
                const float *row = &h[size_t(tapsY.start[y] + k - srcFirst) * dw * 4];
                const float weight = w[k];
                for (int j = 0, total = dw * 4; j < total; ++j)
                    acc[j] += weight * row[j];
            }
            for (int x = 0; x < dw; ++x) {
                const float alpha = std::clamp(acc[x * 4 + 3], 0.0f, 255.0f);
                acc[x * 4 + 3] = alpha;
                for (int c = 0; c < 3; ++c)
                    acc[x * 4 + c] = std::clamp(acc[x * 4 + c], 0.0f, alpha);
            }
        }

        if (enhance)
            enhanceBand(v, rows, dw, y0 - ya, y1 - ya, params);

        for (int y = y0; y < y1; ++y) {
            const float *from = &v[size_t(y - ya) * dw * 4];
            quint8 *to = out.scanLine(y);
            for (int j = 0, total = dw * 4; j < total; ++j)
                to[j] = static_cast<quint8>(std::clamp(static_cast<int>(from[j] + 0.5f), 0, 255));
        }
    });

    return premultiplied ? out.convertToFormat(QImage::Format_RGBA8888) : out;
}

} // namespace core::edit
