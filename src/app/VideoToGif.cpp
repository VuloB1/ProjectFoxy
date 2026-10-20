#include "VideoToGif.h"

#include "VideoSupport.h"

#include <QFileInfo>
#include <QUrl>
#include <algorithm>
#include <cmath>

namespace {
constexpr int kWatchdogMs = 20000; // a video that gives no frame for this long is given up
}

VideoToGif::VideoToGif(QObject *parent)
    : QObject(parent)
{
    connect(&m_reader, &VideoFrameReader::opened, this, [this]() { m_reader.requestFrame(0); });
    connect(&m_reader, &VideoFrameReader::failed, this, [this](const QString &) { fail(m_loaded ? QStringLiteral("read") : QStringLiteral("open")); });
    connect(&m_reader, &VideoFrameReader::frameReady, this, &VideoToGif::onFrame);
    connect(&m_reader, &VideoFrameReader::sampleFinished, this, [this]() {
        if (!m_busy)
            return;
        if (m_cancelled || int(m_frames.size()) >= kMaxFrames)
            finish(!m_cancelled);
        else
            nextSegment();
    });
    m_watchdog.setSingleShot(true);
    m_watchdog.setInterval(kWatchdogMs);
    connect(&m_watchdog, &QTimer::timeout, this, [this]() {
        if (m_busy)
            m_reader.cancelSample();
        fail(QStringLiteral("read"));
    });
}

QString VideoToGif::toPath(const QString &file)
{
    const QUrl url(file);
    return url.isLocalFile() ? url.toLocalFile() : file;
}

bool VideoToGif::isVideo(const QString &file) const
{
    static VideoSupport support;
    const QString path = toPath(file);
    return VideoSupport::looksLikeVideo(path) && support.extensions().contains(QFileInfo(path).suffix().toLower());
}

void VideoToGif::open(const QString &file)
{
    close();
    const QString path = toPath(file);
    m_fileName = QFileInfo(path).fileName();
    m_error.clear();
    emit errorChanged();
    m_reader.open(path);
}

void VideoToGif::close()
{
    if (m_busy)
        cancel();
    if (m_loaded) {
        m_loaded = false;
        emit loadedChanged();
    }
    m_source = QSize();
    {
        QMutexLocker lock(&m_previewMutex);
        m_preview = QImage();
    }
    m_pendingSeek = -1;
}

void VideoToGif::seek(int ms)
{
    if (!m_loaded || m_busy)
        return;
    m_reader.requestFrame(ms);
}

void VideoToGif::onFrame(qint64, const QImage &image)
{
    if (m_busy) {
        // a frame of the sample
        m_watchdog.start();
        if (m_cancelled || int(m_frames.size()) >= kMaxFrames) {
            m_reader.cancelSample();
            return;
        }
        QImage cut = image;
        if (m_crop.isValid() && m_crop != image.rect())
            cut = image.copy(m_crop.intersected(image.rect()));
        if (cut.size() != m_outSize)
            cut = cut.scaled(m_outSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        m_frames << cut;
        m_progress = std::min(1.0, qreal(m_frames.size()) / m_expected);
        emit progressChanged();
        return;
    }
    // a preview frame
    {
        QMutexLocker lock(&m_previewMutex);
        m_preview = image;
    }
    ++m_revision;
    if (!m_loaded) {
        m_source = image.size();
        m_loaded = true;
        emit loadedChanged();
    }
    emit previewChanged();
}

int VideoToGif::frameCount(const QVariantList &segments, int fps)
{
    const qint64 step = std::max(1, int(std::lround(1000.0 / std::max(1, fps))));
    qint64 total = 0;
    for (const QVariant &s : segments) {
        const QVariantList pair = s.toList();
        if (pair.size() < 2)
            continue;
        const qint64 from = pair[0].toLongLong(), to = pair[1].toLongLong();
        if (to > from)
            total += (to - from + step - 1) / step;
    }
    return int(std::min<qint64>(total, 1 << 20));
}

void VideoToGif::create(const QVariantList &segments, int fps, qreal cropX, qreal cropY, qreal cropW, qreal cropH,
                        int outWidth)
{
    if (!m_loaded || m_busy || m_source.isEmpty())
        return;
    m_segments.clear();
    for (const QVariant &s : segments) {
        const QVariantList pair = s.toList();
        if (pair.size() >= 2 && pair[1].toLongLong() > pair[0].toLongLong())
            m_segments << qMakePair(pair[0].toLongLong(), pair[1].toLongLong());
    }
    if (m_segments.isEmpty())
        return;

    m_step = std::max(1, int(std::lround(1000.0 / std::max(1, fps))));
    const int sw = m_source.width(), sh = m_source.height();
    const int cx = std::clamp(int(std::lround(cropX * sw)), 0, sw - 2);
    const int cy = std::clamp(int(std::lround(cropY * sh)), 0, sh - 2);
    const int cw = std::clamp(int(std::lround(cropW * sw)), 2, sw - cx);
    const int ch = std::clamp(int(std::lround(cropH * sh)), 2, sh - cy);
    m_crop = QRect(cx, cy, cw, ch);
    const int ow = std::clamp(outWidth, 16, 4096);
    m_outSize = QSize(ow, std::max(2, int(std::lround(double(ow) * ch / cw))));
    m_expected = std::max(1, std::min(kMaxFrames, frameCount(segments, fps)));

    m_frames.clear();
    m_frames.reserve(m_expected);
    m_segment = 0;
    m_cancelled = false;
    m_progress = 0;
    emit progressChanged();
    if (!m_error.isEmpty()) {
        m_error.clear();
        emit errorChanged();
    }
    setBusy(true);
    m_watchdog.start();
    nextSegment();
}

void VideoToGif::nextSegment()
{
    if (m_segment >= m_segments.size()) {
        finish(true);
        return;
    }
    const auto seg = m_segments[m_segment++];
    m_reader.sample(seg.first, seg.second, m_step);
}

void VideoToGif::cancel()
{
    if (!m_busy)
        return;
    m_cancelled = true;
    if (m_reader.sampling())
        m_reader.cancelSample(); // -> sampleFinished -> finish()
    else
        finish(false);
}

void VideoToGif::finish(bool ok)
{
    m_watchdog.stop();
    const bool wasCancelled = m_cancelled;
    QList<QImage> frames = std::move(m_frames);
    m_frames.clear();
    setBusy(false);
    if (ok && !wasCancelled && !frames.isEmpty())
        emit framesReady(frames, int(m_step), m_fileName);
    else if (wasCancelled)
        emit cancelled();
    else if (frames.isEmpty())
        fail(QStringLiteral("read"));
}

void VideoToGif::setBusy(bool busy)
{
    if (busy == m_busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void VideoToGif::fail(const QString &what)
{
    m_watchdog.stop();
    if (m_busy) {
        m_frames.clear();
        setBusy(false);
    }
    m_error = what;
    emit errorChanged();
}

QImage VideoPreviewProvider::requestImage(const QString &, QSize *size, const QSize &requestedSize)
{
    QImage image = m_video->preview();
    if (image.isNull()) {
        image = QImage(16, 9, QImage::Format_RGBA8888);
        image.fill(Qt::black);
    }
    // (a side of 0 in the requested size means "any")
    const int maxW = requestedSize.width() > 0 ? requestedSize.width() : image.width();
    const int maxH = requestedSize.height() > 0 ? requestedSize.height() : image.height();
    if (maxW < image.width() || maxH < image.height())
        image = image.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (size)
        *size = image.size();
    return image;
}
