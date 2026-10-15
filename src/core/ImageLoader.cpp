#include "ImageLoader.h"
#include "DecoderRegistry.h"
#include "MetadataReader.h"
#include "decoders/VipsGuard.h"
#include <QFileInfo>
#include <QRunnable>

namespace core {

ImageLoader::ImageLoader(QObject *parent)
    : QObject(parent)
{
    // Leave one core free for the UI thread; decoding is the hot path.
    const int workers = qMax(1, QThread::idealThreadCount() - 1);
    m_pool.setMaxThreadCount(workers);
    m_prefetchPool.setMaxThreadCount(1);
    // libvips takes a noticeable time to start: do it now, not inside the first picture the user opens.
    m_prefetchPool.start(QRunnable::create([]() { ensureVipsInitialized(); }));
}

ImageLoader::~ImageLoader()
{
    for (const CancelFlag &flag : std::as_const(m_active))
        flag->store(true); // let queued work skip itself instead of decoding for nobody
    ++m_prefetchGeneration; // what is still queued is not prefetched
    m_prefetchPool.waitForDone();
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

        ImageDocument doc;
        QString error;
        if (!maxSize.isValid() && wantsQuickPreview(filePath)) {
            ImageDocument preview;
            QString ignored; // if the reduced one fails, the full decode below says why
            if (decodeUncached(filePath, QSize(kQuickPreviewSide, kQuickPreviewSide), preview, ignored)
                && !flag->load()) {
                QMetaObject::invokeMethod(
                    this,
                    [this, requestId, flag, preview]() {
                        if (m_active.value(requestId) == flag)
                            emit previewLoaded(requestId, preview);
                    },
                    Qt::QueuedConnection);
            }
        }
        if (maxSize.isValid() ? !decodeUncached(filePath, maxSize, doc, error) : !fetch(filePath, doc, error)) {
            deliver(requestId, flag, {}, error);
            return;
        }
        // The decode itself could not be interrupted, but a cancelled request need not be delivered.
        if (flag->load())
            return;
        deliver(requestId, flag, doc, QString());
    }));
}

bool ImageLoader::decodeUncached(const QString &filePath, QSize maxSize, ImageDocument &doc, QString &error)
{
    auto decoder = DecoderRegistry::instance().decoderFor(filePath);
    if (!decoder) {
        error = QStringLiteral("Unsupported format: %1").arg(filePath);
        return false;
    }
    const DecodeResult result = decoder->decode(filePath, maxSize);
    if (!result.ok) {
        error = result.error;
        return false;
    }
    doc = ImageDocument();
    doc.filePath = filePath;
    doc.pixels = result.image;
    doc.sourceSize = result.sourceSize;
    doc.isPreview = maxSize.isValid();
    doc.fileSizeBytes = QFileInfo(filePath).size();
    doc.metadata = MetadataReader::read(filePath); // off the UI thread, same as the decode above
    doc.iccProfile = result.iccProfile;
    doc.animationFrames = result.frames;
    doc.animationDelaysMs = result.frameDelaysMs;
    const int longSide = qMax(doc.pixels.width(), doc.pixels.height());
    if (doc.animationFrames.isEmpty() && longSide > kDisplayProxySide * 5 / 4) // when the canvas wants one
        doc.displayProxy = doc.pixels.scaled(kDisplayProxySide, kDisplayProxySide, Qt::KeepAspectRatio,
                                             Qt::SmoothTransformation);
    return true;
}

bool ImageLoader::wantsQuickPreview(const QString &path)
{
    const QFileInfo info(path);
    const QString ext = info.suffix().toLower();
    if (info.size() < (1 << 20) || (ext != QLatin1String("jpg") && ext != QLatin1String("jpeg")
                                    && ext != QLatin1String("jpe") && ext != QLatin1String("jfif")))
        return false;
    std::lock_guard lock(m_cacheMutex);
    if (m_decoding.contains(path))
        return false; // being prefetched: the full picture is on its way
    for (const CacheEntry &e : m_cache)
        if (e.path == path && e.size == info.size() && e.modified == info.lastModified())
            return false;
    return true;
}

bool ImageLoader::fetch(const QString &path, ImageDocument &doc, QString &error)
{
    const QFileInfo info(path);
    const bool cacheable = info.exists();
    std::unique_lock lock(m_cacheMutex);
    if (cacheable) {
        // Somebody (a prefetch) is decoding this very file: wait for it instead of decoding it twice.
        m_cacheChanged.wait(lock, [&] { return !m_decoding.contains(path); });
        for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
            if (it->path == path && it->size == info.size() && it->modified == info.lastModified()) {
                doc = it->doc;
                m_cache.splice(m_cache.begin(), m_cache, it);
                return true;
            }
        }
        m_decoding.insert(path);
    }
    lock.unlock();

    const bool ok = decodeUncached(path, QSize(), doc, error);

    lock.lock();
    if (cacheable) {
        m_decoding.remove(path);
        m_cache.remove_if([&](const CacheEntry &e) { return e.path == path; }); // the older version of the file
        if (ok && doc.animationFrames.isEmpty()) {
            m_cache.push_front({path, info.size(), info.lastModified(), doc});
            // Up to 4 pictures and half a gigabyte, but always the newest one however big it is.
            qint64 bytes = 0;
            int count = 0;
            for (auto it = m_cache.begin(); it != m_cache.end();) {
                bytes += it->doc.pixels.sizeInBytes() + it->doc.displayProxy.sizeInBytes();
                if (++count > 1 && (count > 4 || bytes > (512LL << 20)))
                    it = m_cache.erase(it);
                else
                    ++it;
            }
        }
        m_cacheChanged.notify_all();
    }
    return ok;
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

void ImageLoader::prefetch(const QStringList &filePaths)
{
    const int generation = ++m_prefetchGeneration;
    for (const QString &path : filePaths) {
        m_prefetchPool.start(QRunnable::create([this, path, generation]() {
            if (m_prefetchGeneration != generation)
                return; // replaced by a newer list while it waited
            ImageDocument doc;
            QString error;
            fetch(path, doc, error); // the result stays in the cache
        }));
    }
}

} // namespace core
