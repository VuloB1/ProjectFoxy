#include "QtImageDecoder.h"

#include <QImageReader>

namespace core {

bool QtImageDecoder::canDecode(const QString &filePath) const
{
    const QString ext = filePath.section('.', -1).toLower();
    return supportedExtensions().contains(ext);
}

DecodeResult QtImageDecoder::decode(const QString &filePath, QSize maxSize)
{
    DecodeResult result;

    // QImageReader opens the file through Qt (correct with any Unicode name) and
    // closes it again as soon as it is destroyed.
    QImageReader reader(filePath);
    reader.setAutoTransform(true);

    // An .ico holds several sizes of the same icon: take the largest.
    if (reader.imageCount() > 1) {
        int best = 0;
        qint64 bestPixels = 0;
        for (int i = 0; i < reader.imageCount(); ++i) {
            reader.jumpToImage(i);
            const QSize s = reader.size();
            if (qint64(s.width()) * s.height() > bestPixels) {
                bestPixels = qint64(s.width()) * s.height();
                best = i;
            }
        }
        reader.jumpToImage(best);
    }

    result.sourceSize = reader.size();
    QImage image = reader.read();
    if (image.isNull()) {
        result.error = QStringLiteral("%1 (%2)").arg(reader.errorString(), filePath);
        return result;
    }
    if (result.sourceSize.isEmpty())
        result.sourceSize = image.size();

    // These formats have no shrink-on-load: for a preview, scale the decoded picture.
    if (maxSize.isValid() && maxSize.width() > 0 && maxSize.height() > 0
        && (image.width() > maxSize.width() || image.height() > maxSize.height())) {
        image = image.scaled(maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    result.image = image.convertToFormat(QImage::Format_RGBA8888);
    result.ok = true;
    return result;
}

QStringList QtImageDecoder::supportedExtensions() const
{
    return { "bmp", "gif", "ico" };
}

} // namespace core
