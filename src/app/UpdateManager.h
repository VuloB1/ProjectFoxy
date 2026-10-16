#pragma once

#include "UpdateCheck.h"

#include <QFile>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QUrl>

class AppSettings;
class QNetworkReply;

// Looks on GitHub for a newer published version of Project Foxy and, on Windows when the program was installed with
// its installer, downloads that installer, checks it against the release's SHA256SUMS.txt and runs it.
//
// States (`state`): "idle" (nothing to say), "checking", "upToDate" (only after a manual check), "available" (a newer
// version exists), "downloading", "ready" (downloaded and verified), "installing", "failed" (`error` says why).
//
// What it sends: one HTTPS GET to GitHub's public API (the program's name and version in the User-Agent, nothing
// else), at most once a day and only while "Buscar actualizaciones" is on in Settings. Nothing is installed without
// the person pressing the button, and never while a picture has unsaved edits (the QML asks `appController`).
class UpdateManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY stateChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    // network | invalid | nosums | corrupt | disk | launch (the QML words them)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    // This copy can update itself: Windows, installed by the installer (not the portable zip).
    Q_PROPERTY(bool canInstall READ canInstall CONSTANT)
    // The notice is on screen (until it is closed, or until the next version).
    Q_PROPERTY(bool bannerVisible READ bannerVisible NOTIFY bannerChanged)

public:
    UpdateManager(AppSettings *settings, const QString &currentVersion, QObject *parent = nullptr);

    QString state() const { return m_state; }
    QString currentVersion() const { return m_currentVersion; }
    QString latestVersion() const { return m_release.version; }
    double progress() const { return m_progress; }
    QString error() const { return m_error; }
    bool canInstall() const;
    bool bannerVisible() const;

    // Where the answer comes from; tests and the developer build point it somewhere else.
    void setApiUrl(const QUrl &url) { m_apiUrl = url; }
    // Where the installer goes (default: a folder in the temporary location).
    void setDownloadDir(const QString &dir) { m_downloadDir = dir; }
    // Do everything except starting the installer and quitting.
    void setDryRun(bool dry) { m_dryRun = dry; }
    // Developer builds: fetch the files of a release from here instead of https://github.com (a fake server).
    void setDownloadBase(const QString &base) { m_downloadBase = base; }

    // The check that runs by itself a few seconds after start: only when it is turned on and a day has passed.
    Q_INVOKABLE void checkOnStart();
    // "Buscar ahora": always asks, and says so when there is nothing new.
    Q_INVOKABLE void checkNow() { check(true); }
    // Downloads and verifies the installer (state "ready"); without that possibility it opens the release page.
    Q_INVOKABLE void download();
    // Starts the verified installer and quits so that it can replace the files.
    Q_INVOKABLE void installAndRestart();
    Q_INVOKABLE void openReleasePage() const;
    // "Omitir esta versión": no automatic notice for it any more (a newer one will notify again).
    Q_INVOKABLE void skipVersion();
    // Closes the notice for this session.
    Q_INVOKABLE void dismiss();

signals:
    void stateChanged();
    void progressChanged();
    void bannerChanged();

private:
    void check(bool manual);
    void setState(const QString &state, const QString &error = QString());
    void fail(const QString &error);
    void fetchSums();
    void fetchInstaller(const QString &expectedHash);
    QString installerPath() const;
    QUrl fileUrl(const QString &githubUrl) const;

    QPointer<AppSettings> m_settings;
    QString m_currentVersion;
    QUrl m_apiUrl;
    QString m_downloadDir;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    core::ReleaseInfo m_release;
    QString m_state = QStringLiteral("idle");
    QString m_error;
    double m_progress = 0.0;
    bool m_dismissed = false;
    bool m_manual = false;
    bool m_dryRun = false;
    QString m_downloadBase;
};
