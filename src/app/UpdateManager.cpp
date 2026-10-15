#include "UpdateManager.h"

#include "AppSettings.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>

namespace {

constexpr auto kDefaultApi = "https://api.github.com/repos/VuloB1/ProjectFoxy/releases/latest";
constexpr qint64 kCheckEveryHours = 20;

QNetworkRequest makeRequest(const QUrl &url, const QString &version, bool json)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("ProjectFoxy/%1").arg(version));
    if (json)
        request.setRawHeader("Accept", "application/vnd.github+json");
    // GitHub sends the files on to another host.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(20000);
    return request;
}

} // namespace

UpdateManager::UpdateManager(AppSettings *settings, const QString &currentVersion, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_currentVersion(currentVersion)
    , m_apiUrl(QUrl(QLatin1String(kDefaultApi)))
    , m_downloadDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/ProjectFoxy-update"))
{
}

bool UpdateManager::canInstall() const
{
#ifdef Q_OS_WIN
    // The installer leaves its uninstaller next to the program; the portable zip does not have one.
    return QFileInfo::exists(QCoreApplication::applicationDirPath() + QStringLiteral("/unins000.exe"));
#else
    return false;
#endif
}

bool UpdateManager::bannerVisible() const
{
    if (m_dismissed)
        return false;
    return m_state == QLatin1String("available") || m_state == QLatin1String("downloading")
           || m_state == QLatin1String("ready") || m_state == QLatin1String("installing")
           || (m_state == QLatin1String("failed") && !m_release.version.isEmpty());
}

void UpdateManager::setState(const QString &state, const QString &error)
{
    const bool banner = bannerVisible();
    m_state = state;
    m_error = error;
    emit stateChanged();
    if (banner != bannerVisible())
        emit bannerChanged();
}

void UpdateManager::fail(const QString &error)
{
    // A check nobody asked for does not bother anyone when the network is down.
    if (m_state == QLatin1String("checking") && !m_manual)
        setState(QStringLiteral("idle"));
    else
        setState(QStringLiteral("failed"), error);
}

void UpdateManager::checkOnStart()
{
    if (!m_settings || !m_settings->updateCheck())
        return;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (now - m_settings->lastUpdateCheck() < kCheckEveryHours * 3600)
        return;
    check(false);
}

void UpdateManager::check(bool manual)
{
    if (m_state == QLatin1String("checking") || m_state == QLatin1String("downloading")
        || m_state == QLatin1String("ready") || m_state == QLatin1String("installing"))
        return;
    m_manual = manual;
    setState(QStringLiteral("checking"));
    m_reply = m_network.get(makeRequest(m_apiUrl, m_currentVersion, true));
    connect(m_reply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *reply = m_reply;
        if (!reply)
            return;
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return fail(QStringLiteral("network"));
        const core::ReleaseInfo release = core::parseLatestRelease(reply->readAll());
        if (!release.valid)
            return fail(QStringLiteral("invalid"));
        if (m_settings)
            m_settings->setLastUpdateCheck(QDateTime::currentSecsSinceEpoch());

        if (!core::isNewerVersion(release.version, m_currentVersion)) {
            m_release = {};
            setState(m_manual ? QStringLiteral("upToDate") : QStringLiteral("idle"));
            return;
        }
        if (!m_manual && m_settings && m_settings->skippedUpdate() == release.version) {
            setState(QStringLiteral("idle")); // the person said "not this one"
            return;
        }
        if (release.version != m_release.version)
            m_dismissed = false;
        m_release = release;
        setState(QStringLiteral("available"));
    });
}

QUrl UpdateManager::fileUrl(const QString &githubUrl) const
{
    if (m_downloadBase.isEmpty())
        return QUrl(githubUrl);
    return QUrl(m_downloadBase + githubUrl.mid(QStringLiteral("https://github.com").size()));
}

QString UpdateManager::installerPath() const
{
    return m_downloadDir + QLatin1Char('/') + m_release.installerName;
}

void UpdateManager::download()
{
    if (m_state != QLatin1String("available") && m_state != QLatin1String("failed"))
        return;
    // Without an installer to run here, the person goes to the release page.
    if (!canInstall() || m_release.installerUrl.isEmpty() || m_release.sumsUrl.isEmpty()) {
        openReleasePage();
        return;
    }
    m_progress = 0.0;
    emit progressChanged();
    setState(QStringLiteral("downloading"));
    fetchSums();
}

void UpdateManager::fetchSums()
{
    m_reply = m_network.get(makeRequest(fileUrl(m_release.sumsUrl), m_currentVersion, false));
    connect(m_reply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *reply = m_reply;
        if (!reply)
            return;
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return fail(QStringLiteral("network"));
        const QString hash = core::expectedSha256(reply->readAll(), m_release.installerName);
        if (hash.isEmpty())
            return fail(QStringLiteral("nosums")); // never run what cannot be checked
        fetchInstaller(hash);
    });
}

void UpdateManager::fetchInstaller(const QString &expectedHash)
{
    QDir dir(m_downloadDir);
    dir.removeRecursively(); // an older download is of no use
    if (!QDir().mkpath(m_downloadDir))
        return fail(QStringLiteral("disk"));
    auto *file = new QFile(installerPath(), this);
    if (!file->open(QIODevice::WriteOnly)) {
        file->deleteLater();
        return fail(QStringLiteral("disk"));
    }

    m_reply = m_network.get(makeRequest(fileUrl(m_release.installerUrl), m_currentVersion, false));
    QNetworkReply *reply = m_reply;
    // The file is written as it arrives (about 30 MB): never all of it in memory.
    connect(reply, &QNetworkReply::readyRead, file, [reply, file]() { file->write(reply->readAll()); });
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64 total) {
        if (total > 0) {
            m_progress = double(got) / double(total);
            emit progressChanged();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, expectedHash]() {
        reply->deleteLater();
        file->write(reply->readAll());
        file->close();
        file->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            QFile::remove(file->fileName());
            return fail(QStringLiteral("network"));
        }
        if (core::sha256OfFile(file->fileName()) != expectedHash) {
            QFile::remove(file->fileName());
            return fail(QStringLiteral("corrupt"));
        }
        m_progress = 1.0;
        emit progressChanged();
        setState(QStringLiteral("ready"));
    });
}

void UpdateManager::installAndRestart()
{
    if (m_state != QLatin1String("ready"))
        return;
    setState(QStringLiteral("installing"));
    if (m_dryRun)
        return;
    // /SILENT: a small progress window, no questions. /CLOSEAPPLICATIONS: the installer closes what is still running.
    // /update=1: ProjectFoxy.iss starts the program again when it is done.
    const QStringList arguments{QStringLiteral("/SILENT"), QStringLiteral("/SUPPRESSMSGBOXES"),
                                QStringLiteral("/CLOSEAPPLICATIONS"), QStringLiteral("/update=1")};
    if (!QProcess::startDetached(installerPath(), arguments))
        return fail(QStringLiteral("launch"));
    QCoreApplication::quit();
}

void UpdateManager::openReleasePage() const
{
    if (!m_release.pageUrl.isEmpty())
        QDesktopServices::openUrl(QUrl(m_release.pageUrl));
}

void UpdateManager::skipVersion()
{
    if (m_settings && !m_release.version.isEmpty())
        m_settings->setSkippedUpdate(m_release.version);
    dismiss();
}

void UpdateManager::dismiss()
{
    if (m_dismissed)
        return;
    m_dismissed = true;
    emit bannerChanged();
}
