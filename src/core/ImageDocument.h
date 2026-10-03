#pragma once

#include "MetadataReader.h"
#include <QString>
#include <QImage>
#include <QSize>
#include <QVector>

namespace core {

// A single loaded image plus the metadata the UI needs. Immutable snapshot
// produced by ImageLoader; edits are applied on top via EditStack, never by
// mutating this in place.
struct ImageDocument {
    QString filePath;
    QImage pixels;
    QSize sourceSize;      // full resolution, even if `pixels` is a preview
    bool isPreview = false; // true if pixels is downsampled
    qint64 fileSizeBytes = 0;
    ImageMetadata metadata; // EXIF/IPTC/XMP, read alongside the decode (see ImageLoader)
    QByteArray iccProfile;  // see DecodeResult::iccProfile: only when the pixels are NOT sRGB

    // Populated only for a genuinely animated GIF/APNG full (non-preview)
    // load - see AnimatedDecoder. Empty or size-1 means "not animated";
    // `pixels` above is always frame 0 either way, so every existing caller
    // that ignores these two fields keeps working unchanged.
    QVector<QImage> animationFrames;
    QVector<int> animationDelaysMs; // parallel to animationFrames, milliseconds
};

} // namespace core
