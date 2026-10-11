#include "DesktopWallpaper.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QMetaObject>
#include <QRunnable>
#include <QStandardPaths>
#include <QUrl>
#include <QWindow>

#ifdef _WIN32
// NOMINMAX before Windows.h - otherwise its min/max macros shadow std::min/std::max.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <QProcess>
#include "LinuxPortal.h"
#endif
#include "Translate.h"

namespace {

QString doneMessage() { return core::tr("Fondo de escritorio cambiado"); }
QString failedMessage() { return core::tr("No se pudo cambiar el fondo de escritorio"); }

#ifndef _WIN32
// The desktop's own tool, whose result is known at once (this waits for it: call it from a worker). False when the
// desktop is not one of those handled here, or the tool failed.
bool wallpaperViaDesktopTools(const QString &path)
{
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP").toLower();
    const QString uri = QUrl::fromLocalFile(path).toString();
    auto run = [](const QString &program, const QStringList &args) {
        QProcess p;
        p.start(program, args);
        return p.waitForFinished(5000) && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
    };
    if (desktop.contains(QLatin1String("kde")))
        return run(QStringLiteral("plasma-apply-wallpaperimage"), {path});
    if (desktop.contains(QLatin1String("xfce"))) {
        // One property per monitor/workspace: set them all.
        QProcess list;
        list.start(QStringLiteral("xfconf-query"), {QStringLiteral("-c"), QStringLiteral("xfce4-desktop"), QStringLiteral("-l")});
        if (!list.waitForFinished(5000))
            return false;
        bool any = false;
        for (const QString &prop : QString::fromUtf8(list.readAllStandardOutput()).split(QLatin1Char('\n'))) {
            if (prop.endsWith(QLatin1String("/last-image")))
                any = run(QStringLiteral("xfconf-query"), {QStringLiteral("-c"), QStringLiteral("xfce4-desktop"), QStringLiteral("-p"), prop, QStringLiteral("-s"), path}) || any;
        }
        return any;
    }
    // GNOME, Cinnamon, MATE, Budgie, COSMIC... share the gsettings route with different schemas.
    struct Schema { const char *name; const char *key; const char *keyDark; };
    const Schema schemas[] = {
        {"org.gnome.desktop.background", "picture-uri", "picture-uri-dark"},
        {"org.cinnamon.desktop.background", "picture-uri", nullptr},
        {"org.mate.background", "picture-filename", nullptr},
    };
    for (const Schema &s : schemas) {
        const QString schema = QString::fromLatin1(s.name);
        const QString value = (s.key == QLatin1String("picture-filename")) ? path : uri;
        if (!run(QStringLiteral("gsettings"), {QStringLiteral("set"), schema, QString::fromLatin1(s.key), value}))
            continue;
        if (s.keyDark)
            run(QStringLiteral("gsettings"), {QStringLiteral("set"), schema, QString::fromLatin1(s.keyDark), value});
        return true;
    }
    return false;
}
#endif

} // namespace

DesktopWallpaper::DesktopWallpaper(QObject *parent) : QObject(parent) {}

DesktopWallpaper::~DesktopWallpaper()
{
#ifndef _WIN32
    m_pool.waitForDone(); // its worker posts back to this object
#endif
}

void DesktopWallpaper::set(const QImage &picture)
{
    if (picture.isNull())
        return;

#ifdef _WIN32
    const QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QStringLiteral("/imageviewer_wallpaper.png");
    const std::wstring wpath = QDir::toNativeSeparators(tempPath).toStdWString();
    const bool ok = picture.save(tempPath, "PNG")
        && SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, const_cast<wchar_t *>(wpath.c_str()),
                                 SPIF_UPDATEINIFILE | SPIF_SENDCHANGE) != 0;
    emit finished(ok ? doneMessage() : failedMessage());
#else
    // Linux has no single wallpaper API. Writing the picture and asking the desktop's own tool take a while (a process
    // per call), so they run on a worker; the portal, which talks over DBus, is then asked from here.
    if (m_busy)
        return;
    m_busy = true;
    m_pool.start(QRunnable::create([this, picture]() {
        // The file gets a new name every time (GNOME ignores a change of the picture when the path stays the same)
        // and lives in the user's data folder, not in /tmp.
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dir);
        for (const QString &old : QDir(dir).entryList({QStringLiteral("wallpaper-*.png")}, QDir::Files))
            QFile::remove(dir + QLatin1Char('/') + old);
        QString path = QStringLiteral("%1/wallpaper-%2.png").arg(dir).arg(QDateTime::currentMSecsSinceEpoch());
        if (!picture.save(path, "PNG"))
            path.clear();
        // Inside Flatpak the desktop's tools do not exist: only the Wallpaper portal can do it.
        const bool done = !path.isEmpty() && !LinuxPortal::inFlatpak() && wallpaperViaDesktopTools(path);
        QMetaObject::invokeMethod(this, [this, path, done]() { finish(path, done); }, Qt::QueuedConnection);
    }));
#endif
}

#ifndef _WIN32
void DesktopWallpaper::finish(const QString &path, bool done)
{
    const auto report = [this](bool ok) {
        m_busy = false;
        emit finished(ok ? doneMessage() : failedMessage());
    };
    if (done)
        return report(true);
    if (path.isEmpty()) // the picture could not even be written
        return report(false);
    // The desktop's tool did not do it (or there is none): the portal covers the rest (Flatpak, COSMIC...).
    QString parent;
    if (QGuiApplication::platformName() == QLatin1String("xcb")) {
        if (const QWindow *window = QGuiApplication::focusWindow())
            parent = QStringLiteral("x11:%1").arg(quint64(window->winId()), 0, 16);
    }
    LinuxPortal::setWallpaper(path, parent, this, [this, report](LinuxPortal::Outcome outcome) {
        if (outcome == LinuxPortal::Outcome::Cancelled) { // the person said no: nothing to report
            m_busy = false;
            return;
        }
        report(outcome == LinuxPortal::Outcome::Done);
    });
}
#endif
