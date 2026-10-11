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
//
// "image://viewer/current?rev=N&side=S" (and "original" likewise) is the same picture brought down so its long side is S pixels (the display
// proxy: a picture shown far smaller than it is needs no full-size GPU texture). The reduced copy is made once per
// picture and side and kept; the full picture is untouched, so every edit, readout and save still uses it.
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

    // `image` brought down to `side` pixels on its long side (smooth); the picture itself when it is not bigger.
    static QImage reducedTo(const QImage &image, int side);

private:
    mutable QMutex m_mutex;
    QImage m_current;
    QImage m_original;
    QImage m_lut;
    QImage m_lookLut;
    QImage m_lookProxy;
    // The reduced copy of a picture, valid for `side` while `serial` is the one it was made from.
    struct Proxy {
        QImage image;
        int side = 0;
        quint64 serial = 0;
    };
    quint64 m_currentSerial = 0;
    quint64 m_originalSerial = 0;
    Proxy m_currentProxy;
    Proxy m_originalProxy;
};
