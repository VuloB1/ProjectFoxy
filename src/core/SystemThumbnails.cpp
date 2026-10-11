#include "SystemThumbnails.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QUrl>
#include <algorithm>

#ifdef _WIN32
#  include <windows.h>
#  include <shobjidl.h>
#  include <shlobj.h>
#  include <shlwapi.h>
#endif

namespace core {

namespace {

QImage fitTo(QImage image, int edgeLength)
{
    if (image.isNull() || edgeLength <= 0)
        return image;
    if (qMax(image.width(), image.height()) > edgeLength)
        image = image.scaled(edgeLength, edgeLength, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return image;
}

} // namespace

namespace detail {

QString freedesktopThumbnailPath(const QString &storeRoot, const QString &sizeDir, const QString &filePath)
{
    const QByteArray uri = QUrl::fromLocalFile(QFileInfo(filePath).absoluteFilePath()).toEncoded();
    const QString name = QString::fromLatin1(QCryptographicHash::hash(uri, QCryptographicHash::Md5).toHex());
    return storeRoot + QLatin1Char('/') + sizeDir + QLatin1Char('/') + name + QStringLiteral(".png");
}

QImage freedesktopThumbnail(const QString &storeRoot, const QString &filePath, int edgeLength)
{
    const QFileInfo source(filePath);
    if (!source.isFile())
        return {};

    struct Tier { const char *dir; int side; };
    static const Tier tiers[] = {{"normal", 128}, {"large", 256}, {"x-large", 512}, {"xx-large", 1024}};

    for (const Tier &tier : tiers) {
        // Smaller than what is wanted would look soft: wait for a bigger tier (a 20 % shortfall passes).
        if (tier.side * 5 < edgeLength * 4)
            continue;
        QImageReader reader(freedesktopThumbnailPath(storeRoot, QLatin1String(tier.dir), filePath));
        if (!reader.canRead())
            continue;
        const QImage image = reader.read();
        if (image.isNull())
            continue;
        // Stale unless the file has not changed since the thumbnail was made.
        bool ok = false;
        const qint64 recorded = image.text(QStringLiteral("Thumb::MTime")).toLongLong(&ok);
        if (!ok || recorded != source.lastModified().toSecsSinceEpoch())
            continue;
        // The URI guards against two files sharing a hash; the size (recommended by the specification) against a
        // file replaced within the same second. Absent tags are accepted: not every generator writes them.
        const QString uri = image.text(QStringLiteral("Thumb::URI"));
        if (!uri.isEmpty() && uri.toUtf8() != QUrl::fromLocalFile(source.absoluteFilePath()).toEncoded())
            continue;
        const QString size = image.text(QStringLiteral("Thumb::Size"));
        if (!size.isEmpty() && size.toLongLong() != source.size())
            continue;
        return fitTo(image, edgeLength);
    }
    return {};
}

} // namespace detail

#ifdef _WIN32

namespace {

// Each worker thread that asks the shell needs COM, once; it is released when the thread ends.
struct ComScope {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ~ComScope()
    {
        if (SUCCEEDED(hr))
            CoUninitialize();
    }
};

QImage fromBitmap(HBITMAP bitmap)
{
    BITMAP info{};
    if (!GetObject(bitmap, sizeof(info), &info) || info.bmWidth <= 0 || info.bmHeight <= 0)
        return {};

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = info.bmWidth;
    bi.bmiHeader.biHeight = -info.bmHeight; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    QImage image(info.bmWidth, info.bmHeight, QImage::Format_ARGB32_Premultiplied);
    HDC dc = GetDC(nullptr);
    const int lines = GetDIBits(dc, bitmap, 0, UINT(info.bmHeight), image.bits(), &bi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    if (lines != info.bmHeight)
        return {};

    // Some thumbnails carry no alpha at all (the fourth byte is 0 everywhere): they are opaque.
    bool anyAlpha = false;
    for (int y = 0; y < image.height() && !anyAlpha; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(line[x]) != 0) {
                anyAlpha = true;
                break;
            }
        }
    }
    return anyAlpha ? image : image.convertToFormat(QImage::Format_RGB32);
}

} // namespace

QImage systemThumbnail(const QString &filePath, int edgeLength)
{
    if (edgeLength <= 0)
        return {};
    thread_local ComScope com;
    if (FAILED(com.hr) && com.hr != RPC_E_CHANGED_MODE)
        return {};

    const QString native = QDir::toNativeSeparators(QFileInfo(filePath).absoluteFilePath());
    IShellItemImageFactory *factory = nullptr;
    if (FAILED(SHCreateItemFromParsingName(reinterpret_cast<PCWSTR>(native.utf16()), nullptr,
                                           IID_PPV_ARGS(&factory))) || !factory)
        return {};

    // THUMBNAILONLY: never the generic file-type icon. INCACHEONLY: never make the shell decode the file;
    // if Explorer has not shown this picture yet, that is our cue to do it ourselves.
    // BIGGERSIZEOK: a cached 256 px thumbnail for a 160 px request is better than a miss.
    HBITMAP bitmap = nullptr;
    const SIZE size{edgeLength, edgeLength};
    const HRESULT hr = factory->GetImage(size, SIIGBF_THUMBNAILONLY | SIIGBF_INCACHEONLY | SIIGBF_BIGGERSIZEOK, &bitmap);
    factory->Release();
    if (FAILED(hr) || !bitmap)
        return {};

    const QImage image = fromBitmap(bitmap);
    DeleteObject(bitmap);
    return fitTo(image, edgeLength);
}

#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)

QImage systemThumbnail(const QString &filePath, int edgeLength)
{
    if (edgeLength <= 0)
        return {};
    // One store, as the specification says: $XDG_CACHE_HOME/thumbnails, or ~/.cache/thumbnails without it.
    // Inside a Flatpak XDG_CACHE_HOME is the app's own private cache, so the host's (what file managers write
    // to) is used instead.
    const bool flatpak = QFileInfo::exists(QStringLiteral("/.flatpak-info"));
    QByteArray base = qgetenv(flatpak ? "HOST_XDG_CACHE_HOME" : "XDG_CACHE_HOME");
    QString dir = base.isEmpty() ? QDir::homePath() + QStringLiteral("/.cache") : QString::fromLocal8Bit(base);
    return detail::freedesktopThumbnail(dir + QStringLiteral("/thumbnails"), filePath, edgeLength);
}

#else

QImage systemThumbnail(const QString &, int)
{
    return {};
}

#endif

} // namespace core
