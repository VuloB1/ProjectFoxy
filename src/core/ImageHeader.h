#pragma once

#include <QByteArray>
#include <QSize>

namespace core {

// The pixel size of a PNG, JPEG or WebP read from the first bytes of the file, without decoding anything; an invalid
// size for anything else (or when the bytes do not reach the size). A comic reader needs the shape of every page to
// lay out double pages, and decoding hundreds of pages for that would be slow.
// shortcut: other formats (AVIF, TIFF, GIF...) report no size and are laid out as ordinary portrait pages; add their
// headers here if comics in them turn up.
inline QSize imageSizeFromHeader(const QByteArray &d)
{
    const auto b = [&d](qsizetype i) { return i < d.size() ? int(quint8(d[i])) : 0; };
    // PNG: signature, then the IHDR chunk with the size big-endian at byte 16.
    if (d.size() >= 24 && d.startsWith("\x89PNG\r\n\x1a\n"))
        return {b(16) << 24 | b(17) << 16 | b(18) << 8 | b(19), b(20) << 24 | b(21) << 16 | b(22) << 8 | b(23)};
    // JPEG: walk the segments until a "start of frame".
    if (d.size() >= 4 && b(0) == 0xFF && b(1) == 0xD8) {
        qsizetype p = 2;
        while (p + 9 < d.size()) {
            if (b(p) != 0xFF) {
                ++p;
                continue;
            }
            const int marker = b(p + 1);
            if (marker == 0xFF) { // fill byte
                ++p;
                continue;
            }
            if (marker == 0x00 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD8)) { // no length
                p += 2;
                continue;
            }
            if (marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC)
                return {b(p + 7) << 8 | b(p + 8), b(p + 5) << 8 | b(p + 6)};
            p += 2 + (b(p + 2) << 8 | b(p + 3));
        }
        return {};
    }
    // WebP: RIFF container; lossy ("VP8 "), lossless ("VP8L") or extended ("VP8X").
    if (d.size() >= 30 && d.startsWith("RIFF") && d.mid(8, 4) == "WEBP") {
        const QByteArray kind = d.mid(12, 4);
        if (kind == "VP8 ")
            return {(b(26) | b(27) << 8) & 0x3FFF, (b(28) | b(29) << 8) & 0x3FFF};
        if (kind == "VP8L")
            return {1 + (b(21) | (b(22) & 0x3F) << 8), 1 + ((b(22) >> 6) | b(23) << 2 | (b(24) & 0x0F) << 10)};
        if (kind == "VP8X")
            return {1 + (b(24) | b(25) << 8 | b(26) << 16), 1 + (b(27) | b(28) << 8 | b(29) << 16)};
    }
    return {};
}

} // namespace core
