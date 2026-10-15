// Developer tool (not a test): where the time of opening a picture goes, per stage and per file.
// Usage: bench_load file...
#include "DecoderRegistry.h"
#include "MetadataReader.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <cstdio>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    for (int i = 1; i < argc; ++i) {
        const QString path = QString::fromLocal8Bit(argv[i]);
        auto decoder = core::DecoderRegistry::instance().decoderFor(path);
        if (!decoder) {
            std::printf("%s: no decoder\n", argv[i]);
            continue;
        }
        for (int run = 0; run < 3; ++run) {
            QElapsedTimer t;
            t.start();
            const auto r = decoder->decode(path, QSize());
            const double decode = t.nsecsElapsed() / 1e6;
            t.restart();
            core::MetadataReader::read(path);
            const double meta = t.nsecsElapsed() / 1e6;
            std::printf("%-40s run%d %5dx%-5d size %6.1f MB  decode %7.1f ms  metadata %6.1f ms\n",
                        qPrintable(QFileInfo(path).fileName().left(40)), run, r.image.width(), r.image.height(),
                        QFileInfo(path).size() / 1048576.0, decode, meta);
        }
    }
    return 0;
}
