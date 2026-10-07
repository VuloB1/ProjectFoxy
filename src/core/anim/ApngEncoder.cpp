// An APNG writer. Qt's own PNG encoder does the pixels (deflate and filtering); this only wraps what it
// produces into the animation chunks (acTL, fcTL, fdAT). Each frame after the first stores only the rectangle
// that changed, replacing what was there (blend "source", dispose "none"), which is exactly right for the
// full-colour frames the studio produces.

#include "Anim.h"

#include <QBuffer>
#include <QByteArray>
#include <QImageWriter>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include "Translate.h"

namespace core::anim {

namespace {

uint32_t crc32(const uint8_t *data, size_t len, uint32_t crc = 0)
{
    static const std::array<uint32_t, 256> table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t n = 0; n < 256; ++n) {
            uint32_t c = n;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[n] = c;
        }
        return t;
    }();
    crc = ~crc;
    for (size_t i = 0; i < len; ++i)
        crc = table[(crc ^ data[i]) & 255] ^ (crc >> 8);
    return ~crc;
}

void put32(QByteArray &b, uint32_t v)
{
    b.append(char((v >> 24) & 255));
    b.append(char((v >> 16) & 255));
    b.append(char((v >> 8) & 255));
    b.append(char(v & 255));
}

void put16(QByteArray &b, uint32_t v)
{
    b.append(char((v >> 8) & 255));
    b.append(char(v & 255));
}

void chunk(QByteArray &out, const char *type, const QByteArray &data)
{
    put32(out, uint32_t(data.size()));
    QByteArray body(type, 4);
    body.append(data);
    out.append(body);
    put32(out, crc32(reinterpret_cast<const uint8_t *>(body.constData()), size_t(body.size())));
}

struct PngParts {
    QByteArray ihdr; // the 13 bytes
    QByteArray idat; // the compressed pixel data, all IDAT chunks joined
    bool ok = false;
};

PngParts splitPng(const QByteArray &png)
{
    PngParts parts;
    if (png.size() < 8 || std::memcmp(png.constData(), "\x89PNG\r\n\x1a\n", 8) != 0)
        return parts;
    int pos = 8;
    while (pos + 8 <= png.size()) {
        const uint32_t len = (uint32_t(uint8_t(png[pos])) << 24) | (uint32_t(uint8_t(png[pos + 1])) << 16)
                             | (uint32_t(uint8_t(png[pos + 2])) << 8) | uint32_t(uint8_t(png[pos + 3]));
        const QByteArray type = png.mid(pos + 4, 4);
        if (pos + 8 + int(len) + 4 > png.size())
            return parts;
        if (type == "IHDR")
            parts.ihdr = png.mid(pos + 8, int(len));
        else if (type == "IDAT")
            parts.idat.append(png.constData() + pos + 8, int(len));
        else if (type == "IEND")
            break;
        pos += 8 + int(len) + 4;
    }
    parts.ok = parts.ihdr.size() == 13 && !parts.idat.isEmpty();
    return parts;
}

bool encodePng(const QImage &img, QByteArray &png)
{
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    QImageWriter writer(&buffer, "png");
    return writer.write(img);
}

} // namespace

bool writeApng(FrameSource &frames, const ApngOptions &options, QIODevice &out, const ProgressFn &progress, QString *error)
{
    const int total = frames.count();
    const QSize size = frames.size();
    auto fail = [&](const QString &why) {
        if (error)
            *error = why;
        return false;
    };
    if (total < 1 || size.width() < 1 || size.height() < 1)
        return fail(core::tr("No hay fotogramas para guardar."));

    QByteArray file("\x89PNG\r\n\x1a\n", 8);
    QByteArray ihdr;
    uint32_t sequence = 0;
    QImage previous;

    for (int i = 0; i < total; ++i) {
        int delayMs = 100;
        QImage frame = frames.frame(i, &delayMs).convertToFormat(QImage::Format_RGBA8888);
        if (frame.size() != size)
            return fail(core::tr("Los fotogramas no tienen todos el mismo tamaño."));

        // the part that changed (the first frame is always the whole picture)
        int x0 = 0, y0 = 0, x1 = size.width() - 1, y1 = size.height() - 1;
        if (i > 0 && options.optimize) {
            x0 = size.width(); y0 = size.height(); x1 = -1; y1 = -1;
            for (int y = 0; y < size.height(); ++y) {
                const uchar *a = frame.constScanLine(y), *b = previous.constScanLine(y);
                for (int x = 0; x < size.width(); ++x)
                    if (std::memcmp(a + x * 4, b + x * 4, 4) != 0) {
                        x0 = std::min(x0, x); x1 = std::max(x1, x);
                        y0 = std::min(y0, y); y1 = std::max(y1, y);
                    }
            }
            if (x1 < 0) { x0 = y0 = 0; x1 = y1 = 0; }
        }
        const int w = x1 - x0 + 1, h = y1 - y0 + 1;
        const QImage part = (w == size.width() && h == size.height()) ? frame : frame.copy(x0, y0, w, h);

        QByteArray png;
        if (!encodePng(part, png))
            return fail(core::tr("No se pudo codificar un fotograma."));
        const PngParts parts = splitPng(png);
        if (!parts.ok)
            return fail(core::tr("No se pudo codificar un fotograma."));

        if (i == 0) {
            ihdr = parts.ihdr;
            chunk(file, "IHDR", ihdr);
            QByteArray actl;
            put32(actl, uint32_t(total));
            put32(actl, uint32_t(std::max(0, options.loops)));
            chunk(file, "acTL", actl);
        } else if (parts.ihdr.mid(8, 5) != ihdr.mid(8, 5)) {
            return fail(core::tr("Los fotogramas no tienen el mismo formato."));
        }

        QByteArray fctl;
        put32(fctl, sequence++);
        put32(fctl, uint32_t(w));
        put32(fctl, uint32_t(h));
        put32(fctl, uint32_t(x0));
        put32(fctl, uint32_t(y0));
        put16(fctl, uint32_t(std::clamp(delayMs, 1, 65535)));
        put16(fctl, 1000);
        fctl.append(char(0)); // dispose: none
        fctl.append(char(0)); // blend: source (the rectangle replaces what is there)
        chunk(file, "fcTL", fctl);

        if (i == 0) {
            chunk(file, "IDAT", parts.idat);
        } else {
            QByteArray fdat;
            put32(fdat, sequence++);
            fdat.append(parts.idat);
            chunk(file, "fdAT", fdat);
        }

        // write in pieces so a long animation does not pile up in memory
        if (out.write(file) != file.size())
            return fail(core::tr("No se pudo escribir el archivo."));
        file.clear();
        previous = frame;
        if (progress && !progress(i + 1, total))
            return false;
    }
    chunk(file, "IEND", QByteArray());
    if (out.write(file) != file.size())
        return fail(core::tr("No se pudo escribir el archivo."));
    return true;
}

} // namespace core::anim
