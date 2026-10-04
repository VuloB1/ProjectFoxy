// Developer tool (not a test): how long each effect takes on a photo-sized image, wall time
// and CPU time summed over all threads. Usage: bench_effects [longSide=1600] [effectId...]
#include "edit/Effects.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QImage>
#include <algorithm>
#include <QThread>
#include <QPainter>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>
#include <windows.h>

static double cpuSeconds()
{
    FILETIME c, e, k, u;
    GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u);
    auto t = [](FILETIME f) { return double((quint64(f.dwHighDateTime) << 32) | f.dwLowDateTime) * 1e-7; };
    return t(k) + t(u);
}

// PSNR of RGB between two same-size pictures (alpha ignored), in dB.
static double psnr(const QImage &a, const QImage &b)
{
    double sum = 0;
    for (int y = 0; y < a.height(); ++y) {
        const uchar *p = a.constScanLine(y), *q = b.constScanLine(y);
        for (int x = 0; x < a.width(); ++x)
            for (int c = 0; c < 3; ++c) {
                const double d = double(p[x * 4 + c]) - q[x * 4 + c];
                sum += d * d;
            }
    }
    const double mse = sum / (double(a.width()) * a.height() * 3);
    return mse <= 0 ? 99.0 : 10.0 * std::log10(255.0 * 255.0 / mse);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const int side = argc > 1 ? atoi(argv[1]) : 1600;
    QImage img(side, side * 2 / 3, QImage::Format_RGBA8888);
    for (int y = 0; y < img.height(); ++y) {
        uchar *p = img.scanLine(y);
        for (int x = 0; x < img.width(); ++x) {
            p[x * 4 + 0] = uchar((x * 255) / img.width() ^ (y & 31));
            p[x * 4 + 1] = uchar((y * 255) / img.height() + (x % 17));
            p[x * 4 + 2] = uchar(((x + y) * 7) & 255);
            p[x * 4 + 3] = 255;
        }
    }
    // BENCH_IMG=photo.jpg uses a real picture; BENCH_SAVE=folder writes every result as a PNG.
    const QString photo = qEnvironmentVariable("BENCH_IMG");
    if (!photo.isEmpty()) {
        QImage loaded(photo);
        if (!loaded.isNull())
            img = (side > 0 ? loaded.scaledToWidth(side, Qt::SmoothTransformation) : loaded).convertToFormat(QImage::Format_RGBA8888);
    }
    // BENCH_NOISE=sigma adds Gaussian noise (independent per channel) and prints how close each
    // result gets to the clean picture (PSNR, higher is better); BENCH_P="a,b,c" overrides the sliders.
    const QImage clean = img;
    const double noiseSigma = qEnvironmentVariable("BENCH_NOISE").toDouble();
    if (noiseSigma > 0) {
        // BENCH_CORR=n blurs the noise n times with [1 2 1]: blotchy, camera-like noise
        const int corr = qEnvironmentVariable("BENCH_CORR").toInt();
        const int w = img.width(), h = img.height();
        std::mt19937 rng(42);
        std::normal_distribution<double> gauss(0.0, 1.0);
        for (int c = 0; c < 3; ++c) {
            std::vector<float> n(size_t(w) * h), t(size_t(w) * h);
            for (auto &v : n)
                v = float(gauss(rng));
            for (int it = 0; it < corr; ++it) {
                for (int y = 0; y < h; ++y)
                    for (int x = 0; x < w; ++x)
                        t[size_t(y) * w + x] = 0.25f * n[size_t(y) * w + (std::max)(x - 1, 0)] + 0.5f * n[size_t(y) * w + x] + 0.25f * n[size_t(y) * w + (std::min)(x + 1, w - 1)];
                for (int y = 0; y < h; ++y)
                    for (int x = 0; x < w; ++x)
                        n[size_t(y) * w + x] = 0.25f * t[size_t((std::max)(y - 1, 0)) * w + x] + 0.5f * t[size_t(y) * w + x] + 0.25f * t[size_t((std::min)(y + 1, h - 1)) * w + x];
            }
            double sq = 0;
            for (float v : n)
                sq += double(v) * v;
            const double scale = noiseSigma / std::sqrt(sq / n.size());
            for (int y = 0; y < h; ++y) {
                uchar *p = img.scanLine(y);
                for (int x = 0; x < w; ++x)
                    p[x * 4 + c] = uchar(std::clamp(int(std::lround(p[x * 4 + c] + n[size_t(y) * w + x] * scale)), 0, 255));
            }
        }
        std::printf("noisy input: %.2f dB\n", psnr(img, clean));
        if (!qEnvironmentVariable("BENCH_SAVE").isEmpty()) {
            clean.save(qEnvironmentVariable("BENCH_SAVE") + "/_clean.png");
            img.save(qEnvironmentVariable("BENCH_SAVE") + "/_noisy.png");
        }
    }
    const QString saveDir = qEnvironmentVariable("BENCH_SAVE");
    std::printf("image %dx%d, %d threads\n", img.width(), img.height(), QThread::idealThreadCount());
    std::printf("%-12s %9s %9s\n", "effect", "wall ms", "cpu ms");
    for (const auto &spec : core::edit::allEffects()) {
        bool wanted = argc <= 2;
        for (int i = 2; i < argc; ++i)
            wanted |= spec.id == QLatin1String(argv[i]);
        if (!wanted)
            continue;
        auto values = core::edit::defaultEffectValues(spec);
        const QStringList over = qEnvironmentVariable("BENCH_P").split(',', Qt::SkipEmptyParts);
        for (int i = 0; i < over.size() && i < int(values.size()); ++i)
            values[i] = over[i].toDouble();
        if (qEnvironmentVariableIsSet("BENCH_MAX")) // worst case: every slider at its maximum
            for (size_t i = 0; i < spec.params.size(); ++i)
                values[i] = spec.params[i].max;
        QElapsedTimer t;
        t.start();
        const double c0 = cpuSeconds();
        const QImage out = core::edit::applyEffect(img, spec.id, values, 1.0);
        if (noiseSigma > 0)
            std::printf("  -> %.2f dB vs clean\n", psnr(out, clean));
        if (!saveDir.isEmpty())
            out.save(saveDir + "/" + spec.id + ".png");
        const double wall = t.nsecsElapsed() / 1e6, cpu = (cpuSeconds() - c0) * 1000;
        std::printf("%-12s %9.0f %9.0f   %08x\n", qPrintable(spec.id), wall, cpu,
                    qHash(QByteArray::fromRawData(reinterpret_cast<const char *>(out.constBits()), out.sizeInBytes()), 0));
    }
}
