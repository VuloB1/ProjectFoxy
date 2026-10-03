#pragma once

#include <QQuickImageProvider>
#include <QColor>
#include <QMutex>

// Bridge between core::ImageLoader (which produces plain QImage on a worker
// thread) and QML's Image element (which needs a URL). AppController writes
// the latest decoded frame here and points Image.source at
// "image://viewer/current?rev=N" - bumping the revision in the query string
// forces QML to re-request even though the provider id itself doesn't
// change. Also separately serves "image://viewer/original?rev=N" - the
// as-opened, never-edited image - for ImageCanvas.qml's before/after
// compare toggle, "image://viewer/lut?rev=N" - the 256x1 tone lookup table
// the Grade.frag shader reads for the Ajustes stage (see
// core::edit::buildAdjustLut), "image://viewer/looklut?rev=N" - the same for
// the active Filtros look - and "image://viewer/lookthumb/<lookId>?rev=N" -
// a small preview of the current photo with that look applied, rendered on
// request from the proxy image set by setLookProxy() - and
// "image://viewer/effectthumb/<effectId>?rev=N", the same for an Efectos effect
// at its "sample" settings (see core::edit::sampleEffectValues).
class ImageProvider : public QQuickImageProvider {
public:
    ImageProvider();

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

    void setImage(const QImage &image);
    // The pixel of the current picture at (x, y) as straight colour; invalid outside it.
    QColor pixelAt(int x, int y) const;
    void setOriginalImage(const QImage &image);
    void setLut(const QImage &image);
    void setLookLut(const QImage &image);
    void setLookProxy(const QImage &image);

private:
    mutable QMutex m_mutex;
    QImage m_current;
    QImage m_original;
    QImage m_lut;
    QImage m_lookLut;
    QImage m_lookProxy;
};
