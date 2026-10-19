#include "VideoSupport.h"

#include <QMediaFormat>

QStringList VideoSupport::extensions() const
{
    if (m_known)
        return m_extensions;
    m_known = true;

    struct Container {
        QMediaFormat::FileFormat format;
        const char *extensions;
    };
    static constexpr Container kContainers[] = {
        {QMediaFormat::MPEG4, "mp4 m4v"},      {QMediaFormat::QuickTime, "mov"}, {QMediaFormat::WebM, "webm"},
        {QMediaFormat::Matroska, "mkv"},       {QMediaFormat::AVI, "avi"},       {QMediaFormat::Ogg, "ogv"},
    };
    const auto decodable = QMediaFormat().supportedFileFormats(QMediaFormat::Decode);
    for (const Container &c : kContainers)
        if (decodable.contains(c.format))
            m_extensions << QString::fromLatin1(c.extensions).split(QLatin1Char(' '));
    return m_extensions;
}
