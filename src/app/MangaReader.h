#pragma once

#include "ComicBook.h"
#include "SpreadLayout.h"

#include <QObject>
#include <QSettings>
#include <QThreadPool>
#include <QTimer>
#include <QVariantList>
#include <atomic>

class PaneImageStore;

// The manga / comic reader (qml/MangaReader.qml): a book of pictures (a folder, or a .cbz/.zip) read one spread at a
// time, right to left or left to right, with one page or two. It only decides WHAT is shown (the files of the pages on
// screen and where the reader is); the pictures are decoded by the same "pane" image provider the compare view uses.
//
// The shape of every page is read in the background from the first bytes of the file, so that double-page layouts know
// which pages are wide (a landscape page is a spread already and stands alone). Until a page's shape is known it counts
// as an ordinary portrait page, and the layout settles as the shapes arrive.
class MangaReader : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QString title READ title NOTIFY activeChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY activeChanged)
    // The first page on screen (0-based) and the last one: with a double page they differ.
    Q_PROPERTY(int currentPage READ currentPage NOTIFY positionChanged)
    Q_PROPERTY(int lastPage READ lastPage NOTIFY positionChanged)
    // What is on screen, from left to right: [{ path, width, height, page }]. Two entries for a double page.
    Q_PROPERTY(QVariantList pages READ pages NOTIFY positionChanged)
    Q_PROPERTY(bool atStart READ atStart NOTIFY positionChanged)
    Q_PROPERTY(bool atEnd READ atEnd NOTIFY positionChanged)
    // The reading settings; each is remembered between runs.
    Q_PROPERTY(bool rightToLeft READ rightToLeft WRITE setRightToLeft NOTIFY settingsChanged)
    Q_PROPERTY(bool doublePage READ doublePage WRITE setDoublePage NOTIFY settingsChanged)
    Q_PROPERTY(bool coverAlone READ coverAlone WRITE setCoverAlone NOTIFY settingsChanged)
    // "screen" (the whole spread fits), "height" or "width" (the spread is as wide as the view and scrolls).
    Q_PROPERTY(QString fit READ fit WRITE setFit NOTIFY settingsChanged)
    // The longest side, in pixels, the pages are decoded at: the view sets it from its size so that what is
    // prefetched is what the view asks for.
    Q_PROPERTY(int pageSide READ pageSide WRITE setPageSide NOTIFY pageSideChanged)

public:
    explicit MangaReader(PaneImageStore *store, QObject *parent = nullptr);
    ~MangaReader() override;

    // `path`: a folder, a .cbz/.zip, or a picture (then its folder is the book and it is the first page shown). An
    // archive or folder opened as such goes back to the page it was left on.
    Q_INVOKABLE bool open(const QString &path);
    Q_INVOKABLE void close();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void first();
    Q_INVOKABLE void last();
    Q_INVOKABLE void goToPage(int page);
    Q_INVOKABLE static bool isArchive(const QString &path);

    bool active() const { return m_active; }
    QString title() const { return m_book.title(); }
    QString error() const { return m_error; }
    int pageCount() const { return m_active ? m_book.pageCount() : 0; }
    int currentPage() const;
    int lastPage() const;
    QVariantList pages();
    bool atStart() const { return m_spread <= 0; }
    bool atEnd() const { return m_spread >= m_spreads.size() - 1; }
    bool rightToLeft() const { return m_rightToLeft; }
    bool doublePage() const { return m_doublePage; }
    bool coverAlone() const { return m_coverAlone; }
    QString fit() const { return m_fit; }
    int pageSide() const { return m_pageSide; }
    void setRightToLeft(bool on);
    void setDoublePage(bool on);
    void setCoverAlone(bool on);
    void setFit(const QString &fit);
    void setPageSide(int side);

signals:
    void activeChanged();
    void errorChanged();
    void positionChanged();
    void settingsChanged();
    void pageSideChanged();

private:
    void relayout(); // the spreads from what is known of the pages, keeping the page on screen
    void showPage(int page);
    void scanShapes();
    void prefetch();
    void remember();
    QString bookKey() const;

    core::ComicBook m_book;
    PaneImageStore *m_store;
    QThreadPool m_scanner;    // reads the shape of every page, one at a time
    QThreadPool m_prefetcher; // unpacks and decodes the pages that come next
    QVector<QSize> m_sizes;
    QVector<core::Spread> m_spreads;
    QString m_path;
    QString m_error;
    QSettings m_store_settings;
    QTimer m_relayoutTimer;
    std::atomic<int> m_generation{0};
    bool m_active = false;
    bool m_scanned = false; // the shapes of this book are being (or have been) read
    int m_spread = 0;
    int m_page = 0; // the first page on screen; the spreads can change under it
    bool m_rightToLeft = true;
    bool m_doublePage = false;
    bool m_coverAlone = true;
    QString m_fit = QStringLiteral("screen");
    int m_pageSide = 2048;
};
