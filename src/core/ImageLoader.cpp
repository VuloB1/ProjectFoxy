#include "ImageLoader.h"
#include "DecoderRegistry.h"
#include "MetadataReader.h"
#include <QFileInfo>
#include <QRunnable>

namespace core {

ImageLoader::ImageLoader(QObject *parent)
    : QObject(parent)
{
    // Leave one core free for the UI thread; decoding is the hot path.
    const int workers = qMax(1, QThread::idealThreadCount() - 1);
    m_pool.setMaxThreadCount(workers);
}

ImageLoader::~ImageLoader()
{
    for (const CancelFlag &flag : std::as_const(m_active))
        flag->store(true); // let queued work skip itself instead of decoding for nobody
    m_pool.waitForDone();
}

void ImageLoader::requestLoad(const QString &requestId, const QString &filePath, QSize maxSize)
{
    cancel(requestId);
    const auto flag = std::make_shared<std::atomic<bool>>(false);
    m_active.insert(requestId, flag);

    m_pool.start(QRunnable::create([this, requestId, filePath, maxSize, flag]() {
        // Superseded while it was still waiting in the queue: nobody wants it.
        if (flag->load())
            return;

        auto decoder = DecoderRegistry::instance().decoderFor(filePath);
        if (!decoder) {
            deliver(requestId, flag, {}, QStringLiteral("Unsupported format: %1").arg(filePath));
            return;
        }

        const DecodeResult result = decoder->decode(filePath, maxSize);
        if (!result.ok) {
            deliver(requestId, flag, {}, result.error);
            return;
        }
        // The decode itself could not be interrupted, but what comes after it can be
        // skipped: reading the metadata re-reads the whole file.
        if (flag->load())
            return;

        ImageDocument doc;
        doc.filePath = filePath;
        doc.pixels = result.image;
        doc.sourceSize = result.sourceSize;
        doc.isPreview = maxSize.isValid();
        doc.fileSizeBytes = QFileInfo(filePath).size();
        doc.metadata = MetadataReader::read(filePath); // off the UI thread, same as the decode above
        doc.iccProfile = result.iccProfile;
        doc.animationFrames = result.frames;
        doc.animationDelaysMs = result.frameDelaysMs;
        deliver(requestId, flag, doc, QString());
    }));
}

void ImageLoader::deliver(const QString &requestId, const CancelFlag &flag, const ImageDocument &doc,
                          const QString &error)
{
    // The check that matters runs on the loader's own thread, at the moment of
    // delivery: a worker can pass any check of its own and still lose the race to a
    // newer requestLoad() issued while its result was in flight.
    QMetaObject::invokeMethod(
        this,
        [this, requestId, flag, doc, error]() {
            if (m_active.value(requestId) != flag)
                return; // superseded or cancelled since the worker started
            m_active.remove(requestId);
            if (error.isEmpty())
                emit loaded(requestId, doc);
            else
                emit failed(requestId, error);
        },
        Qt::QueuedConnection);
}

void ImageLoader::cancel(const QString &requestId)
{
    // A QThreadPool task already mid-decode can't be interrupted from here. Flagging
    // it makes the worker skip every step it has not reached yet, and the check in
    // deliver() drops whatever it finishes with.
    if (const CancelFlag flag = m_active.take(requestId))
        flag->store(true);
}

void ImageLoader::preloadNeighbors(const QStringList &filePaths, QSize maxSize)
{
    for (const auto &path : filePaths) {
        m_pool.start(QRunnable::create([path, maxSize]() {
            auto decoder = DecoderRegistry::instance().decoderFor(path);
            if (decoder)
                decoder->decode(path, maxSize); // result cached at decoder/OS level
        }));
    }
}

} // namespace core
