#include "ImageProvider.h"
#include "edit/Effects.h"
#include "edit/Looks.h"

ImageProvider::ImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage ImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    Q_UNUSED(requestedSize); // full preview res is already what we want to show

    QImage image;
    QString thumbId;
    enum class Thumb { None, Look, Effect } thumb = Thumb::None;
    {
        QMutexLocker lock(&m_mutex);
        if (id.startsWith(QLatin1String("original"))) {
            image = m_original;
        } else if (id.startsWith(QLatin1String("looklut"))) {
            image = m_lookLut;
        } else if (id.startsWith(QLatin1String("lut"))) {
            image = m_lut;
        } else if (id.startsWith(QLatin1String("lookthumb/"))) {
            image = m_lookProxy;
            thumbId = id.mid(int(QLatin1String("lookthumb/").size())).section(QLatin1Char('?'), 0, 0);
            thumb = Thumb::Look;
        } else if (id.startsWith(QLatin1String("effectthumb/"))) {
            image = m_lookProxy; // same small square of the photo as the look previews
            thumbId = id.mid(int(QLatin1String("effectthumb/").size())).section(QLatin1Char('?'), 0, 0);
            thumb = Thumb::Effect;
        } else {
            image = m_current;
        }
    }

    // The look/effect itself is rendered outside the lock: it is a real (if
    // tiny) image operation, and requests can come in from several threads at
    // once.
    if (thumb == Thumb::Look) {
        image = core::edit::applyLook(image, thumbId, 1.0);
    } else if (thumb == Thumb::Effect) {
        if (const core::edit::EffectSpec *spec = core::edit::findEffect(thumbId))
            image = core::edit::applyEffect(image, thumbId, core::edit::sampleEffectValues(*spec), 1.0);
    }

    if (size)
        *size = image.size();
    return image;
}

void ImageProvider::setImage(const QImage &image)
{
    QMutexLocker lock(&m_mutex);
    m_current = image;
}

QColor ImageProvider::pixelAt(int x, int y) const
{
    QMutexLocker lock(&m_mutex);
    if (m_current.isNull() || x < 0 || y < 0 || x >= m_current.width() || y >= m_current.height())
        return {};
    return m_current.pixelColor(x, y);
}

void ImageProvider::setOriginalImage(const QImage &image)
{
    QMutexLocker lock(&m_mutex);
    m_original = image;
}

void ImageProvider::setLut(const QImage &image)
{
    QMutexLocker lock(&m_mutex);
    m_lut = image;
}

void ImageProvider::setLookLut(const QImage &image)
{
    QMutexLocker lock(&m_mutex);
    m_lookLut = image;
}

void ImageProvider::setLookProxy(const QImage &image)
{
    QMutexLocker lock(&m_mutex);
    m_lookProxy = image;
}
