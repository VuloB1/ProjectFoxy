#include "MetadataReader.h"

#include <exiv2/exiv2.hpp>
#include <QFile>

namespace core {

namespace {

// GPSLatitude/GPSLongitude are stored as three rationals (degrees, minutes,
// seconds); the matching *Ref tag ('S'/'W') gives the sign.
double dmsToDecimal(const Exiv2::Exifdatum &datum, char ref, char negativeRef)
{
    if (datum.count() < 3)
        return 0.0;
    double decimal = datum.toFloat(0) + datum.toFloat(1) / 60.0 + datum.toFloat(2) / 3600.0;
    if (ref == negativeRef)
        decimal = -decimal;
    return decimal;
}

} // namespace

ImageMetadata MetadataReader::read(const QString &filePath)
{
    ImageMetadata meta;

    // Read the bytes ourselves via QFile (correctly handles Unicode paths on
    // Windows) and hand them to exiv2 in memory - this build has no
    // wide-char path overload, and exiv2's own std::string-path open() goes
    // through a plain fopen() that uses the ANSI codepage on Windows.
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return meta;
    const QByteArray fileData = file.readAll();
    file.close();
    if (fileData.isEmpty())
        return meta;

    try {
        auto image = Exiv2::ImageFactory::open(
            reinterpret_cast<const Exiv2::byte *>(fileData.constData()),
            static_cast<size_t>(fileData.size()));
        if (!image)
            return meta;

        image->readMetadata();
        const Exiv2::ExifData &exif = image->exifData();
        if (exif.empty())
            return meta;

        if (auto it = Exiv2::make(exif); it != exif.end())
            meta.cameraMake = QString::fromStdString(it->toString()).trimmed();
        if (auto it = Exiv2::model(exif); it != exif.end())
            meta.cameraModel = QString::fromStdString(it->toString()).trimmed();
        if (auto it = Exiv2::dateTimeOriginal(exif); it != exif.end())
            meta.dateTaken = QDateTime::fromString(QString::fromStdString(it->toString()),
                                                     QStringLiteral("yyyy:MM:dd HH:mm:ss"));
        if (auto it = Exiv2::focalLength(exif); it != exif.end())
            meta.focalLengthMm = it->toFloat();
        if (auto it = Exiv2::fNumber(exif); it != exif.end())
            meta.apertureF = it->toFloat();
        if (auto it = Exiv2::lensName(exif); it != exif.end())
            meta.lensModel = QString::fromStdString(it->toString()).trimmed();
        if (const auto it = exif.findKey(Exiv2::ExifKey("Exif.Photo.SubjectDistance")); it != exif.end())
            meta.subjectDistanceM = it->toFloat();
        if (auto it = Exiv2::exposureTime(exif); it != exif.end())
            meta.exposureSeconds = it->toFloat();
        if (auto it = Exiv2::isoSpeed(exif); it != exif.end())
            meta.isoSpeed = static_cast<int>(it->toInt64());

        const auto latIt = exif.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLatitude"));
        const auto latRefIt = exif.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLatitudeRef"));
        const auto lonIt = exif.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLongitude"));
        const auto lonRefIt = exif.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSLongitudeRef"));
        if (latIt != exif.end() && latRefIt != exif.end() && lonIt != exif.end() && lonRefIt != exif.end()) {
            const std::string latRefStr = latRefIt->toString();
            const std::string lonRefStr = lonRefIt->toString();
            meta.gpsLatitude = dmsToDecimal(*latIt, latRefStr.empty() ? 'N' : latRefStr[0], 'S');
            meta.gpsLongitude = dmsToDecimal(*lonIt, lonRefStr.empty() ? 'E' : lonRefStr[0], 'W');
            meta.hasGps = true;
        }

        // Full tag dump for a future "all metadata" view - key -> display string.
        for (const auto &datum : exif)
            meta.raw.insert(QString::fromStdString(datum.key()), QString::fromStdString(datum.toString()));
        // Maker notes name the lens under their own keys when the standard tag is absent.
        if (meta.lensModel.isEmpty()) {
            for (auto it = meta.raw.cbegin(); it != meta.raw.cend(); ++it) {
                if (it.key().endsWith(QLatin1String(".LensModel")) || it.key().endsWith(QLatin1String(".LensType"))
                    || it.key().endsWith(QLatin1String(".Lens"))) {
                    const QString v = it.value().toString().trimmed();
                    if (!v.isEmpty() && v != QLatin1String("0") && !v.startsWith(QLatin1String("Unknown"), Qt::CaseInsensitive)) {
                        meta.lensModel = v;
                        break;
                    }
                }
            }
        }
    } catch (const Exiv2::Error &) {
        // Not a supported format, or corrupt/absent metadata - return
        // whatever was gathered so far (likely the default-constructed
        // empty result) rather than propagating.
    }

    return meta;
}

bool MetadataReader::strip(const QString &filePath, QString *error)
{
    Q_UNUSED(filePath);
    if (error)
        *error = QStringLiteral("MetadataReader::strip not implemented yet");
    return false;
}

} // namespace core
