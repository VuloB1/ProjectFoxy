#include "FolderModel.h"
#include "DecoderRegistry.h"
#include "AppPaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCollator>
#include <QMetaObject>
#include <QRunnable>
#include <QUrl>
#include <QSet>
#include <algorithm>
#include "Translate.h"

FolderModel::FolderModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_scanPool.setMaxThreadCount(1);
    m_rescanTimer.setSingleShot(true);
    m_rescanTimer.setInterval(400);
    connect(&m_rescanTimer, &QTimer::timeout, this, &FolderModel::rescanCurrentFolder);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_rescanTimer, qOverload<>(&QTimer::start));
    // m_thumbnailCache is a plain value member (not parented to `this`):
    // its lifetime is already tied to FolderModel via normal C++ member
    // destruction order, so it doesn't need Qt's parent/child ownership too.
}

int FolderModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_files.size();
}

QVariant FolderModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_files.size())
        return {};

    const QString &path = m_files.at(index.row());
    switch (role) {
    case FilePathRole:
        return path;
    case FileNameRole:
        return QFileInfo(path).fileName();
    case ThumbnailSourceRole:
        return QStringLiteral("image://thumb/%1")
            .arg(QString::fromUtf8(QUrl::toPercentEncoding(path)));
    case IsCurrentRole:
        return index.row() == m_currentIndex;
    default:
        return {};
    }
}

QHash<int, QByteArray> FolderModel::roleNames() const
{
    return {
        { FilePathRole, "filePath" },
        { FileNameRole, "fileName" },
        { ThumbnailSourceRole, "thumbnailSource" },
        { IsCurrentRole, "isCurrent" },
    };
}

namespace {

// Lists, filters and sorts the folder of `filePath`. Touches nothing shared, so it can run on a worker.
FolderModel::Scan scanFolder(const QString &filePath)
{
    FolderModel::Scan scan;
    const QDir dir = QFileInfo(filePath).dir();
    scan.dir = dir.absolutePath();
    // Read before listing: a change made during the listing then shows up as "modified" the next time.
    scan.modified = QFileInfo(scan.dir).lastModified();

    // Matched by hand, ignoring case: a QDir name filter is case-sensitive on Linux, where "FOTO.JPG" must be found too.
    QSet<QString> extensions;
    for (const auto &ext : core::DecoderRegistry::instance().allSupportedExtensions())
        extensions.insert(QString(ext).toLower());

    QFileInfoList entries = dir.entryInfoList(QDir::Files);
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                 [&extensions](const QFileInfo &e) { return !extensions.contains(e.suffix().toLower()); }),
                  entries.end());

    // Natural sort (img2 before img10) instead of plain lexicographic order.
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(entries.begin(), entries.end(), [&collator](const QFileInfo &a, const QFileInfo &b) {
        return collator.compare(a.fileName(), b.fileName()) < 0;
    });

    scan.files.reserve(entries.size());
    for (const auto &entry : entries)
        scan.files << entry.absoluteFilePath();
    return scan;
}

} // namespace

FolderModel::~FolderModel()
{
    ++m_scanGeneration; // whatever a worker is still listing is not wanted
    m_scanPool.waitForDone(); // it posts its result to this object
}

bool FolderModel::selectIfFolderUnchanged(const QString &filePath)
{
    // Moving between pictures of the same folder (every arrow key press) must not list, sort and
    // reset the whole model again: that costs hundreds of ms on a big folder and rebuilds the
    // filmstrip. The listing is only redone when the folder itself changed (a file added,
    // removed or renamed moves its modification time) or the file is not in it.
    const QFileInfo info(filePath);
    const QString dirPath = info.dir().absolutePath();
    const QDateTime modified = QFileInfo(dirPath).lastModified();
    const int known = m_files.indexOf(info.absoluteFilePath());
    if (known < 0 || dirPath != m_scannedDir || !modified.isValid() || modified != m_scannedModified)
        return false;
    setCurrentIndexInternal(known, /*announceFilePath=*/false);
    return true;
}

void FolderModel::applyScan(const Scan &scan, const QString &filePath, int fallbackIndex)
{
    m_scannedDir = scan.dir;
    m_scannedModified = scan.modified;
    // Watch the folder just listed (one at a time). Where the file system cannot be watched (some network shares)
    // addPath fails and the folder's modification time, checked on every move, still catches changes.
    if (m_watcher.directories() != QStringList{scan.dir}) {
        if (!m_watcher.directories().isEmpty())
            m_watcher.removePaths(m_watcher.directories());
        m_watcher.addPath(scan.dir);
    }
    // The same list again (a rescan that found nothing new) changes nothing: no reset, so the strip keeps its place.
    if (scan.files != m_files) {
        beginResetModel();
        m_files = scan.files;
        endResetModel();
        emit countChanged();
    }

    int index = m_files.indexOf(QFileInfo(filePath).absoluteFilePath());
    if (index < 0 && !m_files.isEmpty())
        index = qBound(0, fallbackIndex, int(m_files.size()) - 1); // not in the list (unsupported ext, deleted...)

    setCurrentIndexInternal(index, /*announceFilePath=*/false);
}

void FolderModel::openFolderForFile(const QString &filePath)
{
    ++m_scanGeneration; // an asynchronous listing still under way is now out of date
    if (selectIfFolderUnchanged(filePath))
        return;
    applyScan(scanFolder(filePath), filePath);
}

void FolderModel::openFolderForFileAsync(const QString &filePath)
{
    startScan(filePath, 0);
}

void FolderModel::startScan(const QString &filePath, int fallbackIndex)
{
    const quint64 generation = ++m_scanGeneration;
    if (selectIfFolderUnchanged(filePath))
        return;
    // A big or remote folder can take long to list: it is done on a worker and the strip fills in when it is ready.
    // The newest request wins; an older one that finishes later is dropped.
    m_scanPool.start(QRunnable::create([this, filePath, generation, fallbackIndex]() {
        const Scan scan = scanFolder(filePath);
        QMetaObject::invokeMethod(this, [this, scan, filePath, generation, fallbackIndex]() {
            if (generation == m_scanGeneration)
                applyScan(scan, filePath, fallbackIndex);
        }, Qt::QueuedConnection);
    }));
}

// Someone else added, removed or renamed a file in the folder on show: list it again, keeping the picture on show
// (or, if that one is gone, the position it had).
void FolderModel::rescanCurrentFolder()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_files.size())
        return;
    startScan(m_files.at(m_currentIndex), m_currentIndex);
}

void FolderModel::setCurrentIndex(int index)
{
    setCurrentIndexInternal(index, /*announceFilePath=*/true);
}

void FolderModel::next()
{
    if (m_files.isEmpty())
        return;
    setCurrentIndex((m_currentIndex + 1) % m_files.size());
}

void FolderModel::previous()
{
    if (m_files.isEmpty())
        return;
    setCurrentIndex((m_currentIndex - 1 + m_files.size()) % m_files.size());
}

namespace {

QString samePathKey(const QString &path)
{
    return AppPaths::samePathKey(path);
}

} // namespace

QVariantMap FolderModel::checkRename(const QStringList &paths, const QString &baseName, int startNumber,
                                     int padding) const
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("names"), QStringList());

    auto fail = [&result](const QString &message) {
        result.insert(QStringLiteral("error"), message);
        return result;
    };

    if (paths.isEmpty())
        return fail(core::tr("No hay archivos elegidos."));
    if (baseName.trimmed().isEmpty())
        return fail(core::tr("Escribe el nombre base."));
    static const QString illegal = QStringLiteral("<>:\"/\\|?*");
    for (const QChar c : baseName) {
        if (illegal.contains(c) || c.unicode() < 32)
            return fail(core::tr("El nombre no puede llevar los caracteres  < > : \" / \\ | ? *"));
    }
    if (baseName.endsWith(QLatin1Char(' ')) || baseName.endsWith(QLatin1Char('.')))
        return fail(core::tr("El nombre no puede terminar en espacio ni en punto."));

    QSet<QString> chosen;
    for (const QString &p : paths)
        chosen.insert(samePathKey(p));

    QStringList names;
    int number = startNumber;
    for (const QString &path : paths) {
        const QFileInfo info(path);
        const QString name = QStringLiteral("%1%2.%3")
            .arg(baseName, QString::number(number).rightJustified(qMax(1, padding), QLatin1Char('0')), info.suffix());
        const QString target = info.dir().filePath(name);
        if (QFile::exists(target) && !chosen.contains(samePathKey(target)))
            return fail(QStringLiteral("«%1» ya existe en la carpeta y no está entre los archivos elegidos.").arg(name));
        names << name;
        ++number;
    }
    result.insert(QStringLiteral("names"), names);
    result.insert(QStringLiteral("ok"), true);
    return result;
}

QVariantMap FolderModel::renameFiles(const QStringList &paths, const QString &baseName, int startNumber, int padding)
{
    QVariantMap check = checkRename(paths, baseName, startNumber, padding);
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("renamed"), 0);
    if (!check.value(QStringLiteral("ok")).toBool()) {
        result.insert(QStringLiteral("error"), check.value(QStringLiteral("error")));
        return result;
    }
    const QStringList names = check.value(QStringLiteral("names")).toStringList();

    // Phase 1: everything to a temporary name. Undone completely if one fails.
    QStringList temps;
    auto putBack = [&](int movedToTemp, int movedToFinal, const QStringList &finals) {
        for (int i = movedToFinal - 1; i >= 0; --i)
            QFile::rename(finals.at(i), temps.at(i));
        for (int i = movedToTemp - 1; i >= 0; --i)
            QFile::rename(temps.at(i), paths.at(i));
    };
    for (int i = 0; i < paths.size(); ++i) {
        QString temp = paths.at(i) + QStringLiteral(".renaming_tmp");
        for (int n = 2; QFile::exists(temp); ++n)
            temp = paths.at(i) + QStringLiteral(".renaming_tmp%1").arg(n);
        if (!QFile::rename(paths.at(i), temp)) {
            putBack(i, 0, {});
            result.insert(QStringLiteral("error"),
                          core::tr("No se pudo renombrar «%1» (¿está abierto en otro programa?). No se cambió nada.")
                              .arg(QFileInfo(paths.at(i)).fileName()));
            return result;
        }
        temps << temp;
    }

    // Phase 2: temporary -> final.
    QStringList finals;
    for (int i = 0; i < paths.size(); ++i) {
        const QString target = QFileInfo(paths.at(i)).dir().filePath(names.at(i));
        if (!QFile::rename(temps.at(i), target)) {
            putBack(paths.size(), i, finals);
            result.insert(QStringLiteral("error"),
                          core::tr("No se pudo crear «%1». No se cambió nada.").arg(names.at(i)));
            return result;
        }
        finals << target;
    }

    // The folder listing (and the open picture, if it was one of them) follow the new names.
    const QString currentOld = (m_currentIndex >= 0 && m_currentIndex < m_files.size()) ? m_files.at(m_currentIndex) : QString();
    QString currentNew = currentOld;
    for (int i = 0; i < paths.size(); ++i) {
        if (samePathKey(paths.at(i)) == samePathKey(currentOld))
            currentNew = finals.at(i);
    }
    if (!currentNew.isEmpty())
        openFolderForFile(currentNew);
    if (currentNew != currentOld)
        emit currentFilePathChanged(currentNew); // same wiring as navigation: AppController reopens it

    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("renamed"), paths.size());
    return result;
}

void FolderModel::setCurrentIndexInternal(int index, bool announceFilePath)
{
    if (index < 0 || index >= m_files.size() || index == m_currentIndex)
        return;

    const int previous = m_currentIndex;
    m_currentIndex = index;

    if (previous >= 0)
        emit dataChanged(this->index(previous), this->index(previous), { IsCurrentRole });
    emit dataChanged(this->index(m_currentIndex), this->index(m_currentIndex), { IsCurrentRole });
    emit currentIndexChanged();

    if (announceFilePath)
        emit currentFilePathChanged(m_files.at(m_currentIndex));
}
