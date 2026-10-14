#pragma once

#include <QFile>
#include <QString>
#include <QVector>

namespace core {

// Read-only access to a ZIP file (what a .cbz is): the list of its entries and the bytes of one of them. Only what a
// comic needs: stored and deflated entries, ZIP64 sizes and UTF-8 names. Encrypted or otherwise compressed entries
// report an error when read. Not thread-safe: one reader, one thread at a time.
class ZipArchive {
public:
    struct Entry {
        QString name; // with '/' as separator
        quint64 compressedSize = 0;
        quint64 size = 0;
        quint64 headerOffset = 0;
        quint16 method = 0; // 0 stored, 8 deflate
        quint16 flags = 0;
        bool isDirectory = false;
    };

    bool open(const QString &path);
    const QVector<Entry> &entries() const { return m_entries; }
    // The whole entry, decompressed. Empty (and error() set) when it cannot be read.
    QByteArray read(int index);
    QString error() const { return m_error; }
    void close();

private:
    QFile m_file;
    QVector<Entry> m_entries;
    QString m_error;
};

} // namespace core
