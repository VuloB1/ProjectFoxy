#pragma once

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <atomic>
#include <memory>

// Converts/resizes a whole folder's worth of images in one go - "batch
// export" from the Más menu. Runs on a single background worker (not
// parallelized across files): both the decoders and ImageWriter's libvips
// path already serialize their own internal entry (see VipsGuard.h), so
// running the files one at a time here is the simplest way to avoid piling
// several of those locks up at once for no real throughput gain.
class BatchExporter : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(int total READ total NOTIFY progressChanged)

public:
    explicit BatchExporter(QObject *parent = nullptr);

    bool running() const { return m_running; }
    int progress() const { return m_progress; }
    int total() const { return m_total; }

public slots:
    // sourceFiles: absolute paths to convert. destFolder: created if it
    // doesn't exist yet. format: target extension ("png","jpg","jpeg","bmp",
    // "tif","tiff","webp"). maxDimension: 0 keeps each image's original
    // size; otherwise its longest side is capped (aspect ratio preserved).
    // quality: 1-100, only meaningful for jpg/webp - see core::saveImage().
    void start(const QStringList &sourceFiles, const QUrl &destFolder, const QString &format, int maxDimension, int quality);
    void cancel();

signals:
    void runningChanged();
    void progressChanged();
    void finished(int succeeded, int failed);

private:
    bool m_running = false;
    int m_progress = 0;
    int m_total = 0;
    std::shared_ptr<std::atomic<bool>> m_cancelRequested;
};
