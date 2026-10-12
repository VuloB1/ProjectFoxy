#pragma once

#include "ThumbnailCache.h"
#include <QAbstractListModel>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QThreadPool>
#include <QStringList>
#include <QVariantMap>

// List of sibling image files in the folder of the currently opened image,
// sorted naturally by name. Backs the QML ThumbnailStrip and owns folder
// navigation (next/previous/click-to-select).
//
// Ownership split with AppController: AppController::loadPath() calls
// openFolderForFile() to (re)populate the list and silently sync
// currentIndex whenever the *app* opens a file directly (dialog, drag&drop,
// file association). Navigation methods called from QML (next/previous/
// setCurrentIndex) instead emit currentFilePathChanged, which main.cpp wires
// back into AppController::openPath() to actually decode and display that
// file. This one-way split avoids the two objects re-triggering each other
// in a loop.
class FolderModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        FilePathRole = Qt::UserRole + 1,
        FileNameRole,
        ThumbnailSourceRole,
        IsCurrentRole,
    };
    Q_ENUM(Roles)

    explicit FolderModel(QObject *parent = nullptr);
    ~FolderModel() override;

    // One folder listing: the files (absolute paths, naturally sorted) and the folder's modification time when it was read.
    struct Scan {
        QString dir;
        QDateTime modified;
        QStringList files;
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int currentIndex() const { return m_currentIndex; }
    int count() const { return m_files.size(); }

    core::ThumbnailCache *thumbnailCache() { return &m_thumbnailCache; }

    // Absolute paths of every file in the current folder, naturally sorted -
    // the source list for "batch export" (which converts/resizes all of
    // them) in QML.
    Q_INVOKABLE QStringList filePaths() const { return m_files; }

public slots:
    // Scans the folder containing filePath, sorts it, and sets currentIndex
    // to filePath's position - without emitting currentFilePathChanged
    // (the caller already knows what it opened).
    void openFolderForFile(const QString &filePath);
    // The same, without waiting for a listing that has to be redone: it runs on a worker and the model changes when it
    // arrives (the newest call wins). Moving within an already listed, unchanged folder is still immediate.
    void openFolderForFileAsync(const QString &filePath);

    // Navigation entry points for QML: these DO emit currentFilePathChanged.
    void setCurrentIndex(int index);
    void next();
    void previous();

    // Renames the chosen files (in the order given) to "<baseName><NNN>.<ext>" - original
    // extension kept - numbering from startNumber, zero-padded to `padding` digits.
    //
    // checkRename() only works out the new names and whether the rename would be refused
    // ({ ok, names: [new file names, same order], error }): an empty or illegal base name,
    // or a new name that is already taken by a file that is NOT among the chosen ones.
    // renameFiles() does it, all or nothing: two phases (temporary names first, so a new
    // name that equals another chosen file's OLD name cannot overwrite it), and if any
    // step fails everything already moved is put back. Returns { ok, renamed, error }.
    Q_INVOKABLE QVariantMap checkRename(const QStringList &paths, const QString &baseName, int startNumber,
                                        int padding) const;
    Q_INVOKABLE QVariantMap renameFiles(const QStringList &paths, const QString &baseName, int startNumber,
                                        int padding);

signals:
    void currentIndexChanged();
    void countChanged();
    void currentFilePathChanged(const QString &filePath);

private:
    void setCurrentIndexInternal(int index, bool announceFilePath);
    bool selectIfFolderUnchanged(const QString &filePath);
    void applyScan(const Scan &scan, const QString &filePath, int fallbackIndex = 0);
    void startScan(const QString &filePath, int fallbackIndex);
    void rescanCurrentFolder();

    QStringList m_files; // absolute paths, naturally sorted
    QString m_scannedDir;          // folder m_files was listed from
    QDateTime m_scannedModified;   // that folder's modification time at the time of the listing
    quint64 m_scanGeneration = 0;  // bumped by every request; a worker's result counts only if it is still the latest
    QFileSystemWatcher m_watcher;  // the listed folder: a change made by someone else refreshes the strip
    QTimer m_rescanTimer;          // waits for a burst of changes (a copy in progress) to settle
    QThreadPool m_scanPool;        // one listing at a time; waited for in the destructor (its result is posted to this)
    int m_currentIndex = -1;
    core::ThumbnailCache m_thumbnailCache;
};
