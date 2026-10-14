#include "ComicBook.h"
#include "DecoderRegistry.h"
#include "ImageHeader.h"
#include "Translate.h"

#include <QCollator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>

namespace core {

namespace {

bool isPicture(const QString &name, const QStringList &extensions)
{
    return extensions.contains(QFileInfo(name).suffix().toLower());
}

// "__MACOSX/..", "._name" (resource forks) and hidden files are never pages.
bool isJunk(const QString &name)
{
    return name.startsWith(QLatin1String("__MACOSX/")) || name.contains(QLatin1String("/._"))
           || QFileInfo(name).fileName().startsWith(QLatin1Char('.'));
}

} // namespace

bool ComicBook::isArchivePath(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QLatin1String("cbz") || suffix == QLatin1String("zip");
}

bool ComicBook::open(const QString &path, const QString &cacheDir)
{
    close();
    QMutexLocker lock(&m_mutex);
    m_error.clear();
    const QStringList extensions = DecoderRegistry::instance().allSupportedExtensions();
    const QFileInfo info(path);

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    if (info.isFile() && isArchivePath(path)) {
        if (!m_zip.open(path)) {
            m_error = m_zip.error();
            return false;
        }
        const auto &entries = m_zip.entries();
        for (int i = 0; i < entries.size(); ++i)
            if (!entries[i].isDirectory && !isJunk(entries[i].name) && isPicture(entries[i].name, extensions))
                m_pages.append({entries[i].name, QString(), i});
        std::sort(m_pages.begin(), m_pages.end(),
                  [&collator](const Page &a, const Page &b) { return collator.compare(a.name, b.name) < 0; });
        m_title = info.completeBaseName();
        m_cacheDir = cacheDir;
    } else {
        const QDir dir(info.isDir() ? info.absoluteFilePath() : info.absolutePath());
        QStringList filters;
        for (const QString &ext : extensions)
            filters << QStringLiteral("*.") + ext;
        const QStringList names = dir.entryList(filters, QDir::Files | QDir::Readable);
        for (const QString &name : names)
            m_pages.append({name, dir.absoluteFilePath(name), -1});
        std::sort(m_pages.begin(), m_pages.end(),
                  [&collator](const Page &a, const Page &b) { return collator.compare(a.name, b.name) < 0; });
        m_title = dir.dirName();
    }
    if (m_pages.isEmpty()) {
        m_error = core::tr("No hay imágenes que leer aquí.");
        return false;
    }
    return true;
}

void ComicBook::close()
{
    QMutexLocker lock(&m_mutex);
    m_pages.clear();
    m_zip.close();
    if (!m_cacheDir.isEmpty())
        QDir(m_cacheDir).removeRecursively();
    m_cacheDir.clear();
    m_title.clear();
}

int ComicBook::pageOfFile(const QString &path) const
{
    const QString name = QFileInfo(path).fileName();
    for (int i = 0; i < m_pages.size(); ++i)
        if (m_pages[i].name == name)
            return i;
    return 0;
}

QString ComicBook::pageName(int page) const
{
    return page >= 0 && page < m_pages.size() ? QFileInfo(m_pages[page].name).fileName() : QString();
}

QString ComicBook::pageFile(int page)
{
    QMutexLocker lock(&m_mutex);
    if (page < 0 || page >= m_pages.size())
        return {};
    Page &p = m_pages[page];
    if (!p.file.isEmpty() || p.zipIndex < 0)
        return p.file;

    const QByteArray data = m_zip.read(p.zipIndex);
    if (data.isEmpty()) {
        m_error = m_zip.error();
        return {};
    }
    QDir().mkpath(m_cacheDir);
    // The page number leads the name so nothing depends on what the archive called it (any characters, any length).
    const QString file = QStringLiteral("%1/%2.%3")
                             .arg(m_cacheDir)
                             .arg(page, 5, 10, QLatin1Char('0'))
                             .arg(QFileInfo(p.name).suffix().toLower());
    QFile out(file);
    if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size()) {
        m_error = core::tr("No se pudo guardar la página: %1").arg(out.errorString());
        return {};
    }
    out.close();
    p.file = file;
    return file;
}

QSize ComicBook::pageSize(int page)
{
    const QString file = pageFile(page);
    if (file.isEmpty())
        return {};
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return imageSizeFromHeader(f.read(256 * 1024));
}

} // namespace core
