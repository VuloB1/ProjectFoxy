#include "LinuxPortal.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QFile>
#include <QFileInfo>
#include <QVariantMap>

#include <fcntl.h>
#include <unistd.h>

namespace LinuxPortal {

namespace {

constexpr auto kService = "org.freedesktop.portal.Desktop";
constexpr auto kPath = "/org/freedesktop/portal/desktop";

// An open descriptor for the file, closed when it goes out of scope.
struct Fd {
    explicit Fd(const QString &path) : fd(::open(QFile::encodeName(path).constData(), O_RDONLY | O_CLOEXEC)) {}
    ~Fd() { if (fd >= 0) ::close(fd); }
    int fd;
};

} // namespace

bool inFlatpak()
{
    return QFileInfo::exists(QStringLiteral("/.flatpak-info"));
}

bool trashFile(const QString &path)
{
    Fd file(path);
    if (file.fd < 0)
        return false;
    QDBusMessage call = QDBusMessage::createMethodCall(QLatin1String(kService), QLatin1String(kPath),
                                                       QStringLiteral("org.freedesktop.portal.Trash"),
                                                       QStringLiteral("TrashFile"));
    call << QVariant::fromValue(QDBusUnixFileDescriptor(file.fd));
    const QDBusMessage reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 10000);
    // The reply is one uint: 0 = failed, 1 = trashed (2 = trashed, but it cannot be restored).
    return reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()
        && reply.arguments().first().toUInt() >= 1;
}

bool setWallpaper(const QString &path)
{
    Fd file(path);
    if (file.fd < 0)
        return false;
    QVariantMap options;
    options.insert(QStringLiteral("show-preview"), false);
    options.insert(QStringLiteral("set-on"), QStringLiteral("both")); // background and lock screen
    QDBusMessage call = QDBusMessage::createMethodCall(QLatin1String(kService), QLatin1String(kPath),
                                                       QStringLiteral("org.freedesktop.portal.Wallpaper"),
                                                       QStringLiteral("SetWallpaperFile"));
    call << QString() // parent window: none
         << QVariant::fromValue(QDBusUnixFileDescriptor(file.fd)) << options;
    const QDBusMessage reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 10000);
    // The answer to the request itself arrives later as a signal; an accepted call is all that can be known here.
    return reply.type() == QDBusMessage::ReplyMessage;
}

} // namespace LinuxPortal
