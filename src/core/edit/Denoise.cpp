#include "Denoise.h"

#include "ParallelRows.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <vector>

namespace core::edit {

namespace {

constexpr int kPatchR = 2;                       // the patches being compared are 5x5
constexpr int kSearchR = 3;                      // and are looked for in a 7x7 window
constexpr int kMargin = kPatchR + kSearchR;      // how far a band reads beyond its own rows
constexpr int kPatchSide = 2 * kPatchR + 1;
constexpr int kBand = 24;                        // rows per work item
constexpr float kFlatBlockBias = 1.12f;          // the tenth-lowest block variance reads a little under the truth

inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline bool stopped(const std::atomic<bool> *cancel) { return cancel && cancel->load(std::memory_order_relaxed); }
inline uint8_t toByte(float v) { return v <= 0.f ? 0 : (v >= 255.f ? 255 : static_cast<uint8_t>(v + 0.5f)); }

// The three planes the picture is worked in: Y (luma) and two colour differences. Exactly
// invertible: G = Y + Cg, R = Y - Cg + Co, B = Y - Cg - Co.
inline void toYCoCg(const uint8_t *px, float &y, float &co, float &cg)
{
    const float r = px[0], g = px[1], b = px[2];
    y = (r + 2.f * g + b) * 0.25f;
    co = (r - b) * 0.5f;
    cg = (2.f * g - r - b) * 0.25f;
}

inline void fromYCoCg(float y, float co, float cg, uint8_t *px)
{
    px[0] = toByte(y - cg + co);
    px[1] = toByte(y + cg);
    px[2] = toByte(y - cg - co);
}

// exp(-t) as (1 - t/32)^32: no table lookup, so the loops below vectorize.
inline float expNeg(float t)
{
    float u = std::max(1.f - t * (1.f / 32.f), 0.f);
    u *= u; // ^2
    u *= u; // ^4
    u *= u; // ^8
    u *= u; // ^16
    u *= u; // ^32
    return u;
}

// --- Noise measurement ---------------------------------------------------------------------

// Standard deviation of the noise in Y, Co and Cg. The Laplacian-like mask [1 -2 1; -2 4 -2; 1 -2
// 1] cancels any smooth content and turns white noise of deviation s into noise of deviation 6s;
// the MEDIAN of its absolute value (a robust choice: edges are few and only reach the tail) is
// 0.6745 times that deviation.
struct Sigma {
    float v[3];
};

Sigma estimateNoise(const QImage &src, const std::atomic<bool> *cancel)
{
    const int W = src.width(), H = src.height();
    constexpr int kBins = 1024; // |L| in steps of 0.25
    std::mutex lock;
    std::vector<unsigned> total(3 * kBins, 0);
    std::vector<float> blockVar[3]; // variance left in every flat-ish 8x8 block, per plane
    forEachBandParallel(H, 48, [&](int y0, int y1) {
        if (stopped(cancel))
            return;
        std::vector<unsigned> hist(3 * kBins, 0);
        std::vector<float> localVar[3];
        for (int by = y0; by + 8 <= y1; by += 8)
            for (int bx = 0; bx + 8 <= W; bx += 8) {
                double sv[3] = {0, 0, 0}, sxv[3] = {0, 0, 0}, syv[3] = {0, 0, 0}, sv2[3] = {0, 0, 0};
                for (int j = 0; j < 8; ++j) {
                    const uint8_t *row = src.constScanLine(by + j) + size_t(bx) * 4;
                    for (int i = 0; i < 8; ++i) {
                        float v[3];
                        toYCoCg(row + size_t(i) * 4, v[0], v[1], v[2]);
                        const double xc = i - 3.5, yc = j - 3.5;
                        for (int p = 0; p < 3; ++p) {
                            sv[p] += v[p];
                            sxv[p] += xc * v[p];
                            syv[p] += yc * v[p];
                            sv2[p] += double(v[p]) * v[p];
                        }
                    }
                }
                // residual after removing the block's mean and straight-line gradient
                for (int p = 0; p < 3; ++p) {
                    const double rss = sv2[p] - sv[p] * sv[p] / 64.0 - sxv[p] * sxv[p] / 336.0 - syv[p] * syv[p] / 336.0;
                    localVar[p].push_back(float(std::max(rss, 0.0) / 61.0));
                }
            }
        std::vector<float> buf(size_t(3) * 3 * W); // 3 planes x 3 rows
        for (int y = std::max(y0, 1); y < std::min(y1, H - 1); ++y) {
            if (y % 3 != 1) // every third row is plenty
                continue;
            for (int r = 0; r < 3; ++r) {
                const uint8_t *s = src.constScanLine(y - 1 + r);
                for (int x = 0; x < W; ++x)
                    toYCoCg(s + size_t(x) * 4, buf[size_t(0 * 3 + r) * W + x], buf[size_t(1 * 3 + r) * W + x],
                            buf[size_t(2 * 3 + r) * W + x]);
            }
            for (int p = 0; p < 3; ++p) {
                const float *a = &buf[size_t(p * 3 + 0) * W], *b = &buf[size_t(p * 3 + 1) * W], *c = &buf[size_t(p * 3 + 2) * W];
                for (int x = 1; x < W - 1; ++x) {
                    const float l = a[x - 1] + a[x + 1] + c[x - 1] + c[x + 1] - 2.f * (a[x] + c[x] + b[x - 1] + b[x + 1]) + 4.f * b[x];
                    ++hist[size_t(p) * kBins + std::min(int(std::fabs(l) * 4.f), kBins - 1)];
                }
            }
        }
        std::lock_guard<std::mutex> guard(lock);
        for (size_t i = 0; i < total.size(); ++i)
            total[i] += hist[i];
        for (int p = 0; p < 3; ++p)
            blockVar[p].insert(blockVar[p].end(), localVar[p].begin(), localVar[p].end());
    });

    Sigma s{{1.f, 1.f, 1.f}};
    for (int p = 0; p < 3; ++p) {
        unsigned long long count = 0;
        for (int b = 0; b < kBins; ++b)
            count += total[size_t(p) * kBins + b];
        if (count == 0)
            continue;
        unsigned long long seen = 0;
        int bin = 0;
        for (; bin < kBins; ++bin) {
            seen += total[size_t(p) * kBins + bin];
            if (seen * 2 >= count)
                break;
        }
        const float median = (bin + 0.5f) * 0.25f;
        float estimate = median / (0.6745f * 6.f);
        // The Laplacian only sees noise made of independent pixels. Camera noise is blotchier (demosaicing,
        // compression), so also take the variance of the flattest blocks - the lowest tenth, where there
        // is nothing but noise - and keep the larger of the two.
        std::vector<float> &bv = blockVar[p];
        if (bv.size() >= 20) {
            const size_t tenth = bv.size() / 10;
            std::nth_element(bv.begin(), bv.begin() + tenth, bv.end());
            estimate = std::max(estimate, std::sqrt(bv[tenth]) * kFlatBlockBias);
        }
        s.v[p] = clampf(estimate, 0.5f, 50.f);
    }
    return s;
}

// --- Non-local means -----------------------------------------------------------------------

// Denoises the rows [y0, y1) of `src` into `out`. Everything is measured in units of the noise's
// own deviation (the planes are divided by sigma first), so one strength means the same for the
// three planes.
//
// For every offset (dx, dy) of the search window, the squared difference between the picture and
// itself shifted by that offset is summed over every 5x5 patch (two separable box sums). That is
// how different the patch around a pixel is from the patch around its neighbour at (dx, dy); the
// neighbour is averaged in with a weight that falls from 1 (identical) to 0 as that difference
// rises above what noise alone would cause (2 per value: two independent noises).
void nlmBand(const QImage &src, QImage &out, int y0, int y1, const Sigma &sigma, float invHY2, float invHC2,
             const std::atomic<bool> *cancel)
{
    const int W = src.width(), H = src.height(), bh = y1 - y0;
    const int pw = W + 2 * kMargin, ph = bh + 2 * kMargin;
    const float inv[3] = {1.f / sigma.v[0], 1.f / sigma.v[1], 1.f / sigma.v[2]};

    // The band and its margin as normalized planes (border pixels replicated).
    std::vector<float> ny(size_t(pw) * ph), nco(size_t(pw) * ph), ncg(size_t(pw) * ph);
    for (int j = 0; j < ph; ++j) {
        const uint8_t *row = src.constScanLine(clampi(y0 - kMargin + j, 0, H - 1));
        for (int i = 0; i < pw; ++i) {
            float y, co, cg;
            toYCoCg(row + size_t(clampi(i - kMargin, 0, W - 1)) * 4, y, co, cg);
            const size_t k = size_t(j) * pw + i;
            ny[k] = y * inv[0];
            nco[k] = co * inv[1];
            ncg[k] = cg * inv[2];
        }
    }

    // Weighted sums per output pixel, started with the pixel itself at weight 1.
    const size_t n = size_t(W) * bh;
    std::vector<float> accY(n), accCo(n), accCg(n), sumY(n, 1.f), sumC(n, 1.f);
    for (int y = 0; y < bh; ++y)
        for (int x = 0; x < W; ++x) {
            const size_t k = size_t(y + kMargin) * pw + (x + kMargin);
            accY[size_t(y) * W + x] = ny[k];
            accCo[size_t(y) * W + x] = nco[k];
            accCg[size_t(y) * W + x] = ncg[k];
        }

    const int ew = W + 2 * kPatchR, eh = bh + 2 * kPatchR;
    std::vector<float> diff(size_t(ew) * eh), boxed(size_t(eh) * W);
    const float perValue = 1.f / float(3 * kPatchSide * kPatchSide);

    for (int dy = -kSearchR; dy <= kSearchR; ++dy)
        for (int dx = -kSearchR; dx <= kSearchR; ++dx) {
            if (dx == 0 && dy == 0)
                continue;
            if (stopped(cancel))
                return;
            const std::ptrdiff_t shift = std::ptrdiff_t(dy) * pw + dx;

            // squared difference with the shifted picture, over the band plus the patch margin
            for (int v = 0; v < eh; ++v) {
                const size_t base = size_t(v + kSearchR) * pw + kSearchR;
                const float *y0p = &ny[base], *y1p = &ny[base + shift];
                const float *c0p = &nco[base], *c1p = &nco[base + shift];
                const float *g0p = &ncg[base], *g1p = &ncg[base + shift];
                float *e = &diff[size_t(v) * ew];
                for (int u = 0; u < ew; ++u) {
                    const float a = y0p[u] - y1p[u], b = c0p[u] - c1p[u], c = g0p[u] - g1p[u];
                    e[u] = a * a + b * b + c * c;
                }
            }
            // 5-wide box sum along the rows...
            for (int v = 0; v < eh; ++v) {
                const float *e = &diff[size_t(v) * ew];
                float *h = &boxed[size_t(v) * W];
                for (int x = 0; x < W; ++x)
                    h[x] = e[x] + e[x + 1] + e[x + 2] + e[x + 3] + e[x + 4];
            }
            // ...and 5-high down the columns, then the weights
            for (int y = 0; y < bh; ++y) {
                const float *h0 = &boxed[size_t(y) * W], *h1 = h0 + W, *h2 = h1 + W, *h3 = h2 + W, *h4 = h3 + W;
                const float *nyr = &ny[size_t(y + kMargin + dy) * pw + kMargin + dx];
                const float *ncor = &nco[size_t(y + kMargin + dy) * pw + kMargin + dx];
                const float *ncgr = &ncg[size_t(y + kMargin + dy) * pw + kMargin + dx];
                float *aY = &accY[size_t(y) * W], *aCo = &accCo[size_t(y) * W], *aCg = &accCg[size_t(y) * W];
                float *sY = &sumY[size_t(y) * W], *sC = &sumC[size_t(y) * W];
                for (int x = 0; x < W; ++x) {
                    const float excess = std::max((h0[x] + h1[x] + h2[x] + h3[x] + h4[x]) * perValue - 2.f, 0.f);
                    const float wy = expNeg(excess * invHY2);
                    const float wc = expNeg(excess * invHC2);
                    sY[x] += wy;
                    aY[x] += wy * nyr[x];
                    sC[x] += wc;
                    aCo[x] += wc * ncor[x];
                    aCg[x] += wc * ncgr[x];
                }
            }
        }

    for (int y = 0; y < bh; ++y) {
        const uint8_t *s = src.constScanLine(y0 + y);
        uint8_t *d = out.scanLine(y0 + y);
        for (int x = 0; x < W; ++x) {
            const size_t k = size_t(y) * W + x;
            fromYCoCg(accY[k] / sumY[k] * sigma.v[0], accCo[k] / sumC[k] * sigma.v[1], accCg[k] / sumC[k] * sigma.v[2],
                      d + size_t(x) * 4);
            d[size_t(x) * 4 + 3] = s[size_t(x) * 4 + 3];
        }
    }
}

} // namespace

QImage denoiseColor(const QImage &source, double luma, double chroma, const std::atomic<bool> *cancel)
{
    luma = std::clamp(luma, 0.0, 100.0);
    chroma = std::clamp(chroma, 0.0, 100.0);
    if (source.isNull() || source.format() != QImage::Format_RGBA8888 || source.width() < 2 || source.height() < 2
        || (luma <= 0.0 && chroma <= 0.0))
        return source;

    const Sigma sigma = estimateNoise(source, cancel);
    // The averaging strength, in units of the noise deviation. Colour noise is blotchier and far
    // more objectionable than luma noise, so its scale goes much higher.
    const float hY = 0.06f + 1.2f * float(luma) / 100.f;
    const float hC = 0.06f + 2.4f * float(chroma) / 100.f;
    const float invHY2 = 1.f / (hY * hY), invHC2 = 1.f / (hC * hC);

    QImage out(source.size(), QImage::Format_RGBA8888);
    out.bits(); // detach once, before the workers write
    forEachBandParallel(source.height(), kBand, [&](int y0, int y1) {
        if (!stopped(cancel))
            nlmBand(source, out, y0, y1, sigma, invHY2, invHC2, cancel);
    });
    return out;
}

} // namespace core::edit
