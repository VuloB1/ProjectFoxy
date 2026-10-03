#pragma once

#include <QString>
#include <QVariantMap>
#include <QDateTime>

namespace core {

struct ImageMetadata {
    QString cameraMake;
    QString cameraModel;
    QDateTime dateTaken;
    double focalLengthMm = 0.0;
    double apertureF = 0.0;
    double exposureSeconds = 0.0;
    int isoSpeed = 0;
    double gpsLatitude = 0.0;
    double gpsLongitude = 0.0;
    bool hasGps = false;
    QVariantMap raw; // full EXIF/IPTC/XMP tag dump for the metadata panel
};

// Thin wrapper over exiv2. Kept separate from the pixel decoders so reading
// metadata (e.g. for the filmstrip's date sorting) never requires decoding
// pixels.
class MetadataReader {
public:
    static ImageMetadata read(const QString &filePath);

    // Non-destructive: writes to a copy, or in-place if `filePath` is
    // already a working copy. Used by the "strip metadata for privacy" tool.
    static bool strip(const QString &filePath, QString *error = nullptr);
};

} // namespace core
