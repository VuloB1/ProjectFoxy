#include "LinuxPortal.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QRandomGenerator>
#include <QTimer>
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
    // `writable`: the Trash portal wants a descriptor the app could also write to; a read-only file falls back to O_RDONLY.
    explicit Fd(const QString &path, bool writable = false) : fd(-1)
    {
        const QByteArray name = QFile::encodeName(path);
        if (writable)
            fd = ::open(name.constData(), O_RDWR | O_CLOEXEC);
        if (fd < 0)
            fd = ::open(name.constData(), O_RDONLY | O_CLOEXEC);
    }
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
    Fd file(path, /*writable=*/true);
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

// One Wallpaper request, from the call to the portal's Response signal. It deletes itself when it has reported.
class WallpaperRequest : public QObject {
    Q_OBJECT
public:
    WallpaperRequest(QObject *context, std::function<void(Outcome)> done) : QObject(context), m_done(std::move(done))
    {
        m_timeout.setSingleShot(true);
        m_timeout.setInterval(120000);
        connect(&m_timeout, &QTimer::timeout, this, [this]() { finish(Outcome::Failed); });
    }

    void start(const QString &path, const QString &parentWindow)
    {
        Fd file(path);
        if (file.fd < 0)
            return finish(Outcome::Failed);
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected())
            return finish(Outcome::Failed);

        // The Response can arrive before the call returns, so the handle it will come on is worked out first
        // (the portal's rule: the caller's unique name without ':' and with '.' as '_', then the token we choose)
        // and listened to before asking.
        const QString token = QStringLiteral("foxy%1").arg(QRandomGenerator::global()->generate(), 0, 16);
        QString sender = bus.baseService();
        sender.remove(0, 1).replace(QLatin1Char('.'), QLatin1Char('_'));
        listen(QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token));

        QVariantMap options;
        options.insert(QStringLiteral("handle_token"), token);
        options.insert(QStringLiteral("show-preview"), false);
        options.insert(QStringLiteral("set-on"), QStringLiteral("both")); // background and lock screen
        QDBusMessage call = QDBusMessage::createMethodCall(QLatin1String(kService), QLatin1String(kPath),
                                                           QStringLiteral("org.freedesktop.portal.Wallpaper"),
                                                           QStringLiteral("SetWallpaperFile"));
        call << parentWindow << QVariant::fromValue(QDBusUnixFileDescriptor(file.fd)) << options;
        m_timeout.start();
        auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(call, 10000), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
            const QDBusPendingReply<QDBusObjectPath> reply = *w;
            w->deleteLater();
            if (reply.isError())
                return finish(Outcome::Failed);
            // An old portal may hand back a handle other than the expected one: listen on that one too.
            listen(reply.value().path());
        });
    }

private slots:
    void onResponse(uint response, const QVariantMap &)
    {
        finish(response == 0 ? Outcome::Done : response == 1 ? Outcome::Cancelled : Outcome::Failed);
    }

private:
    void listen(const QString &requestPath)
    {
        if (requestPath.isEmpty() || m_listening.contains(requestPath))
            return;
        m_listening << requestPath;
        QDBusConnection::sessionBus().connect(QLatin1String(kService), requestPath,
                                              QStringLiteral("org.freedesktop.portal.Request"),
                                              QStringLiteral("Response"), this, SLOT(onResponse(uint, QVariantMap)));
    }

    void finish(Outcome outcome)
    {
        if (m_finished)
            return;
        m_finished = true;
        m_timeout.stop();
        for (const QString &p : std::as_const(m_listening))
            QDBusConnection::sessionBus().disconnect(QLatin1String(kService), p, QStringLiteral("org.freedesktop.portal.Request"),
                                                     QStringLiteral("Response"), this, SLOT(onResponse(uint, QVariantMap)));
        if (m_done)
            m_done(outcome);
        deleteLater();
    }

    std::function<void(Outcome)> m_done;
    QTimer m_timeout;
    QStringList m_listening;
    bool m_finished = false;
};

void setWallpaper(const QString &path, const QString &parentWindow, QObject *context, std::function<void(Outcome)> done)
{
    auto *request = new WallpaperRequest(context, std::move(done));
    request->start(path, parentWindow);
}

} // namespace LinuxPortal

#include "LinuxPortal.moc"
