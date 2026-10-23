#pragma once

#include <QImage>
#include <QStringList>
#include <QVariantList>

// What a text recogniser hands back: the words as {text, x, y, w, h, line, para, joined} (the box in picture pixels,
// `line` and `para` counting from 0 in reading order, `joined` true when the words of that language are not separated by
// spaces) and whether it could not even start because no language is installed.
struct OcrFound {
    QVariantList words;
    bool noLanguage = false;
};

#ifdef PROJECTFOXY_TESSERACT
// Tesseract (Linux; elsewhere only to try it). `languages` are Tesseract codes ("spa", "eng"...) and `dataDirs` the folders
// where their .traineddata may be.
OcrFound recogniseWithTesseract(const QImage &image, const QStringList &languages, const QStringList &dataDirs);
#endif
