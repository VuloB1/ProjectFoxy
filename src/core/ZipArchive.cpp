#include "ZipArchive.h"
#include "Translate.h"

#include <zlib.h>

namespace core {

namespace {

quint16 u16(const QByteArray &b, qsizetype at) { return quint16(quint8(b[at]) | quint8(b[at + 1]) << 8); }
quint32 u32(const QByteArray &b, qsizetype at) { return quint32(u16(b, at)) | quint32(u16(b, at + 2)) << 16; }
quint64 u64(const QByteArray &b, qsizetype at) { return quint64(u32(b, at)) | quint64(u32(b, at + 4)) << 32; }

constexpr quint32 kEndOfCentral = 0x06054b50;
constexpr quint32 kCentralHeader = 0x02014b50;
constexpr quint32 kLocalHeader = 0x04034b50;
constexpr quint32 kZip64Locator = 0x07064b50;
constexpr quint32 kZip64End = 0x06064b50;

} // namespace

bool ZipArchive::open(const QString &path)
{
    m_entries.clear();
    m_error.clear();
    m_file.close();
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::ReadOnly)) {
        m_error = core::tr("No se pudo abrir el archivo: %1").arg(m_file.errorString());
        return false;
    }
    const auto damaged = [this]() {
        m_error = core::tr("El archivo ZIP está dañado.");
        m_entries.clear();
        return false;
    };

    // The end-of-central-directory record is within the last 64 KB + 22 bytes (a comment can follow it).
    const qint64 fileSize = m_file.size();
    const qint64 tailSize = qMin<qint64>(fileSize, 0xFFFF + 22);
    m_file.seek(fileSize - tailSize);
    const QByteArray tail = m_file.read(tailSize);
    qsizetype eocd = -1;
    for (qsizetype i = tail.size() - 22; i >= 0; --i)
        if (u32(tail, i) == kEndOfCentral) {
            eocd = i;
            break;
        }
    if (eocd < 0) {
        m_error = core::tr("No es un archivo ZIP válido.");
        return false;
    }

    quint64 count = u16(tail, eocd + 10);
    quint64 centralSize = u32(tail, eocd + 12);
    quint64 centralOffset = u32(tail, eocd + 16);
    // ZIP64: the real numbers are in a record that the locator right before the end record points at.
    if (eocd >= 20 && u32(tail, eocd - 20) == kZip64Locator) {
        m_file.seek(qint64(u64(tail, eocd - 20 + 8)));
        const QByteArray z = m_file.read(56);
        if (z.size() == 56 && u32(z, 0) == kZip64End) {
            count = u64(z, 32);
            centralSize = u64(z, 40);
            centralOffset = u64(z, 48);
        }
    }
    if (centralOffset + centralSize > quint64(fileSize))
        return damaged();

    m_file.seek(qint64(centralOffset));
    const QByteArray central = m_file.read(qint64(centralSize));
    qsizetype p = 0;
    for (quint64 n = 0; n < count; ++n) {
        if (p + 46 > central.size() || u32(central, p) != kCentralHeader)
            return damaged();
        Entry e;
        e.flags = u16(central, p + 8);
        e.method = u16(central, p + 10);
        e.compressedSize = u32(central, p + 20);
        e.size = u32(central, p + 24);
        const qsizetype nameLength = u16(central, p + 28), extraLength = u16(central, p + 30),
                        commentLength = u16(central, p + 32);
        e.headerOffset = u32(central, p + 42);
        if (p + 46 + nameLength + extraLength + commentLength > central.size())
            return damaged();
        // Names are UTF-8 when flag 11 says so; older ones are CP437, whose ASCII part is the same. Anything that is
        // not valid UTF-8 is read as Latin-1 so the entry is at least listed.
        const QByteArray rawName = central.mid(p + 46, nameLength);
        e.name = QString::fromUtf8(rawName);
        if (e.name.contains(QChar::ReplacementCharacter))
            e.name = QString::fromLatin1(rawName);
        e.name.replace(QLatin1Char('\\'), QLatin1Char('/'));
        e.isDirectory = e.name.endsWith(QLatin1Char('/'));
        // The ZIP64 block of the extra field carries the values that did not fit in 32 bits, in this order and only
        // for the ones that are 0xFFFFFFFF.
        const QByteArray extra = central.mid(p + 46 + nameLength, extraLength);
        for (qsizetype x = 0; x + 4 <= extra.size();) {
            const quint16 id = u16(extra, x), len = u16(extra, x + 2);
            if (id == 1) {
                qsizetype at = x + 4;
                if (e.size == 0xFFFFFFFFu && at + 8 <= extra.size()) { e.size = u64(extra, at); at += 8; }
                if (e.compressedSize == 0xFFFFFFFFu && at + 8 <= extra.size()) { e.compressedSize = u64(extra, at); at += 8; }
                if (e.headerOffset == 0xFFFFFFFFu && at + 8 <= extra.size()) e.headerOffset = u64(extra, at);
            }
            x += 4 + len;
        }
        m_entries.append(e);
        p += 46 + nameLength + extraLength + commentLength;
    }
    return true;
}

void ZipArchive::close()
{
    m_file.close();
    m_entries.clear();
    m_error.clear();
}

QByteArray ZipArchive::read(int index)
{
    m_error.clear();
    if (index < 0 || index >= m_entries.size()) {
        m_error = core::tr("Ese archivo no está en el ZIP.");
        return {};
    }
    const Entry &e = m_entries.at(index);
    if (e.flags & 1) {
        m_error = core::tr("El archivo ZIP está protegido con contraseña.");
        return {};
    }
    if ((e.method != 0 && e.method != 8) || e.size > (quint64(1) << 31)) {
        m_error = core::tr("El ZIP usa una compresión que no se puede leer.");
        return {};
    }
    m_file.seek(qint64(e.headerOffset));
    const QByteArray local = m_file.read(30);
    if (local.size() != 30 || u32(local, 0) != kLocalHeader) {
        m_error = core::tr("El archivo ZIP está dañado.");
        return {};
    }
    m_file.seek(qint64(e.headerOffset) + 30 + u16(local, 26) + u16(local, 28));
    const QByteArray packed = m_file.read(qint64(e.compressedSize));
    if (quint64(packed.size()) != e.compressedSize) {
        m_error = core::tr("El archivo ZIP está dañado.");
        return {};
    }
    if (e.method == 0)
        return packed;

    QByteArray out;
    out.resize(qsizetype(e.size));
    z_stream s{};
    if (inflateInit2(&s, -MAX_WBITS) != Z_OK) {
        m_error = core::tr("El ZIP usa una compresión que no se puede leer.");
        return {};
    }
    s.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(packed.constData()));
    s.avail_in = uInt(packed.size());
    s.next_out = reinterpret_cast<Bytef *>(out.data());
    s.avail_out = uInt(out.size());
    const int status = inflate(&s, Z_FINISH);
    inflateEnd(&s);
    if (status != Z_STREAM_END || s.total_out != e.size) {
        m_error = core::tr("El archivo ZIP está dañado.");
        return {};
    }
    return out;
}

} // namespace core
