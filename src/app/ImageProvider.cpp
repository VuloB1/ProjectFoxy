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
    Proxy *proxy = nullptr; // set for the two pictures that can be shown reduced
    int side = 0; // > 0: a reduced copy is wanted and exists; < 0: it has to be made
    quint64 serial = 0;
    enum class Thumb { None, Look, Effect } thumb = Thumb::None;
    {
        QMutexLocker lock(&m_mutex);
        if (id.startsWith(QLatin1String("original"))) {
            image = m_original;
            serial = m_originalSerial;
            proxy = &m_originalProxy;
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
            serial = m_currentSerial;
            proxy = &m_currentProxy;
        }
        if (proxy) {
            const int at = id.indexOf(QLatin1String("side="));
            if (at >= 0)
                side = id.mid(at + 5).section(QLatin1Char('&'), 0, 0).toInt();
            if (side > 0 && proxy->serial == serial && proxy->side == side && !proxy->image.isNull())
                image = proxy->image; // made before
            else if (side > 0)
                side = -side; // to be made below
        }
    }

    // The scaling is a real piece of work: outside the lock (pixelAt() and other requests must not wait for it).
    if (side < 0) {
        image = reducedTo(image, -side);
        QMutexLocker lock(&m_mutex);
        const bool isOriginal = id.startsWith(QLatin1String("original"));
        if ((isOriginal ? m_originalSerial : m_currentSerial) == serial) { // not replaced meanwhile
            Proxy &slot = isOriginal ? m_originalProxy : m_currentProxy;
            slot.image = image;
            slot.side = -side;
            slot.serial = serial;
        }
    }

    // The look/effect itself is rendered outside the lock: it is a real (if
    // tiny) image operation, and requests can come in from several threads at
    // once.
    if (thumb == Thumb::Look) {
        image = core::edit::applyLook(image, thumbId, 1.0);
    } else if (thumb == Thumb::Effect) {
        if (const core::edit::EffectSpec *spec = core::edit::findEffect(thumbId))
            image = core::edit::applyEffect(image, thumbId, core::edit::sampleEffectValues(*spec), 1.0, nullptr, spec->usesText ? QStringLiteral("Texto") : QString());
    }

    if (size)
        *size = image.size();
    return image;
}

QImage ImageProvider::reducedTo(const QImage &image, int side)
{
    if (image.isNull() || side <= 0 || qMax(image.width(), image.height()) <= side)
        return image;
    return image.scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void ImageProvider::setImage(const QImage &image)
{
    QMutexLocker lock(&m_mutex);
    m_current = image;
    ++m_currentSerial;
    m_currentProxy = Proxy(); // belonged to the previous picture
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
    ++m_originalSerial;
    m_originalProxy = Proxy();
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
