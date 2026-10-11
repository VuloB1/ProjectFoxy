#pragma once

#include <QImage>
#include <QObject>
#include <QString>
#include <QThreadPool>

// Puts a picture on the desktop as the wallpaper.
//   Windows: a temporary PNG and SystemParametersInfo.
//   Linux:   the desktop's own tool (KDE, XFCE, GNOME-family) from a worker, else the Wallpaper portal (see
//            LinuxPortal.h), whose real answer is awaited. Nothing here blocks the window.
// The outcome comes as finished(message), a short text for a non-blocking notice. When the person cancels in the
// portal nothing is reported.
class DesktopWallpaper : public QObject {
    Q_OBJECT
public:
    explicit DesktopWallpaper(QObject *parent = nullptr);
    ~DesktopWallpaper() override;

    // A request while another is still running (Linux) is ignored.
    void set(const QImage &picture);

signals:
    void finished(const QString &message);

#ifndef _WIN32
private:
    void finish(const QString &path, bool done);
    QThreadPool m_pool; // the worker holds `this`: waited for in the destructor
    bool m_busy = false;
#endif
};
