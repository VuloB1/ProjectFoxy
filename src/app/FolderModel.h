#pragma once

#include "ThumbnailCache.h"
#include <QAbstractListModel>
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

    // Navigation entry points for QML: these DO emit currentFilePathChanged.
    void setCurrentIndex(int index);
    void next();
    void previous();

    // Single entry point for the slideshow timer. Sequential mode just
    // wraps like next() unless `loop` is false, in which case it stops
    // advancing (returns false) after the last file. Random mode visits a
    // shuffled permutation of the whole folder without repeats, reshuffling
    // once exhausted (or stopping there too if `loop` is false). Returns
    // false when the slideshow should stop (QML unchecks its own toggle).
    Q_INVOKABLE bool advanceSlideshow(bool random, bool loop);

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
    void regenerateShuffleOrder();

    QStringList m_files; // absolute paths, naturally sorted
    int m_currentIndex = -1;
    core::ThumbnailCache m_thumbnailCache;

    // Slideshow "random, no repeat until exhausted" state.
    QVector<int> m_shuffleOrder;
    int m_shufflePos = -1;
};
