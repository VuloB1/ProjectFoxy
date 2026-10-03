#pragma once

#include <QImage>
#include <QString>
#include <QStringList>

namespace core {

// What gets used for JPEG / WebP when the caller does not choose: high enough
// that re-saving a photo does not visibly degrade it. (Qt's own default for
// JPEG is 75, which does.)
constexpr int kDefaultJpegQuality = 95;
constexpr int kDefaultWebpQuality = 95;

struct SaveOptions {
    // 1-100, only meaningful for JPEG/WebP (lossy formats); -1 = the defaults above.
    int quality = -1;
    // The ORIGINAL file this picture was made from. When set (and the format can hold it:
    // JPEG, PNG, WebP), its camera/exposure/GPS/date information (EXIF), IPTC and XMP are
    // carried over into the saved file - the pixels are already upright, so the orientation
    // is reset to "normal", and the size fields are updated. Its ICC profile is NOT copied
    // (see iccProfile). Empty = write a file with no metadata.
    QString metadataSource;
    // The ICC profile that describes `image`'s pixels, when they are NOT sRGB (a profile
    // the decoder could not apply: see DecodeResult::iccProfile). Empty - the normal case,
    // since everything is converted to sRGB when it is opened - writes an sRGB profile.
    // The ORIGINAL file's profile is never copied: it describes the original's pixels.
    QByteArray iccProfile;
};

struct SaveResult {
    bool ok = false;
    QString error;         // why it failed, ready to show
    bool metadataCopied = false;
    // Set when the picture was saved but something secondary could not be done
    // (e.g. the metadata could not be carried over). Empty when everything went well.
    QString warning;
};

// Saves `image` to `path`, choosing an encoder from the extension: PNG and BMP
// through Qt, JPEG / WebP / TIFF through libvips (higher JPEG quality control, 4:4:4
// chroma for quality >= 90, lossless LZW for TIFF).
//
// The write is ATOMIC: the file is encoded in memory, written to a temporary file
// next to the target and only then moved over it, so a failure (full disk, a
// crash, an unsupported extension) can never leave the original truncated or
// half-written. Unicode file names work. Transparency is flattened onto white for
// JPEG, which has no alpha channel.
SaveResult saveImageWithOptions(const QImage &image, const QString &path, const SaveOptions &options = {});

// Convenience for callers that only choose a quality (batch export). Same
// guarantees; returns just success.
bool saveImage(const QImage &image, const QString &path, int quality = -1);

// Extensions saveImage() can actually write, for populating a save dialog's
// filters.
QStringList supportedSaveExtensions();

} // namespace core
