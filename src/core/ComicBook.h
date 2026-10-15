#pragma once

#include "ZipArchive.h"

#include <QMutex>
#include <memory>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

class QPdfDocument;

namespace core {

// A book made of pictures: the images of a folder, of a .cbz/.zip (in natural order: page2 before page10) or the pages
// of a PDF. The reader works with pages by number and asks for a real file for each one: for a folder that is the
// picture itself, for an archive the page is unpacked into `cacheDir` the first time it is wanted, and for a PDF it is
// drawn there as a JPEG (so the ordinary decoders can read all of them). Safe to use from several threads.
class ComicBook {
public:
    ComicBook(); // (out of line: QPdfDocument is only declared here)
    ~ComicBook();

    // A file that is a book by itself (.cbz, .zip or .pdf; decided by its name only).
    static bool isBookFile(const QString &path);
    // This build can read PDF files (Qt PDF was found when it was compiled).
    static bool canReadPdf();

    // `path` is a folder, an archive or a picture (its folder is the book). `cacheDir` is where an archive's pages
    // are unpacked; it is created when needed and removed by close(). False (error() says why) when there is no
    // picture to read.
    bool open(const QString &path, const QString &cacheDir);
    void close();

    int pageCount() const { return int(m_pages.size()); }
    QString title() const { return m_title; }
    QString error() const { return m_error; }
    // The page of the picture `path` when the book is its folder (the page the reader starts on); 0 otherwise.
    int pageOfFile(const QString &path) const;
    QString pageName(int page) const;

    // The file that holds page `page`, unpacked if it has to be; empty (and error() set) when it cannot be read.
    QString pageFile(int page);
    // The pixel size of the page from its first bytes; invalid when unknown (see imageSizeFromHeader).
    QSize pageSize(int page);

private:
    struct Page {
        QString name;
        QString file;     // folder: the picture; archive: where it is unpacked (empty until then)
        int zipIndex = -1;
    };

    QMutex m_mutex;
    ZipArchive m_zip;
    std::unique_ptr<QPdfDocument> m_pdf; // set when the book is a PDF
    QVector<Page> m_pages;
    QString m_title;
    QString m_cacheDir;
    QString m_error;
};

} // namespace core
