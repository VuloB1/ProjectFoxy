#include "OcrLanguages.h"

#include "AppSettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QStandardPaths>

namespace {

struct Language { const char *code; const char *name; };
// The languages offered, by their own name. Tesseract's "fast" models (about 1-4 MB each), at a tag that does not move.
constexpr Language kLanguages[] = {
    {"spa", "Español"}, {"eng", "English"}, {"fra", "Français"}, {"deu", "Deutsch"}, {"ita", "Italiano"},
    {"por", "Português"}, {"cat", "Català"}, {"nld", "Nederlands"}, {"swe", "Svenska"}, {"dan", "Dansk"},
    {"nor", "Norsk"}, {"fin", "Suomi"}, {"pol", "Polski"}, {"ces", "Čeština"}, {"hun", "Magyar"},
    {"ron", "Română"}, {"ell", "Ελληνικά"}, {"tur", "Türkçe"}, {"rus", "Русский"}, {"ukr", "Українська"},
    {"heb", "עברית"}, {"ara", "العربية"}, {"hin", "हिन्दी"}, {"tha", "ไทย"}, {"vie", "Tiếng Việt"},
    {"ind", "Bahasa Indonesia"}, {"jpn", "日本語"}, {"kor", "한국어"}, {"chi_sim", "中文 (简体)"}, {"chi_tra", "中文 (繁體)"},
};
constexpr int kMinimumBytes = 100 * 1024; // a language file is never smaller; an error page is

QString fileName(const QString &code) { return code + QStringLiteral(".traineddata"); }

} // namespace

OcrLanguages::OcrLanguages(AppSettings *settings, QObject *parent) : QObject(parent), m_settings(settings) {}

bool OcrLanguages::available() const
{
#ifdef PROJECTFOXY_TESSERACT
    return true;
#else
    return false;
#endif
}

QString OcrLanguages::userDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/tessdata");
}

QStringList OcrLanguages::dataDirs() const
{
    QStringList dirs{userDir()};
    const QString app = QCoreApplication::applicationDirPath();
    // AppImage (usr/bin -> usr/share) and Flatpak (/app/bin -> /app/share), then the system's, for a build run from a tree
    dirs << app + QStringLiteral("/../share/tessdata") << QStringLiteral("/usr/share/tessdata")
         << QStringLiteral("/usr/share/tesseract-ocr/5/tessdata") << QStringLiteral("/usr/share/tesseract-ocr/4.00/tessdata");
    const QString env = qEnvironmentVariable("PROJECTFOXY_TESSDATA"); // to try it somewhere that has none of those
    if (!env.isEmpty())
        dirs.prepend(env);
    return dirs;
}

bool OcrLanguages::bundled(const QString &code) const
{
    for (const QString &dir : dataDirs())
        if (dir != userDir() && QFileInfo::exists(QDir(dir).filePath(fileName(code))))
            return true;
    return false;
}

bool OcrLanguages::installed(const QString &code) const
{
    return bundled(code) || QFileInfo::exists(QDir(userDir()).filePath(fileName(code)));
}

QStringList OcrLanguages::activeLanguages() const
{
    QStringList out;
    for (const QString &code : m_settings->ocrLanguages())
        if (installed(code))
            out << code;
    return out;
}

QVariantList OcrLanguages::languages() const
{
    QVariantList out;
    const QStringList enabled = m_settings->ocrLanguages();
    for (const Language &language : kLanguages) {
        const QString code = QString::fromLatin1(language.code);
        const Job job = m_jobs.value(code);
        QVariantMap entry;
        entry[QStringLiteral("code")] = code;
        entry[QStringLiteral("name")] = QString::fromUtf8(language.name);
        entry[QStringLiteral("installed")] = installed(code);
        entry[QStringLiteral("bundled")] = bundled(code);
        entry[QStringLiteral("enabled")] = enabled.contains(code);
        entry[QStringLiteral("busy")] = m_jobs.contains(code) && !job.failed;
        entry[QStringLiteral("progress")] = job.progress;
        entry[QStringLiteral("failed")] = job.failed;
        out.append(entry);
    }
    return out;
}

void OcrLanguages::setEnabled(const QString &code, bool on)
{
    QStringList enabled = m_settings->ocrLanguages();
    if (on == enabled.contains(code))
        return;
    if (on)
        enabled.append(code);
    else
        enabled.removeAll(code);
    m_settings->setOcrLanguages(enabled);
    emit changed();
}

void OcrLanguages::remove(const QString &code)
{
    if (bundled(code))
        return; // it came with the program
    QFile::remove(QDir(userDir()).filePath(fileName(code)));
    setEnabled(code, false);
    emit changed();
}

void OcrLanguages::download(const QString &code)
{
    if (m_jobs.contains(code) && !m_jobs.value(code).failed)
        return;
    if (!m_network)
        m_network = new QNetworkAccessManager(this);
    m_jobs[code] = Job{};
    emit changed();

    QNetworkRequest request(QUrl(QStringLiteral("https://github.com/tesseract-ocr/tessdata_fast/raw/4.1.0/") + fileName(code)));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = m_network->get(request);
    QPointer<OcrLanguages> self(this);
    connect(reply, &QNetworkReply::downloadProgress, this, [self, code](qint64 received, qint64 total) {
        if (self && total > 0 && self->m_jobs.contains(code)) {
            self->m_jobs[code].progress = qreal(received) / total;
            emit self->changed();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [self, code, reply]() {
        reply->deleteLater();
        if (!self)
            return;
        const QByteArray data = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError && data.size() >= kMinimumBytes;
        if (ok) {
            QDir().mkpath(self->userDir());
            const QString target = QDir(self->userDir()).filePath(fileName(code));
            QFile part(target + QStringLiteral(".part"));
            if (part.open(QIODevice::WriteOnly) && part.write(data) == data.size()) {
                part.close();
                QFile::remove(target);
                if (part.rename(target)) {
                    self->m_jobs.remove(code);
                    self->setEnabled(code, true);
                    emit self->changed();
                    return;
                }
            }
            part.remove();
        }
        self->m_jobs[code].failed = true;
        emit self->changed();
    });
}
