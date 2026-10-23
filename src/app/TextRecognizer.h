#pragma once

#include <QImage>
#include <QObject>
#include <QVariantList>

class AppController;
class OcrLanguages;

// Reads the text of the picture that is on show, with the recogniser Windows already has (Windows.Media.Ocr: the one
// Photos and the Snipping Tool use; it runs on this computer, needs no download and reads the languages installed in
// Windows). Elsewhere `available` is false. The result is a list of words, each {text, x, y, w, h, line} with the box in
// picture pixels and `line` the number of the line it belongs to, in reading order.
class TextRecognizer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QVariantList words READ words NOTIFY wordsChanged)
    // "" (nothing to say), "none" (no text found) or "language" (Windows has no recogniser for the user's languages)
    Q_PROPERTY(QString problem READ problem NOTIFY problemChanged)

public:
    TextRecognizer(AppController *controller, OcrLanguages *languages, QObject *parent = nullptr);

    bool available() const;
    bool busy() const { return m_busy; }
    QVariantList words() const { return m_words; }
    QString problem() const { return m_problem; }

    Q_INVOKABLE void scan();
    Q_INVOKABLE void clear();
    Q_INVOKABLE void copyText(const QString &text) const;

signals:
    void busyChanged();
    void wordsChanged();
    void problemChanged();

private:
    AppController *m_controller;
    OcrLanguages *m_languages;
    bool m_busy = false;
    int m_job = 0; // a result that arrives for an older scan is dropped
    QVariantList m_words;
    QString m_problem;
};
