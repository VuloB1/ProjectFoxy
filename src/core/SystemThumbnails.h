#pragma once

#include <QImage>
#include <QString>

namespace core {

// A thumbnail the operating system already has for this file, so the filmstrip does not have to
// decode the original just to draw a small square.
//
//   Windows: the Explorer thumbnail cache, asked through IShellItemImageFactory with
//            SIIGBF_INCACHEONLY (a miss comes back at once; the shell never generates anything here).
//   Linux:   the freedesktop thumbnail store (~/.cache/thumbnails/{normal,large,x-large,xx-large}),
//            where file managers keep PNGs named after the MD5 of the file's URI.
//
// Returns a null image when the system has nothing usable (or the platform has no such store); the
// caller then decodes the file itself. The result is never larger than `edgeLength` on its long
// side. Thread-safe.
QImage systemThumbnail(const QString &filePath, int edgeLength);

namespace detail {
// The freedesktop lookup on its own, with the store's root given (".../thumbnails"), so it can be
// exercised on any platform. Entries whose recorded modification time does not match the file's
// are ignored, as the specification requires.
QImage freedesktopThumbnail(const QString &storeRoot, const QString &filePath, int edgeLength);
// "<root>/<sizeDir>/<md5 of the file URI>.png"
QString freedesktopThumbnailPath(const QString &storeRoot, const QString &sizeDir, const QString &filePath);
} // namespace detail

} // namespace core
