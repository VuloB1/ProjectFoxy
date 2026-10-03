#include "BatchExporter.h"
#include "DecoderRegistry.h"
#include "ImageWriter.h"

#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QRunnable>
#include <QSet>
#include <QThreadPool>

BatchExporter::BatchExporter(QObject *parent)
    : QObject(parent)
{
}

void BatchExporter::start(const QStringList &sourceFiles, const QUrl &destFolderUrl, const QString &format, int maxDimension, int quality)
{
    if (m_running || sourceFiles.isEmpty())
        return;

    const QString destFolder = destFolderUrl.isLocalFile() ? destFolderUrl.toLocalFile() : destFolderUrl.toString();
    QDir().mkpath(destFolder);

    m_running = true;
    m_progress = 0;
    m_total = sourceFiles.size();
    emit runningChanged();
    emit progressChanged();

    auto cancelFlag = std::make_shared<std::atomic<bool>>(false);
    m_cancelRequested = cancelFlag;

    QThreadPool::globalInstance()->start(QRunnable::create(
        [this, sourceFiles, destFolder, format, maxDimension, quality, cancelFlag]() {
            int succeeded = 0;
            int failed = 0;
            QSet<QString> usedNames;

            for (const QString &sourcePath : sourceFiles) {
                if (cancelFlag->load())
                    break;

                bool ok = false;
                auto decoder = core::DecoderRegistry::instance().decoderFor(sourcePath);
                if (decoder) {
                    const core::DecodeResult result = decoder->decode(sourcePath, QSize());
                    if (result.ok && !result.image.isNull()) {
                        QImage img = result.image;
                        if (maxDimension > 0 && (img.width() > maxDimension || img.height() > maxDimension)) {
                            img = img.scaled(maxDimension, maxDimension,
                                              Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        }
                        // Never over the picture being exported (same folder, same format) nor over
                        // another one of this batch that has the same base name (a.png and a.jpg).
                        const QString baseName = QFileInfo(sourcePath).completeBaseName();
                        const QString sourceKey = QFileInfo(sourcePath).absoluteFilePath().toLower();
                        QString destPath = destFolder + QLatin1Char('/') + baseName + QLatin1Char('.') + format;
                        for (int n = 2; usedNames.contains(QFileInfo(destPath).absoluteFilePath().toLower())
                                        || QFileInfo(destPath).absoluteFilePath().toLower() == sourceKey; ++n)
                            destPath = destFolder + QLatin1Char('/') + QStringLiteral("%1_%2.%3").arg(baseName).arg(n).arg(format);
                        usedNames.insert(QFileInfo(destPath).absoluteFilePath().toLower());
                        // Keep the original's camera/GPS/date information (and colour
                        // profile) in the exported copy where the format can hold it.
                        core::SaveOptions options;
                        options.quality = quality;
                        options.metadataSource = sourcePath;
                        options.iccProfile = result.iccProfile; // empty unless the pixels are not sRGB
                        ok = core::saveImageWithOptions(img, destPath, options).ok;
                    }
                }
                if (ok) ++succeeded; else ++failed;

                QMetaObject::invokeMethod(this, [this]() {
                    ++m_progress;
                    emit progressChanged();
                }, Qt::QueuedConnection);
            }

            QMetaObject::invokeMethod(this, [this, succeeded, failed]() {
                m_running = false;
                emit runningChanged();
                emit finished(succeeded, failed);
            }, Qt::QueuedConnection);
        }));
}

void BatchExporter::cancel()
{
    if (m_cancelRequested)
        m_cancelRequested->store(true);
}
