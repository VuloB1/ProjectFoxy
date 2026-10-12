// libFuzzer harness for core::AnimatedDecoder: the part of the program that reads GIF, APNG and animated WebP files,
// and the only one with a hand-written parser (the APNG chunk walker).
//
// One executable per format (FUZZ_EXT = "gif" | "png" | "webp"): the decoder picks the reader from the extension, so
// the bytes the fuzzer produces are written to a file with that extension and decoded the three ways the program does
// (canDecode, a full decode, a thumbnail-size decode). The limits are set low so a hostile file costs little to try.
//
// Build and run: see tests/fuzz/README.md.
#include "decoders/AnimatedDecoder.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <cstdint>

#ifndef FUZZ_EXT
#error "FUZZ_EXT must be defined (gif, png or webp)"
#endif

namespace {

QTemporaryDir *g_dir = nullptr;
QString g_path;

void quiet(QtMsgType, const QMessageLogContext &, const QString &) {} // Qt and the codecs warn about every bad file

} // namespace

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
    static int argc = 1;
    static char name[] = "fuzz";
    static char *argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    qInstallMessageHandler(quiet);

    static QTemporaryDir dir;
    g_dir = &dir;
    g_path = dir.filePath(QStringLiteral("input." FUZZ_EXT));

    core::AnimatedDecoder::Limits limits;
    limits.maxInputBytes = 4 * 1024 * 1024;
    limits.maxCanvasPixels = 4 * 1000 * 1000;
    limits.maxAnimationBytes = 64 * 1024 * 1024;
    limits.maxFrames = 200;
    core::AnimatedDecoder::setLimits(limits);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    {
        QFile file(g_path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return 0;
        file.write(reinterpret_cast<const char *>(data), qint64(size));
    }
    core::AnimatedDecoder decoder;
    decoder.canDecode(g_path);
    decoder.decode(g_path);
    decoder.decode(g_path, QSize(64, 64));
    return 0;
}
