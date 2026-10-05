// An animated GIF writer: colour quantising (median cut over a 5-bit-per-channel histogram), optional
// Floyd-Steinberg dithering, frame differencing (each frame stores only the rectangle that changed, the
// rest is "transparent" so the previous frame shows through), and the LZW compression GIF needs.

#include "Anim.h"

#include <QByteArray>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace core::anim {

namespace {

// ---- colour histogram and palette ------------------------------------------------------------------------------

constexpr int kBins = 32768;
inline int binOf(int r, int g, int b) { return ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3); }

struct Histogram {
    std::vector<uint32_t> n = std::vector<uint32_t>(kBins, 0);
    std::vector<uint64_t> sr = std::vector<uint64_t>(kBins, 0), sg = std::vector<uint64_t>(kBins, 0), sb = std::vector<uint64_t>(kBins, 0);
    uint64_t total = 0;

    void add(int r, int g, int b)
    {
        const int i = binOf(r, g, b);
        ++n[size_t(i)];
        sr[size_t(i)] += uint64_t(r);
        sg[size_t(i)] += uint64_t(g);
        sb[size_t(i)] += uint64_t(b);
        ++total;
    }
};

// What the eye minds: green most, then red, then blue.
inline int colorDistance(int r1, int g1, int b1, int r2, int g2, int b2)
{
    const int dr = r1 - r2, dg = g1 - g2, db = b1 - b2;
    return 2 * dr * dr + 4 * dg * dg + 3 * db * db;
}

// Median cut: repeatedly split the box with the most pixels (weighted by how wide it is) at the
// median of its widest channel; each final box becomes the average colour of what it holds.
std::vector<unsigned> medianCut(const Histogram &h, int maxColors)
{
    struct Box {
        std::vector<int> bins;
        uint64_t count = 0;
        int lo[3] = {255, 255, 255}, hi[3] = {0, 0, 0};
        double priority() const
        {
            const int range = std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]});
            return bins.size() > 1 ? double(count) * (range + 1) : -1.0;
        }
    };
    auto average = [&](int bin, int ch) -> int {
        const uint64_t c = h.n[size_t(bin)];
        return int(((ch == 0 ? h.sr : ch == 1 ? h.sg : h.sb)[size_t(bin)] + c / 2) / c);
    };
    auto measure = [&](Box &b) {
        b.count = 0;
        for (int c = 0; c < 3; ++c) { b.lo[c] = 255; b.hi[c] = 0; }
        for (int bin : b.bins) {
            b.count += h.n[size_t(bin)];
            for (int c = 0; c < 3; ++c) {
                const int v = average(bin, c);
                b.lo[c] = std::min(b.lo[c], v);
                b.hi[c] = std::max(b.hi[c], v);
            }
        }
    };

    std::vector<Box> boxes(1);
    for (int i = 0; i < kBins; ++i)
        if (h.n[size_t(i)])
            boxes[0].bins.push_back(i);
    if (boxes[0].bins.empty())
        return {0x000000};
    measure(boxes[0]);

    while (int(boxes.size()) < maxColors) {
        size_t best = 0;
        double bestP = -1.0;
        for (size_t i = 0; i < boxes.size(); ++i)
            if (boxes[i].priority() > bestP) { bestP = boxes[i].priority(); best = i; }
        if (bestP < 0.0)
            break; // nothing left to split
        Box &box = boxes[best];
        int axis = 0;
        for (int c = 1; c < 3; ++c)
            if ((box.hi[c] - box.lo[c]) * (c == 1 ? 1.4 : (c == 0 ? 1.1 : 0.8)) >
                (box.hi[axis] - box.lo[axis]) * (axis == 1 ? 1.4 : (axis == 0 ? 1.1 : 0.8)))
                axis = c;
        std::sort(box.bins.begin(), box.bins.end(), [&](int a, int b) { return average(a, axis) < average(b, axis); });
        uint64_t half = box.count / 2, run = 0;
        size_t cut = 1;
        for (size_t i = 0; i < box.bins.size(); ++i) {
            run += h.n[size_t(box.bins[i])];
            if (run >= half) { cut = i + 1; break; }
        }
        cut = std::clamp<size_t>(cut, 1, box.bins.size() - 1);
        Box right;
        right.bins.assign(box.bins.begin() + long(cut), box.bins.end());
        box.bins.resize(cut);
        measure(box);
        measure(right);
        boxes.push_back(std::move(right));
    }

    std::vector<unsigned> palette;
    for (const Box &b : boxes) {
        uint64_t r = 0, g = 0, bl = 0, n = 0;
        for (int bin : b.bins) {
            r += h.sr[size_t(bin)]; g += h.sg[size_t(bin)]; bl += h.sb[size_t(bin)]; n += h.n[size_t(bin)];
        }
        if (n == 0)
            continue;
        palette.push_back((unsigned(r / n) << 16) | (unsigned(g / n) << 8) | unsigned(bl / n));
    }
    return palette;
}

// Median cut leaves boxes whose averages are only roughly where the colours cluster; a few rounds of
// k-means (every colour to its nearest entry, every entry to the mean of its colours) pull the palette
// towards what is really in the pictures, which is what keeps smooth gradients and flat backgrounds clean.
void refinePalette(const Histogram &h, std::vector<unsigned> &palette, int rounds)
{
    if (palette.size() < 2)
        return;
    struct Pt { float r, g, b; uint32_t n; };
    std::vector<Pt> pts;
    for (int bin = 0; bin < kBins; ++bin) {
        const uint32_t n = h.n[size_t(bin)];
        if (n)
            pts.push_back({float(h.sr[size_t(bin)]) / n, float(h.sg[size_t(bin)]) / n, float(h.sb[size_t(bin)]) / n, n});
    }
    const size_t K = palette.size();
    std::vector<float> pr(K), pg(K), pb(K);
    for (size_t k = 0; k < K; ++k) {
        pr[k] = float((palette[k] >> 16) & 255); pg[k] = float((palette[k] >> 8) & 255); pb[k] = float(palette[k] & 255);
    }
    std::vector<double> sr(K), sg(K), sb(K), sn(K);
    for (int round = 0; round < rounds; ++round) {
        std::fill(sr.begin(), sr.end(), 0.0); std::fill(sg.begin(), sg.end(), 0.0);
        std::fill(sb.begin(), sb.end(), 0.0); std::fill(sn.begin(), sn.end(), 0.0);
        for (const Pt &p : pts) {
            size_t best = 0;
            float bestD = 1e30f;
            for (size_t k = 0; k < K; ++k) {
                const float dr = p.r - pr[k], dg = p.g - pg[k], db = p.b - pb[k];
                const float d = 2 * dr * dr + 4 * dg * dg + 3 * db * db;
                if (d < bestD) { bestD = d; best = k; }
            }
            sr[best] += double(p.r) * p.n; sg[best] += double(p.g) * p.n; sb[best] += double(p.b) * p.n; sn[best] += p.n;
        }
        for (size_t k = 0; k < K; ++k)
            if (sn[k] > 0) { pr[k] = float(sr[k] / sn[k]); pg[k] = float(sg[k] / sn[k]); pb[k] = float(sb[k] / sn[k]); }
    }
    for (size_t k = 0; k < K; ++k)
        palette[k] = (unsigned(std::clamp(int(pr[k] + 0.5f), 0, 255)) << 16) | (unsigned(std::clamp(int(pg[k] + 0.5f), 0, 255)) << 8)
                     | unsigned(std::clamp(int(pb[k] + 0.5f), 0, 255));
}

// For every 5-bit colour, the palette entry that is nearest to it (to the colours really seen in that bin when
// there are any, so a bin that holds nothing but black is matched as black and not as "a little above it").
struct Nearest {
    std::vector<uint8_t> lut = std::vector<uint8_t>(kBins, 0);
    void build(const std::vector<unsigned> &palette, int firstIndex, const Histogram *seen = nullptr)
    {
        for (int bin = 0; bin < kBins; ++bin) {
            int r = ((bin >> 10) & 31) * 8 + 4, g = ((bin >> 5) & 31) * 8 + 4, b = (bin & 31) * 8 + 4;
            if (seen && seen->n[size_t(bin)]) {
                const uint64_t n = seen->n[size_t(bin)];
                r = int((seen->sr[size_t(bin)] + n / 2) / n); g = int((seen->sg[size_t(bin)] + n / 2) / n); b = int((seen->sb[size_t(bin)] + n / 2) / n);
            }
            int best = 0, bestD = 1 << 30;
            for (size_t i = 0; i < palette.size(); ++i) {
                const int d = colorDistance(r, g, b, int((palette[i] >> 16) & 255), int((palette[i] >> 8) & 255), int(palette[i] & 255));
                if (d < bestD) { bestD = d; best = int(i); }
            }
            lut[size_t(bin)] = uint8_t(best + firstIndex);
        }
    }
};

// ---- LZW ----------------------------------------------------------------------------------------------------------

class BitWriter {
public:
    explicit BitWriter(QByteArray &out) : m_out(out) {}
    void put(int code, int bits)
    {
        m_acc |= uint32_t(code) << m_n;
        m_n += bits;
        while (m_n >= 8) {
            byte(uint8_t(m_acc & 255));
            m_acc >>= 8;
            m_n -= 8;
        }
    }
    void finish()
    {
        if (m_n > 0)
            byte(uint8_t(m_acc & 255));
        m_acc = 0;
        m_n = 0;
        flushBlock();
    }

private:
    void byte(uint8_t b)
    {
        m_block[m_len++] = char(b);
        if (m_len == 255)
            flushBlock();
    }
    void flushBlock()
    {
        if (m_len == 0)
            return;
        m_out.append(char(m_len));
        m_out.append(m_block, m_len);
        m_len = 0;
    }
    QByteArray &m_out;
    uint32_t m_acc = 0;
    int m_n = 0;
    char m_block[255];
    int m_len = 0;
};

// Appends the image data (LZW minimum code size, the data sub-blocks, the zero terminator) for `pixels`.
void lzwEncode(const std::vector<uint8_t> &pixels, int minCodeSize, QByteArray &out)
{
    out.append(char(minCodeSize));
    BitWriter bits(out);
    const int clearCode = 1 << minCodeSize, eoiCode = clearCode + 1;

    constexpr int kTable = 1 << 13; // open addressing, keys (prefix << 8 | byte)
    std::vector<int32_t> keys(kTable, -1);
    std::vector<uint16_t> values(kTable, 0);
    auto resetTable = [&]() { std::fill(keys.begin(), keys.end(), -1); };

    int next = eoiCode + 1, size = minCodeSize + 1;
    bits.put(clearCode, size);
    if (pixels.empty()) {
        bits.put(eoiCode, size);
        bits.finish();
        out.append(char(0));
        return;
    }
    int prefix = pixels[0];
    for (size_t i = 1; i < pixels.size(); ++i) {
        const int c = pixels[i];
        const int32_t key = (prefix << 8) | c;
        uint32_t h = (uint32_t(key) * 2654435761u) >> 19; // 13 bits
        bool found = false;
        while (keys[h] != -1) {
            if (keys[h] == key) { found = true; break; }
            h = (h + 1) & (kTable - 1);
        }
        if (found) {
            prefix = values[h];
            continue;
        }
        bits.put(prefix, size);
        keys[h] = key;
        values[h] = uint16_t(next);
        if (next == (1 << size) && size < 12)
            ++size;
        ++next;
        if (next == 4096) { // the table is full: start over
            bits.put(clearCode, size);
            resetTable();
            next = eoiCode + 1;
            size = minCodeSize + 1;
        }
        prefix = c;
    }
    bits.put(prefix, size);
    // the decoder adds one more entry after reading the last code, which may widen the code that follows
    if (next == (1 << size) && size < 12)
        ++size;
    bits.put(eoiCode, size);
    bits.finish();
    out.append(char(0));
}

// ---- the writer ---------------------------------------------------------------------------------------------------

void put16(QByteArray &b, int v)
{
    b.append(char(v & 255));
    b.append(char((v >> 8) & 255));
}

int bitsFor(int colors) // the table holds 2^n entries, n >= 1
{
    int n = 1;
    while ((1 << n) < colors)
        ++n;
    return n;
}

bool sameColor(const uchar *a, const uchar *b) { return std::memcmp(a, b, 4) == 0; }

struct Indexed {
    int x = 0, y = 0, w = 0, h = 0;
    std::vector<uint8_t> pixels; // palette indices, row by row, for the rectangle
};

} // namespace

std::vector<unsigned> quantizePalette(const std::vector<QImage> &images, int colors)
{
    Histogram h;
    for (const QImage &img : images) {
        const QImage rgba = img.convertToFormat(QImage::Format_RGBA8888);
        for (int y = 0; y < rgba.height(); ++y) {
            const uchar *p = rgba.constScanLine(y);
            for (int x = 0; x < rgba.width(); ++x, p += 4)
                if (p[3] >= 128)
                    h.add(p[0], p[1], p[2]);
        }
    }
    std::vector<unsigned> palette = medianCut(h, std::clamp(colors, 2, 256));
    refinePalette(h, palette, 4);
    return palette;
}

bool writeGif(FrameSource &frames, const GifOptions &options, QIODevice &out, const ProgressFn &progress, QString *error)
{
    const int total = frames.count();
    const QSize size = frames.size();
    auto fail = [&](const QString &why) {
        if (error)
            *error = why;
        return false;
    };
    if (total < 1 || size.width() < 1 || size.height() < 1)
        return fail(QStringLiteral("No hay fotogramas para guardar."));
    if (size.width() > 65535 || size.height() > 65535)
        return fail(QStringLiteral("La imagen es demasiado grande para un GIF."));

    const int maxColors = std::clamp(options.colors, 2, 256);
    // one palette slot is kept for "transparent", always at index 0
    const int usable = maxColors - 1;

    // ---- pass 1: look at every frame - whether anything is see-through, and (one shared palette) which colours there are
    std::vector<unsigned> globalPalette;
    Nearest globalNearest;
    bool anyTranslucent = false;
    {
        Histogram h;
        for (int i = 0; i < total; ++i) {
            int delay = 0;
            const QImage f = frames.frame(i, &delay).convertToFormat(QImage::Format_RGBA8888);
            if (f.size() != size)
                return fail(QStringLiteral("Los fotogramas no tienen todos el mismo tamaño."));
            for (int y = 0; y < f.height() && !anyTranslucent; ++y) {
                const uchar *p = f.constScanLine(y);
                for (int x = 0; x < f.width(); ++x)
                    if (p[x * 4 + 3] < 128) { anyTranslucent = true; break; }
            }
            if (!options.localPalettes) {
                // a sample of every frame (a sparser one for big frames)
                const int step = std::max(1, int(std::sqrt(double(f.width()) * f.height() / 90000.0)));
                for (int y = 0; y < f.height(); y += step) {
                    const uchar *p = f.constScanLine(y);
                    for (int x = 0; x < f.width(); x += step)
                        if (p[x * 4 + 3] >= 128)
                            h.add(p[x * 4], p[x * 4 + 1], p[x * 4 + 2]);
                }
            }
            if (progress && !progress(i, total * 2))
                return false;
        }
        if (!options.localPalettes) {
            globalPalette = medianCut(h, usable);
            refinePalette(h, globalPalette, 4);
            globalNearest.build(globalPalette, 1, &h);
        }
    }

    // ---- header
    QByteArray head;
    head.append("GIF89a");
    put16(head, size.width());
    put16(head, size.height());
    int gctBits = 0;
    if (!options.localPalettes) {
        gctBits = bitsFor(int(globalPalette.size()) + 1);
        head.append(char(0x80 | (7 << 4) | (gctBits - 1))); // global table, 8 bits of colour resolution, its size
    } else {
        head.append(char(0x70));
    }
    head.append(char(0)); // background colour index
    head.append(char(0)); // pixel aspect ratio
    if (!options.localPalettes) {
        // index 0 is "transparent": its colour is never shown
        head.append(char(0)); head.append(char(0)); head.append(char(0));
        for (unsigned c : globalPalette) {
            head.append(char((c >> 16) & 255)); head.append(char((c >> 8) & 255)); head.append(char(c & 255));
        }
        for (int i = int(globalPalette.size()) + 1; i < (1 << gctBits); ++i) {
            head.append(char(0)); head.append(char(0)); head.append(char(0));
        }
    }
    if (options.loops != 1) { // NETSCAPE2.0: how many times to play (0 = for ever)
        head.append("\x21\xFF\x0B", 3);
        head.append("NETSCAPE2.0");
        head.append("\x03\x01", 2);
        put16(head, options.loops <= 1 ? 0 : options.loops - 1);
        head.append(char(0));
    }
    if (out.write(head) != head.size())
        return fail(QStringLiteral("No se pudo escribir el archivo."));

    // ---- pass 2: the frames
    QImage previous;
    for (int i = 0; i < total; ++i) {
        int delayMs = 100;
        const QImage frame = frames.frame(i, &delayMs).convertToFormat(QImage::Format_RGBA8888);
        if (frame.size() != size)
            return fail(QStringLiteral("Los fotogramas no tienen todos el mismo tamaño."));

        // With anything see-through in the animation every frame is drawn on its own over a cleared
        // background; otherwise each one only adds what changed to the one before.
        const bool translucent = anyTranslucent;
        const bool diff = options.optimize && !translucent && !previous.isNull();

        // the part that changed
        int x0 = 0, y0 = 0, x1 = size.width() - 1, y1 = size.height() - 1;
        if (diff) {
            x0 = size.width(); y0 = size.height(); x1 = -1; y1 = -1;
            for (int y = 0; y < size.height(); ++y) {
                const uchar *a = frame.constScanLine(y), *b = previous.constScanLine(y);
                for (int x = 0; x < size.width(); ++x)
                    if (!sameColor(a + x * 4, b + x * 4)) {
                        x0 = std::min(x0, x); x1 = std::max(x1, x);
                        y0 = std::min(y0, y); y1 = std::max(y1, y);
                    }
            }
            if (x1 < 0) { x0 = y0 = 0; x1 = y1 = 0; } // nothing changed: a single transparent pixel
        }
        const int w = x1 - x0 + 1, h = y1 - y0 + 1;

        // palette for this frame
        std::vector<unsigned> palette;
        Nearest localNearest;
        const Nearest *nearest = &globalNearest;
        if (options.localPalettes) {
            Histogram hist;
            for (int y = y0; y <= y1; ++y) {
                const uchar *p = frame.constScanLine(y);
                const uchar *q = diff ? previous.constScanLine(y) : nullptr;
                for (int x = x0; x <= x1; ++x) {
                    if (p[x * 4 + 3] < 128 || (diff && sameColor(p + x * 4, q + x * 4)))
                        continue;
                    hist.add(p[x * 4], p[x * 4 + 1], p[x * 4 + 2]);
                }
            }
            palette = medianCut(hist, usable);
            refinePalette(hist, palette, 2);
            localNearest.build(palette, 1, &hist);
            nearest = &localNearest;
        } else {
            palette = globalPalette;
        }

        // index the rectangle (index 0 = transparent), with error diffusion when asked
        std::vector<uint8_t> pixels(size_t(w) * size_t(h), 0);
        std::vector<float> errCur(size_t(w + 2) * 3, 0.f), errNext(size_t(w + 2) * 3, 0.f);
        bool usesTransparency = false;
        for (int y = 0; y < h; ++y) {
            const uchar *p = frame.constScanLine(y0 + y) + size_t(x0) * 4;
            const uchar *q = diff ? previous.constScanLine(y0 + y) + size_t(x0) * 4 : nullptr;
            const bool leftToRight = (y & 1) == 0 || !options.dither;
            std::fill(errNext.begin(), errNext.end(), 0.f);
            for (int k = 0; k < w; ++k) {
                const int x = leftToRight ? k : w - 1 - k;
                const uchar *px = p + x * 4;
                uint8_t &outIdx = pixels[size_t(y) * size_t(w) + size_t(x)];
                if (px[3] < 128 || (diff && sameColor(px, q + x * 4))) {
                    outIdx = 0;
                    usesTransparency = true;
                    continue;
                }
                float r = px[0], g = px[1], b = px[2];
                if (options.dither) {
                    r += errCur[size_t(x + 1) * 3];
                    g += errCur[size_t(x + 1) * 3 + 1];
                    b += errCur[size_t(x + 1) * 3 + 2];
                }
                const int ri = std::clamp(int(r + 0.5f), 0, 255), gi = std::clamp(int(g + 0.5f), 0, 255), bi = std::clamp(int(b + 0.5f), 0, 255);
                const uint8_t idx = nearest->lut[size_t(binOf(ri, gi, bi))];
                outIdx = idx;
                if (options.dither && size_t(idx) - 1 < palette.size()) {
                    const unsigned pc = palette[size_t(idx) - 1];
                    // the error is capped: where the palette has nothing near a colour it would otherwise grow from pixel
                    // to pixel and the picture fills with speckles of unrelated colours
                    constexpr float kMaxError = 24.f;
                    const float er = std::clamp(r - float((pc >> 16) & 255), -kMaxError, kMaxError);
                    const float eg = std::clamp(g - float((pc >> 8) & 255), -kMaxError, kMaxError);
                    const float eb = std::clamp(b - float(pc & 255), -kMaxError, kMaxError);
                    const int dir = leftToRight ? 1 : -1;
                    auto spread = [&](std::vector<float> &buf, int xx, float f) {
                        if (xx < 0 || xx >= w)
                            return;
                        buf[size_t(xx + 1) * 3] += er * f;
                        buf[size_t(xx + 1) * 3 + 1] += eg * f;
                        buf[size_t(xx + 1) * 3 + 2] += eb * f;
                    };
                    spread(errCur, x + dir, 7.f / 16.f);
                    spread(errNext, x - dir, 3.f / 16.f);
                    spread(errNext, x, 5.f / 16.f);
                    spread(errNext, x + dir, 1.f / 16.f);
                }
            }
            std::swap(errCur, errNext);
        }

        // a graphic control extension, the image descriptor, the colours and the data
        QByteArray block;
        const int delayCs = std::clamp(int(std::lround(delayMs / 10.0)), 2, 65535);
        const bool transparentFlag = usesTransparency || translucent || diff;
        const int disposal = translucent ? 2 : 1; // see through to the background / leave it as it is
        block.append("\x21\xF9\x04", 3);
        block.append(char((disposal << 2) | (transparentFlag ? 1 : 0)));
        put16(block, delayCs);
        block.append(char(0)); // the transparent colour is index 0
        block.append(char(0));
        block.append(char(0x2C));
        put16(block, x0);
        put16(block, y0);
        put16(block, w);
        put16(block, h);
        int dataBits;
        if (options.localPalettes) {
            const int lctBits = bitsFor(int(palette.size()) + 1);
            block.append(char(0x80 | (lctBits - 1)));
            block.append(char(0)); block.append(char(0)); block.append(char(0));
            for (unsigned c : palette) {
                block.append(char((c >> 16) & 255)); block.append(char((c >> 8) & 255)); block.append(char(c & 255));
            }
            for (int k = int(palette.size()) + 1; k < (1 << lctBits); ++k) {
                block.append(char(0)); block.append(char(0)); block.append(char(0));
            }
            dataBits = lctBits;
        } else {
            block.append(char(0));
            dataBits = gctBits;
        }
        lzwEncode(pixels, std::max(2, dataBits), block);
        if (out.write(block) != block.size())
            return fail(QStringLiteral("No se pudo escribir el archivo."));

        previous = frame;
        if (progress && !progress(total + i + 1, total * 2))
            return false;
    }
    const char trailer = 0x3B;
    if (out.write(&trailer, 1) != 1)
        return fail(QStringLiteral("No se pudo escribir el archivo."));
    return true;
}

} // namespace core::anim
