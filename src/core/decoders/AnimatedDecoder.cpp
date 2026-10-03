#include "AnimatedDecoder.h"

#include <QFile>
#include <QImageReader>
#include <QPainter>
#include <QtEndian>
#include <array>
#include <cstring>

namespace core {

namespace {

// --- PNG chunk plumbing -----------------------------------------------
// Just enough of the PNG container format to walk chunks and rebuild new
// ones - no pixel-level PNG knowledge here, that's all left to Qt's own
// "png" codec (QImage::fromData) once a per-frame buffer is reassembled.

constexpr char kPngSignature[8] = { '\x89', 'P', 'N', 'G', '\r', '\n', '\x1a', '\n' };

struct PngChunk {
    QByteArray type;
    QByteArray data;
};

QVector<PngChunk> readPngChunks(const QByteArray &bytes)
{
    QVector<PngChunk> chunks;
    if (bytes.size() < 8 || std::memcmp(bytes.constData(), kPngSignature, 8) != 0)
        return chunks;

    int pos = 8;
    while (pos + 8 <= bytes.size()) {
        const quint32 length = qFromBigEndian<quint32>(bytes.constData() + pos);
        const QByteArray type = bytes.mid(pos + 4, 4);
        const qint64 dataStart = qint64(pos) + 8;
        if (dataStart + qint64(length) + 4 > bytes.size())
            break; // truncated/corrupt file - stop with whatever was parsed so far
        PngChunk chunk;
        chunk.type = type;
        chunk.data = bytes.mid(int(dataStart), int(length));
        chunks.push_back(chunk);
        pos = int(dataStart + qint64(length) + 4);
        if (type == "IEND")
            break;
    }
    return chunks;
}

// PNG's CRC-32 (ISO 3309 / ITU-T V.42, the same variant zlib/libpng use) -
// hand-rolled instead of pulling in a zlib dependency this project doesn't
// otherwise link against directly. libpng validates every chunk's CRC on
// read, so a reconstructed frame's IHDR/IDAT/IEND need a genuinely correct
// one or QImage::fromData silently fails the whole frame.
const std::array<quint32, 256> &crcTable()
{
    static const std::array<quint32, 256> table = [] {
        std::array<quint32, 256> t{};
        for (quint32 n = 0; n < 256; ++n) {
            quint32 c = n;
            for (int k = 0; k < 8; ++k)
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            t[n] = c;
        }
        return t;
    }();
    return table;
}

quint32 pngCrc(const QByteArray &type, const QByteArray &data)
{
    quint32 c = 0xFFFFFFFFu;
    const auto &table = crcTable();
    auto feed = [&](const QByteArray &bytes) {
        for (unsigned char byte : bytes)
            c = table[(c ^ byte) & 0xFFu] ^ (c >> 8);
    };
    feed(type);
    feed(data);
    return c ^ 0xFFFFFFFFu;
}

void appendChunk(QByteArray &out, const char *type, const QByteArray &data)
{
    const QByteArray typeBytes(type, 4);
    quint32 lenBE = qToBigEndian<quint32>(quint32(data.size()));
    out.append(reinterpret_cast<const char *>(&lenBE), 4);
    out.append(typeBytes);
    out.append(data);
    quint32 crcBE = qToBigEndian<quint32>(pngCrc(typeBytes, data));
    out.append(reinterpret_cast<const char *>(&crcBE), 4);
}

// --- APNG-specific parsing ----------------------------------------------

constexpr quint8 kDisposeOpNone = 0;
constexpr quint8 kDisposeOpBackground = 1;
constexpr quint8 kDisposeOpPrevious = 2;
constexpr quint8 kBlendOpSource = 0;

struct ApngFrame {
    quint32 width = 0, height = 0, xOffset = 0, yOffset = 0;
    quint16 delayNum = 0, delayDen = 0;
    quint8 disposeOp = kDisposeOpNone;
    quint8 blendOp = kBlendOpSource;
    QByteArray pixelData; // concatenated IDAT/fdAT payloads for this frame alone
};

int apngDelayMs(quint16 num, quint16 den)
{
    if (den == 0)
        den = 100; // spec: denominator 0 means "num is in 1/100s", same as GIF
    const double seconds = double(num) / double(den);
    const int ms = int(seconds * 1000.0 + 0.5);
    return ms > 0 ? ms : 100;
}

bool pngHasAnimation(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    // acTL must appear before the first IDAT (spec requirement) and in
    // every encoder observed in practice sits right after IHDR, so a small
    // head read is always enough - never touch the rest of a possibly huge
    // file just to answer "is this animated?".
    const QByteArray head = f.read(65536);
    for (const PngChunk &chunk : readPngChunks(head)) {
        if (chunk.type == "acTL")
            return true;
        if (chunk.type == "IDAT")
            return false; // acTL didn't show up before the first IDAT - not APNG
    }
    return false;
}

// Rebuilds one frame's sub-image (at its own fcTL width/height, NOT the
// full canvas) as a standalone PNG buffer and decodes it via Qt's own PNG
// codec - reusing a well-tested decoder instead of reimplementing deflate/
// PNG filter reconstruction by hand.
QImage decodeApngFrameImage(const QByteArray &originalIhdrData, const ApngFrame &frame,
                            const QVector<PngChunk> &ancillary)
{
    QByteArray ihdr;
    quint32 w = qToBigEndian<quint32>(frame.width);
    quint32 h = qToBigEndian<quint32>(frame.height);
    ihdr.append(reinterpret_cast<const char *>(&w), 4);
    ihdr.append(reinterpret_cast<const char *>(&h), 4);
    ihdr.append(originalIhdrData.mid(8, 5)); // bitDepth/colorType/compression/filter/interlace, unchanged per frame

    QByteArray buffer;
    buffer.append(kPngSignature, 8);
    appendChunk(buffer, "IHDR", ihdr);
    for (const PngChunk &chunk : ancillary)
        appendChunk(buffer, chunk.type.constData(), chunk.data);
    appendChunk(buffer, "IDAT", frame.pixelData);
    appendChunk(buffer, "IEND", QByteArray());

    QImage image = QImage::fromData(buffer, "PNG");
    return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

bool decodeApngFrames(const QString &filePath, bool wantAllFrames,
                      QVector<QImage> &frames, QVector<int> &delays, QString &error)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("No se pudo abrir %1").arg(filePath);
        return false;
    }
    const QByteArray bytes = f.readAll();
    const QVector<PngChunk> chunks = readPngChunks(bytes);

    QByteArray ihdrData;
    QVector<PngChunk> ancillary; // PLTE/tRNS/color-management chunks, copied into every reconstructed frame
    QVector<ApngFrame> rawFrames;
    bool haveFctl = false;

    for (const PngChunk &chunk : chunks) {
        if (chunk.type == "IHDR") {
            ihdrData = chunk.data;
        } else if (chunk.type == "PLTE" || chunk.type == "tRNS" || chunk.type == "gAMA"
                   || chunk.type == "cHRM" || chunk.type == "sRGB" || chunk.type == "iCCP") {
            ancillary.push_back(chunk);
        } else if (chunk.type == "fcTL") {
            if (chunk.data.size() < 26)
                continue;
            const char *d = chunk.data.constData();
            ApngFrame frame;
            frame.width = qFromBigEndian<quint32>(d + 4);
            frame.height = qFromBigEndian<quint32>(d + 8);
            frame.xOffset = qFromBigEndian<quint32>(d + 12);
            frame.yOffset = qFromBigEndian<quint32>(d + 16);
            frame.delayNum = qFromBigEndian<quint16>(d + 20);
            frame.delayDen = qFromBigEndian<quint16>(d + 22);
            frame.disposeOp = quint8(d[24]);
            frame.blendOp = quint8(d[25]);
            // A SECOND fcTL marks frame 1's start, meaning frame 0's own
            // IDAT/fdAT chunks have all already been collected by now - only
            // then is it safe to stop for a preview decode. Breaking on the
            // FIRST fcTL instead (before its data chunk is even reached)
            // left frame 0 with empty pixel data and libpng correctly
            // refusing to decode it ("Not enough image data").
            if (!wantAllFrames && haveFctl)
                break;
            rawFrames.push_back(frame);
            haveFctl = true;
        } else if (chunk.type == "IDAT") {
            if (haveFctl && !rawFrames.isEmpty())
                rawFrames.back().pixelData += chunk.data; // default image doubles as frame 0
            // else: pre-animation "poster" IDAT, not part of the animation - skip
        } else if (chunk.type == "fdAT") {
            if (chunk.data.size() > 4 && !rawFrames.isEmpty())
                rawFrames.back().pixelData += chunk.data.mid(4); // strip the 4-byte sequence number
        }
    }

    if (ihdrData.size() < 13 || rawFrames.isEmpty()) {
        error = QStringLiteral("APNG sin fotogramas válidos: %1").arg(filePath);
        return false;
    }

    const quint32 canvasWidth = qFromBigEndian<quint32>(ihdrData.constData());
    const quint32 canvasHeight = qFromBigEndian<quint32>(ihdrData.constData() + 4);
    if (canvasWidth == 0 || canvasHeight == 0 || canvasWidth > 20000 || canvasHeight > 20000) {
        error = QStringLiteral("Dimensiones de APNG inválidas: %1").arg(filePath);
        return false;
    }

    QImage canvas(int(canvasWidth), int(canvasHeight), QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);
    QImage canvasBeforePrevBlend;
    quint8 prevDisposeOp = kDisposeOpNone;
    QRect prevRect;

    for (int i = 0; i < rawFrames.size(); ++i) {
        const ApngFrame &frame = rawFrames[i];
        const QRect rect(int(frame.xOffset), int(frame.yOffset), int(frame.width), int(frame.height));

        if (i > 0) {
            if (prevDisposeOp == kDisposeOpBackground) {
                QPainter clear(&canvas);
                clear.setCompositionMode(QPainter::CompositionMode_Source);
                clear.fillRect(prevRect, Qt::transparent);
            } else if (prevDisposeOp == kDisposeOpPrevious && !canvasBeforePrevBlend.isNull()) {
                canvas = canvasBeforePrevBlend;
            }
        }

        if (frame.disposeOp == kDisposeOpPrevious)
            canvasBeforePrevBlend = canvas.copy();

        const QImage sub = decodeApngFrameImage(ihdrData, frame, ancillary);
        if (!sub.isNull()) {
            QPainter paint(&canvas);
            paint.setCompositionMode(i == 0 || frame.blendOp == kBlendOpSource
                                          ? QPainter::CompositionMode_Source
                                          : QPainter::CompositionMode_SourceOver);
            paint.drawImage(rect.topLeft(), sub);
        }

        frames.push_back(canvas.convertToFormat(QImage::Format_RGBA8888));
        delays.push_back(apngDelayMs(frame.delayNum, frame.delayDen));

        prevDisposeOp = frame.disposeOp;
        prevRect = rect;

        if (!wantAllFrames)
            break;
    }

    return !frames.isEmpty();
}

bool decodeGifFrames(const QString &filePath, bool wantAllFrames,
                     QVector<QImage> &frames, QVector<int> &delays, QString &error)
{
    QImageReader reader(filePath, QByteArrayLiteral("gif"));
    QImage frame = reader.read();
    if (frame.isNull()) {
        error = reader.errorString();
        return false;
    }
    while (!frame.isNull()) {
        frames.push_back(frame.convertToFormat(QImage::Format_RGBA8888));
        const int delay = reader.nextImageDelay();
        delays.push_back(delay > 0 ? delay : 100);
        if (!wantAllFrames)
            break;
        frame = reader.read();
    }
    return true;
}

} // namespace

bool AnimatedDecoder::canDecode(const QString &filePath) const
{
    const QString ext = filePath.section('.', -1).toLower();
    if (ext == QLatin1String("gif")) {
        QImageReader reader(filePath, QByteArrayLiteral("gif"));
        return reader.imageCount() > 1;
    }
    if (ext == QLatin1String("png"))
        return pngHasAnimation(filePath);
    return false;
}

DecodeResult AnimatedDecoder::decode(const QString &filePath, QSize maxSize)
{
    DecodeResult result;
    const bool wantAllFrames = !maxSize.isValid();
    const QString ext = filePath.section('.', -1).toLower();

    QVector<QImage> frames;
    QVector<int> delays;
    QString error;
    const bool decoded = (ext == QLatin1String("gif"))
        ? decodeGifFrames(filePath, wantAllFrames, frames, delays, error)
        : decodeApngFrames(filePath, wantAllFrames, frames, delays, error);

    if (!decoded || frames.isEmpty()) {
        result.error = error.isEmpty() ? QStringLiteral("No se pudo decodificar %1").arg(filePath) : error;
        return result;
    }

    result.image = frames.first();
    result.sourceSize = frames.first().size();
    result.ok = true;
    if (wantAllFrames && frames.size() > 1) {
        result.frames = frames;
        result.frameDelaysMs = delays;
    }
    return result;
}

QStringList AnimatedDecoder::supportedExtensions() const
{
    return { "gif", "png" };
}

} // namespace core
