// Developer tool (not a test): how long each effect takes on a photo-sized image, wall time
// and CPU time summed over all threads. Usage: bench_effects [longSide=1600] [effectId...]
#include "edit/Effects.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QThread>
#include <QPainter>
#include <cstdio>
#include <windows.h>

static double cpuSeconds()
{
    FILETIME c, e, k, u;
    GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u);
    auto t = [](FILETIME f) { return double((quint64(f.dwHighDateTime) << 32) | f.dwLowDateTime) * 1e-7; };
    return t(k) + t(u);
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
            img = loaded.scaledToWidth(side, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGBA8888);
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
        const auto values = core::edit::defaultEffectValues(spec);
        QElapsedTimer t;
        t.start();
        const double c0 = cpuSeconds();
        const QImage out = core::edit::applyEffect(img, spec.id, values, 1.0);
        if (!saveDir.isEmpty())
            out.save(saveDir + "/" + spec.id + ".png");
        const double wall = t.nsecsElapsed() / 1e6, cpu = (cpuSeconds() - c0) * 1000;
        std::printf("%-12s %9.0f %9.0f   %08x\n", qPrintable(spec.id), wall, cpu,
                    qHash(QByteArray::fromRawData(reinterpret_cast<const char *>(out.constBits()), out.sizeInBytes()), 0));
    }
}
