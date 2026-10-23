#include "OcrBackend.h"

// Some build setups define this for every file; Tesseract's installed headers then ask for config_auto.h, which is not installed.
#ifdef HAVE_CONFIG_H
#undef HAVE_CONFIG_H
#endif

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <tesseract/baseapi.h>
#include <tesseract/resultiterator.h>

#include <memory>

OcrFound recogniseWithTesseract(const QImage &input, const QStringList &languages, const QStringList &dataDirs)
{
    OcrFound found;
    // Tesseract takes ONE data folder: the language files wanted are linked (copied on Windows, where a link is a
    // shortcut) into a folder of their own, wherever they live (the ones that came with the program, the ones downloaded).
    QTemporaryDir run;
    QStringList usable;
    for (const QString &language : languages) {
        const QString name = language + QStringLiteral(".traineddata");
        for (const QString &dir : dataDirs) {
            const QString source = QDir(dir).filePath(name);
            if (!QFileInfo::exists(source))
                continue;
            const QString target = run.filePath(name);
#ifdef Q_OS_WIN
            const bool linked = QFile::copy(source, target);
#else
            const bool linked = QFile::link(source, target);
#endif
            if (linked)
                usable << language;
            break;
        }
    }
    if (usable.isEmpty() || !run.isValid()) {
        found.noLanguage = true;
        return found;
    }

    // Very large pictures are read reduced (the boxes are brought back), or it would take minutes.
    constexpr int limit = 5000;
    QImage image = input;
    double back = 1.0;
    if (qMax(image.width(), image.height()) > limit) {
        back = double(qMax(image.width(), image.height())) / limit;
        image = image.scaled(limit, limit, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    image = image.convertToFormat(QImage::Format_Grayscale8);

    tesseract::TessBaseAPI api;
    if (api.Init(run.path().toUtf8().constData(), usable.join(QLatin1Char('+')).toUtf8().constData()) != 0) {
        found.noLanguage = true;
        return found;
    }
    api.SetPageSegMode(tesseract::PSM_AUTO);
    api.SetImage(image.constBits(), image.width(), image.height(), 1, int(image.bytesPerLine()));
    if (api.Recognize(nullptr) != 0)
        return found;

    bool noSpaces = false;
    for (const QString &language : usable)
        noSpaces = noSpaces || language == QLatin1String("jpn") || language.startsWith(QLatin1String("chi_")); // not separated by spaces
    std::unique_ptr<tesseract::ResultIterator> it(api.GetIterator());
    int line = -1, para = -1;
    if (it) {
        do {
            if (it->IsAtBeginningOf(tesseract::RIL_PARA))
                ++para;
            if (it->IsAtBeginningOf(tesseract::RIL_TEXTLINE))
                ++line;
            std::unique_ptr<char[]> text(it->GetUTF8Text(tesseract::RIL_WORD));
            if (!text || it->Confidence(tesseract::RIL_WORD) < 40)
                continue; // what Tesseract itself doubts is mostly noise
            const QString word = QString::fromUtf8(text.get()).trimmed();
            int l, t, r, b;
            if (word.isEmpty() || !it->BoundingBox(tesseract::RIL_WORD, &l, &t, &r, &b))
                continue;
            QVariantMap entry;
            entry[QStringLiteral("text")] = word;
            entry[QStringLiteral("x")] = l * back;
            entry[QStringLiteral("y")] = t * back;
            entry[QStringLiteral("w")] = (r - l) * back;
            entry[QStringLiteral("h")] = (b - t) * back;
            entry[QStringLiteral("line")] = qMax(line, 0);
            entry[QStringLiteral("para")] = qMax(para, 0);
            entry[QStringLiteral("joined")] = noSpaces;
            found.words.append(entry);
        } while (it->Next(tesseract::RIL_WORD));
    }
    api.End();
    return found;
}
