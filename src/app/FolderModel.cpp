#include "FolderModel.h"
#include "DecoderRegistry.h"
#include "AppPaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCollator>
#include <QUrl>
#include <QRandomGenerator>
#include <QSet>
#include <algorithm>
#include <numeric>
#include "Translate.h"

FolderModel::FolderModel(QObject *parent)
    : QAbstractListModel(parent)
{
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

void FolderModel::openFolderForFile(const QString &filePath)
{
    const QFileInfo info(filePath);
    const QDir dir = info.dir();

    // Moving between pictures of the same folder (every arrow key press) must not list, sort and
    // reset the whole model again: that costs hundreds of ms on a big folder, on the UI thread,
    // and rebuilds the filmstrip. The listing is only redone when the folder itself changed
    // (a file added, removed or renamed moves its modification time) or the file is not in it.
    const QString dirPath = dir.absolutePath();
    const QDateTime modified = QFileInfo(dirPath).lastModified();
    const int known = m_files.indexOf(info.absoluteFilePath());
    if (known >= 0 && dirPath == m_scannedDir && modified.isValid() && modified == m_scannedModified) {
        setCurrentIndexInternal(known, /*announceFilePath=*/false);
        return;
    }

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

    m_scannedDir = dirPath;
    m_scannedModified = modified;
    beginResetModel();
    m_files.clear();
    m_files.reserve(entries.size());
    for (const auto &entry : entries)
        m_files << entry.absoluteFilePath();
    endResetModel();
    emit countChanged();

    const QString absolutePath = info.absoluteFilePath();
    int index = m_files.indexOf(absolutePath);
    if (index < 0 && !m_files.isEmpty())
        index = 0; // opened file wasn't in the scanned list (unsupported ext, etc.)

    setCurrentIndexInternal(index, /*announceFilePath=*/false);
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

void FolderModel::regenerateShuffleOrder()
{
    m_shuffleOrder.resize(m_files.size());
    std::iota(m_shuffleOrder.begin(), m_shuffleOrder.end(), 0);
    std::shuffle(m_shuffleOrder.begin(), m_shuffleOrder.end(), *QRandomGenerator::global());
    m_shufflePos = -1;
}

bool FolderModel::advanceSlideshow(bool random, bool loop)
{
    if (m_files.isEmpty())
        return false;

    if (!random) {
        if (m_currentIndex + 1 >= m_files.size()) {
            if (!loop)
                return false;
            setCurrentIndex(0);
            return true;
        }
        setCurrentIndex(m_currentIndex + 1);
        return true;
    }

    if (m_shuffleOrder.size() != m_files.size())
        regenerateShuffleOrder();

    ++m_shufflePos;
    if (m_shufflePos >= m_shuffleOrder.size()) {
        if (!loop) {
            m_shufflePos = m_shuffleOrder.size() - 1;
            return false;
        }
        regenerateShuffleOrder();
        m_shufflePos = 0;
    }
    setCurrentIndex(m_shuffleOrder[m_shufflePos]);
    return true;
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
