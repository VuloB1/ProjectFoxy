#include "ComicBook.h"
#include "ImageHeader.h"
#include "SpreadLayout.h"
#include "ZipArchive.h"

#include <QBuffer>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>
#include <zlib.h>

// The pieces of the comic reader that do not need a window: reading a .cbz, finding the pages and their shapes, and
// laying out single and double pages.
namespace {

QByteArray picture(int w, int h, const char *format)
{
    QImage image(w, h, QImage::Format_RGB32);
    image.fill(Qt::darkCyan);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, format);
    return bytes;
}

void put16(QByteArray &b, quint16 v) { b.append(char(v & 0xFF)).append(char(v >> 8)); }
void put32(QByteArray &b, quint32 v) { put16(b, quint16(v & 0xFFFF)); put16(b, quint16(v >> 16)); }

// A ZIP file made by hand: `deflated` entries are compressed, the others stored. (No data descriptors, no ZIP64.)
struct Entry { QString name; QByteArray data; bool deflated = false; };
QByteArray makeZip(const QVector<Entry> &entries)
{
    QByteArray out, central;
    for (const Entry &e : entries) {
        QByteArray packed = e.data;
        if (e.deflated) {
            z_stream s{};
            deflateInit2(&s, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
            packed.resize(int(deflateBound(&s, uLong(e.data.size()))));
            s.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(e.data.constData()));
            s.avail_in = uInt(e.data.size());
            s.next_out = reinterpret_cast<Bytef *>(packed.data());
            s.avail_out = uInt(packed.size());
            deflate(&s, Z_FINISH);
            packed.resize(int(s.total_out));
            deflateEnd(&s);
        }
        const quint32 crc = crc32(0, reinterpret_cast<const Bytef *>(e.data.constData()), uInt(e.data.size()));
        const QByteArray name = e.name.toUtf8();
        const quint32 offset = quint32(out.size());
        out.append("PK\x03\x04", 4); put16(out, 20); put16(out, 0x800); put16(out, e.deflated ? 8 : 0);
        put16(out, 0); put16(out, 0x21); put32(out, crc); put32(out, quint32(packed.size())); put32(out, quint32(e.data.size()));
        put16(out, quint16(name.size())); put16(out, 0); out.append(name).append(packed);
        central.append("PK\x01\x02", 4); put16(central, 20); put16(central, 20); put16(central, 0x800);
        put16(central, e.deflated ? 8 : 0); put16(central, 0); put16(central, 0x21); put32(central, crc);
        put32(central, quint32(packed.size())); put32(central, quint32(e.data.size())); put16(central, quint16(name.size()));
        put16(central, 0); put16(central, 0); put16(central, 0); put16(central, 0); put32(central, 0); put32(central, offset);
        central.append(name);
    }
    const quint32 centralOffset = quint32(out.size());
    out.append(central);
    out.append("PK\x05\x06", 4); put16(out, 0); put16(out, 0); put16(out, quint16(entries.size())); put16(out, quint16(entries.size()));
    put32(out, quint32(central.size())); put32(out, centralOffset); put16(out, 0);
    return out;
}

QString writeFile(const QString &path, const QByteArray &bytes)
{
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(bytes);
    return path;
}

} // namespace

class TestComicBook : public QObject {
    Q_OBJECT

private slots:
    void theSizeOfAPictureIsReadFromItsFirstBytes()
    {
        QCOMPARE(core::imageSizeFromHeader(picture(120, 80, "PNG")), QSize(120, 80));
        QCOMPARE(core::imageSizeFromHeader(picture(321, 457, "JPG")), QSize(321, 457));
        // WebP: the three ways the size is written (lossy, lossless, extended); only the header bytes matter here.
        QByteArray lossy("RIFF\0\0\0\0WEBPVP8 \0\0\0\0\0\0\0\0\0\0\0\0\0\0", 30);
        lossy[26] = char(0x2C); lossy[27] = char(0x01); lossy[28] = char(0x90); lossy[29] = char(0x01); // 300 x 400
        QCOMPARE(core::imageSizeFromHeader(lossy), QSize(300, 400));
        QByteArray extended("RIFF\0\0\0\0WEBPVP8X\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 30);
        extended[24] = char(0xFF); extended[25] = char(0x03); extended[27] = char(0xE7); extended[28] = char(0x03); // 1024 x 1000
        QCOMPARE(core::imageSizeFromHeader(extended), QSize(1024, 1000));
        QVERIFY(!core::imageSizeFromHeader("not a picture at all, just some bytes").isValid());
        QVERIFY(!core::imageSizeFromHeader(QByteArray()).isValid());
    }

    void singlePagesAreOneByOne()
    {
        const QVector<QSize> sizes(5, QSize(100, 150));
        const auto spreads = core::buildSpreads(sizes, false, true);
        QCOMPARE(spreads.size(), 5);
        QCOMPARE(spreads[3].first, 3);
        QCOMPARE(spreads[3].count, 1);
    }

    void doublePagesPairUpAfterTheCoverAndWidePagesStandAlone()
    {
        QVector<QSize> sizes(8, QSize(100, 150));
        // cover | 1 2 | 3 4 | 5 6 | 7
        auto spreads = core::buildSpreads(sizes, true, true);
        QCOMPARE(spreads.size(), 5);
        QCOMPARE(spreads[0].count, 1);
        QCOMPARE(spreads[1].first, 1);
        QCOMPARE(spreads[1].count, 2);
        QCOMPARE(spreads[4].first, 7);
        QCOMPARE(spreads[4].count, 1);
        // without the cover alone: 0 1 | 2 3 | 4 5 | 6 7
        spreads = core::buildSpreads(sizes, true, false);
        QCOMPARE(spreads.size(), 4);
        QCOMPARE(spreads[0].count, 2);
        // a landscape page in the middle stands alone and the pairing starts again after it: 0 1 | 2 | 3 4 | 5 6 | 7
        sizes[2] = QSize(300, 150);
        spreads = core::buildSpreads(sizes, true, false);
        QCOMPARE(spreads.size(), 5);
        QCOMPARE(spreads[1].first, 2);
        QCOMPARE(spreads[1].count, 1);
        QCOMPARE(spreads[2].first, 3);
        QCOMPARE(spreads[2].count, 2);
        // every page is in exactly one spread, in order
        int next = 0;
        for (const auto &s : spreads) {
            QCOMPARE(s.first, next);
            next += s.count;
        }
        QCOMPARE(next, 8);
        QCOMPARE(core::spreadOfPage(spreads, 4), 2);
    }

    void aCbzIsReadInNaturalOrderAndItsPagesAreUnpacked()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QByteArray p1 = picture(60, 90, "PNG"), p2 = picture(70, 100, "PNG"), p10 = picture(80, 110, "JPG");
        const QString zip = writeFile(dir.filePath("Mi serie 01.cbz"),
                                      makeZip({{"cap/10.jpg", p10, true},
                                               {"cap/2.png", p2, false},
                                               {"cap/", {}, false},
                                               {"__MACOSX/cap/._2.png", "junk", false},
                                               {"cap/notes.txt", "hello", false},
                                               {"cap/1.png", p1, true}}));
        QVERIFY(core::ComicBook::isArchivePath(zip));
        QVERIFY(!core::ComicBook::isArchivePath(dir.filePath("photo.jpg")));

        core::ComicBook book;
        QVERIFY2(book.open(zip, dir.filePath("cache")), qPrintable(book.error()));
        QCOMPARE(book.pageCount(), 3); // 1, 2, 10: not 1, 10, 2; the junk and the text file are not pages
        QCOMPARE(book.pageName(0), QString("1.png"));
        QCOMPARE(book.pageName(1), QString("2.png"));
        QCOMPARE(book.pageName(2), QString("10.jpg"));
        QCOMPARE(book.title(), QString("Mi serie 01"));
        QCOMPARE(book.pageSize(0), QSize(60, 90));
        QCOMPARE(book.pageSize(2), QSize(80, 110));

        const QString file = book.pageFile(1);
        QVERIFY(QFile::exists(file));
        QImage decoded(file);
        QCOMPARE(decoded.size(), QSize(70, 100));
        QCOMPARE(book.pageFile(1), file); // unpacked once

        book.close();
        QVERIFY(!QDir(dir.filePath("cache")).exists()); // the unpacked pages go with the book
    }

    void aFolderIsABookAndAPictureOpensItsFolderAtThatPage()
    {
        QTemporaryDir dir;
        for (const char *name : {"page 2.png", "page 10.png", "page 1.png", "readme.txt"})
            writeFile(dir.filePath(name), QByteArray(name).endsWith(".png") ? picture(40, 60, "PNG") : QByteArray("x"));
        core::ComicBook book;
        QVERIFY(book.open(dir.filePath("page 10.png"), QString()));
        QCOMPARE(book.pageCount(), 3);
        QCOMPARE(book.pageName(2), QString("page 10.png"));
        QCOMPARE(book.pageOfFile(dir.filePath("page 10.png")), 2);
        QCOMPARE(book.pageFile(0), QDir(dir.path()).absoluteFilePath("page 1.png")); // used where it is, not copied
        book.close();
        QVERIFY(QFile::exists(dir.filePath("page 1.png"))); // closing a folder book touches nothing
    }

    void whatCannotBeReadIsReportedNotCrashed()
    {
        QTemporaryDir dir;
        core::ComicBook book;
        QVERIFY(!book.open(writeFile(dir.filePath("broken.cbz"), "this is not a zip"), dir.filePath("c")));
        QVERIFY(!book.error().isEmpty());
        QVERIFY(!book.open(dir.path(), QString())); // an empty folder
        QVERIFY(!book.error().isEmpty());
        // an archive with no pictures
        QVERIFY(!book.open(writeFile(dir.filePath("text.zip"), makeZip({{"a.txt", "hi", false}})), dir.filePath("c")));
        // an entry cut short is an error, not garbage
        core::ZipArchive zip;
        const QString damaged = writeFile(dir.filePath("damaged.zip"), makeZip({{"a.bin", QByteArray(5000, 'x'), true}}).left(60));
        QVERIFY(!zip.open(damaged));
    }
};

QTEST_MAIN(TestComicBook)
#include "test_comic_book.moc"
