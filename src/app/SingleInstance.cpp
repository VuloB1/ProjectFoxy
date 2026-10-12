#include "SingleInstance.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QTimer>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace {

constexpr int kMaxRequestBytes = 64 * 1024; // a line of file names: anything bigger is not from us
constexpr int kRequestTimeoutMs = 5000;     // a client that connects and says nothing is let go

QString userName()
{
    QString user = qEnvironmentVariable("USERNAME");
    if (user.isEmpty())
        user = qEnvironmentVariable("USER");
    // keep it to characters that are safe in a pipe or file name
    for (QChar &c : user)
        if (!c.isLetterOrNumber())
            c = QLatin1Char('_');
    return user.isEmpty() ? QStringLiteral("user") : user;
}

} // namespace

QString SingleInstance::defaultName()
{
    const QString base = QStringLiteral("ProjectFoxy-%1").arg(userName());
#ifdef Q_OS_WIN
    return base; // a named pipe: the system keeps the namespace per session
#else
    // The runtime folder is per user (and the one Flatpak shares between runs of the same app); /tmp is the fallback.
    QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (dir.isEmpty())
        dir = QDir::tempPath();
    return dir + QLatin1Char('/') + base + QStringLiteral(".sock");
#endif
}

SingleInstance::SingleInstance(const QString &name, QObject *parent) : QObject(parent), m_name(name) {}

SingleInstance::~SingleInstance() = default;

SingleInstance::Result SingleInstance::sendToRunning(const QString &name, const QStringList &files, int timeoutMs)
{
#ifdef Q_OS_WIN
    // This process was just started by the person's click, so Windows lets it hand the foreground over; without this
    // the other window would only blink in the taskbar.
    AllowSetForegroundWindow(static_cast<DWORD>(-1)); // ASFRW_ANY
#endif
    QLocalSocket socket;
    socket.connectToServer(name);
    if (!socket.waitForConnected(timeoutMs))
        return Result::NobodyThere;

    QJsonArray list;
    for (const QString &file : files)
        list.append(QFileInfo(file).absoluteFilePath());
    QByteArray line = QJsonDocument(QJsonObject{{QStringLiteral("files"), list}}).toJson(QJsonDocument::Compact);
    line.append('\n');
    socket.write(line);
    if (!socket.waitForBytesWritten(timeoutMs))
        return Result::NoAnswer;
    if (!socket.waitForReadyRead(timeoutMs))
        return Result::NoAnswer;
    return socket.readLine().trimmed() == "ok" ? Result::Delivered : Result::NoAnswer;
}

bool SingleInstance::listen()
{
    if (m_server)
        return true;

    // Somebody already answers on this name: it is theirs, not ours to take over.
    {
        QLocalSocket probe;
        probe.connectToServer(m_name);
        if (probe.waitForConnected(200))
            return false;
    }

    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption); // only this user may connect
    // Nobody answered, so what is left on disk (a crash, a kill) is stale: clear it.
    QLocalServer::removeServer(m_name);
    if (!m_server->listen(m_name)) {
        delete m_server;
        m_server = nullptr;
        return false;
    }
    connect(m_server, &QLocalServer::newConnection, this, &SingleInstance::onNewConnection);
    return true;
}

void SingleInstance::onNewConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        socket->setParent(this);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() { onReadyRead(socket); });
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        QTimer::singleShot(kRequestTimeoutMs, socket, [socket]() { socket->disconnectFromServer(); });
    }
}

void SingleInstance::onReadyRead(QLocalSocket *socket)
{
    if (socket->bytesAvailable() > kMaxRequestBytes) {
        socket->disconnectFromServer();
        return;
    }
    if (!socket->canReadLine())
        return; // the rest of the line has not arrived yet

    const QJsonDocument doc = QJsonDocument::fromJson(socket->readLine());
    if (!doc.isObject() || !doc.object().value(QStringLiteral("files")).isArray()) {
        socket->write("bad\n");
        socket->disconnectFromServer();
        return;
    }
    QStringList files;
    for (const QJsonValue &v : doc.object().value(QStringLiteral("files")).toArray())
        if (v.isString() && !v.toString().isEmpty())
            files << v.toString();

    // Answer first: the other process is waiting, and opening a picture can take a while.
    socket->write("ok\n");
    socket->flush();
    socket->disconnectFromServer();

    emit activateRequested();
    if (!files.isEmpty())
        emit openRequested(files.first());
}
