// An animated WebP writer on top of libwebp's animation encoder (which also does the frame differencing and
// picks lossy or lossless blocks per frame where that pays off).

#include "Anim.h"

#include <webp/encode.h>
#include <webp/mux.h>

#include <algorithm>
#include <memory>
#include "Translate.h"

namespace core::anim {

namespace {
struct EncoderDeleter { void operator()(WebPAnimEncoder *e) const { if (e) WebPAnimEncoderDelete(e); } };
}

bool writeWebp(FrameSource &frames, const WebpOptions &options, QIODevice &out, const ProgressFn &progress, QString *error)
{
    const int total = frames.count();
    const QSize size = frames.size();
    auto fail = [&](const QString &why) {
        if (error)
            *error = why;
        return false;
    };
    if (total < 1 || size.width() < 1 || size.height() < 1)
        return fail(core::tr("No hay fotogramas para guardar."));
    if (size.width() > 16383 || size.height() > 16383)
        return fail(core::tr("La imagen es demasiado grande para un WebP."));

    WebPAnimEncoderOptions encoderOptions;
    if (!WebPAnimEncoderOptionsInit(&encoderOptions))
        return fail(core::tr("No se pudo iniciar el codificador de WebP."));
    encoderOptions.anim_params.loop_count = std::max(0, options.loops);
    encoderOptions.allow_mixed = !options.lossless; // lossy and lossless blocks side by side where it is smaller

    std::unique_ptr<WebPAnimEncoder, EncoderDeleter> encoder(WebPAnimEncoderNew(size.width(), size.height(), &encoderOptions));
    if (!encoder)
        return fail(core::tr("No se pudo iniciar el codificador de WebP."));

    WebPConfig config;
    if (!WebPConfigInit(&config))
        return fail(core::tr("No se pudo iniciar el codificador de WebP."));
    config.lossless = options.lossless ? 1 : 0;
    config.quality = float(std::clamp(options.quality, 0, 100));
    config.method = options.lossless ? 4 : 4;
    if (!options.lossless) {
        config.use_sharp_yuv = 1;       // keeps thin coloured lines from bleeding
        config.alpha_quality = 100;
    }
    if (!WebPValidateConfig(&config))
        return fail(core::tr("Las opciones de WebP no son válidas."));

    int timestamp = 0;
    for (int i = 0; i < total; ++i) {
        int delayMs = 100;
        const QImage frame = frames.frame(i, &delayMs).convertToFormat(QImage::Format_RGBA8888);
        if (frame.size() != size)
            return fail(core::tr("Los fotogramas no tienen todos el mismo tamaño."));

        WebPPicture picture;
        if (!WebPPictureInit(&picture))
            return fail(core::tr("No se pudo preparar un fotograma."));
        picture.use_argb = 1;
        picture.width = size.width();
        picture.height = size.height();
        if (!WebPPictureImportRGBA(&picture, frame.constBits(), int(frame.bytesPerLine()))) {
            WebPPictureFree(&picture);
            return fail(core::tr("No se pudo preparar un fotograma."));
        }
        const int ok = WebPAnimEncoderAdd(encoder.get(), &picture, timestamp, &config);
        WebPPictureFree(&picture);
        if (!ok)
            return fail(core::tr("No se pudo codificar un fotograma: %1").arg(QString::fromUtf8(WebPAnimEncoderGetError(encoder.get()))));
        timestamp += std::max(1, delayMs);
        if (progress && !progress(i + 1, total + 1))
            return false;
    }
    if (!WebPAnimEncoderAdd(encoder.get(), nullptr, timestamp, nullptr)) // the end time of the last frame
        return fail(core::tr("No se pudo terminar el WebP."));

    WebPData data;
    WebPDataInit(&data);
    if (!WebPAnimEncoderAssemble(encoder.get(), &data))
        return fail(core::tr("No se pudo armar el WebP: %1").arg(QString::fromUtf8(WebPAnimEncoderGetError(encoder.get()))));
    const bool written = out.write(reinterpret_cast<const char *>(data.bytes), qint64(data.size)) == qint64(data.size);
    WebPDataClear(&data);
    if (!written)
        return fail(core::tr("No se pudo escribir el archivo."));
    if (progress)
        progress(total + 1, total + 1);
    return true;
}

} // namespace core::anim
