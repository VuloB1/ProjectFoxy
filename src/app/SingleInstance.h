#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

class QLocalServer;
class QLocalSocket;

// One window for the program: opening a picture while Project Foxy is already running (double click in the file
// manager, "Open with") hands the file to the running one and brings its window forward, instead of starting a
// second process.
//
// The first instance listens on a local socket that belongs to the user (a named pipe on Windows, a socket in the
// runtime folder elsewhere). Later ones connect, send one JSON line - {"files": [...]} - wait for "ok" and exit.
// If nobody answers (no instance, a stale socket, or one that is hung), the caller just runs on its own.
class SingleInstance : public QObject {
    Q_OBJECT
public:
    explicit SingleInstance(const QString &name = defaultName(), QObject *parent = nullptr);
    ~SingleInstance() override;

    // Per user (and per sandbox where the runtime folder differs), so two users on one machine never meet.
    static QString defaultName();

    enum class Result {
        Delivered,  // the running instance took the request: this process can exit
        NobodyThere, // nothing is listening
        NoAnswer    // something accepted the connection but did not answer in time: run on our own, do not listen
    };
    // Asks the running instance to open `files` (the first one; any other is ignored) and come forward. `files` are
    // made absolute here, against THIS process's folder: the other one could be anywhere. Blocks up to `timeoutMs`.
    static Result sendToRunning(const QString &name, const QStringList &files, int timeoutMs = 1500);

    // Becomes the instance that others talk to. False when that is not possible (then it just runs on its own).
    bool listen();

signals:
    // The first file of a request.
    void openRequested(const QString &file);
    // Every request: the window should come to the front.
    void activateRequested();

private:
    void onNewConnection();
    void onReadyRead(QLocalSocket *socket);

    QString m_name;
    QLocalServer *m_server = nullptr;
};
