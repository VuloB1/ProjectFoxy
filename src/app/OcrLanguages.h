#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariantList>

class AppSettings;
class QNetworkAccessManager;

// The languages the text recogniser of Tesseract (Linux) can read: which ones are installed (the ones that come with
// the program and the ones downloaded to the user's folder), which of them are in use, and downloading more. Where
// Tesseract is not built in (`available` false) the list is empty.
class OcrLanguages : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    // {code, name, installed, bundled, enabled, busy, progress 0..1, failed}
    Q_PROPERTY(QVariantList languages READ languages NOTIFY changed)

public:
    explicit OcrLanguages(AppSettings *settings, QObject *parent = nullptr);

    bool available() const;
    QVariantList languages() const;
    // Installed and switched on: what a scan reads.
    QStringList activeLanguages() const;
    // Folders to look for the .traineddata in: the user's (downloads) and the ones the program came with.
    QStringList dataDirs() const;

    Q_INVOKABLE void download(const QString &code);
    Q_INVOKABLE void remove(const QString &code);
    Q_INVOKABLE void setEnabled(const QString &code, bool on);

signals:
    void changed();

private:
    struct Job { qreal progress = 0; bool failed = false; };
    QString userDir() const;
    bool bundled(const QString &code) const;
    bool installed(const QString &code) const;

    AppSettings *m_settings;
    QNetworkAccessManager *m_network = nullptr;
    QHash<QString, Job> m_jobs; // downloads under way (or that failed)
};
