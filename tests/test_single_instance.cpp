#include "SingleInstance.h"
#include <QLocalServer>
#include <QLocalSocket>
#include <QUuid>
#include <QtTest>
#include <future>

Q_DECLARE_METATYPE(SingleInstance::Result)

namespace {

// A name nobody else is using, so tests (and a real Project Foxy that may be running) never meet.
QString uniqueName()
{
    const QString token = QUuid::createUuid().toString(QUuid::Id128).left(12);
#ifdef Q_OS_WIN
    return QStringLiteral("ProjectFoxyTest-") + token;
#else
    return QDir::tempPath() + QStringLiteral("/ProjectFoxyTest-") + token + QStringLiteral(".sock");
#endif
}

// The client blocks, so it runs on its own thread while this one keeps the server's event loop going.
std::future<SingleInstance::Result> sendAsync(const QString &name, const QStringList &files, int timeoutMs = 1500)
{
    return std::async(std::launch::async, [=]() { return SingleInstance::sendToRunning(name, files, timeoutMs); });
}

SingleInstance::Result waitFor(std::future<SingleInstance::Result> &result)
{
    while (result.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready)
        QCoreApplication::processEvents();
    return result.get();
}

} // namespace

// One window for the program: a second launch hands its file to the first and leaves.
class TestSingleInstance : public QObject {
    Q_OBJECT

private slots:
    void withNobodyListeningThereIsNobodyThere()
    {
        auto result = sendAsync(uniqueName(), {QStringLiteral("a.jpg")});
        QCOMPARE(waitFor(result), SingleInstance::Result::NobodyThere);
    }

    void aRequestReachesTheRunningInstanceWithAnAbsolutePath()
    {
        const QString name = uniqueName();
        SingleInstance primary(name);
        QVERIFY(primary.listen());
        QSignalSpy opened(&primary, &SingleInstance::openRequested);
        QSignalSpy activated(&primary, &SingleInstance::activateRequested);

        auto result = sendAsync(name, {QStringLiteral("rel/photo one.jpg"), QStringLiteral("other.jpg")});
        QCOMPARE(waitFor(result), SingleInstance::Result::Delivered);
        QTRY_COMPARE(opened.count(), 1);
        QCOMPARE(activated.count(), 1);
        // made absolute by the SENDER (its folder, not the running one's), and only the first file is opened
        QCOMPARE(opened.first().first().toString(), QFileInfo(QStringLiteral("rel/photo one.jpg")).absoluteFilePath());
        QVERIFY(QFileInfo(opened.first().first().toString()).isAbsolute());
    }

    void unicodeNamesSurvive()
    {
        const QString name = uniqueName();
        SingleInstance primary(name);
        QVERIFY(primary.listen());
        QSignalSpy opened(&primary, &SingleInstance::openRequested);
        const QString file = QDir::tempPath() + QStringLiteral("/foto 日本語 áñ \U0001F98A.png");
        auto result = sendAsync(name, {file});
        QCOMPARE(waitFor(result), SingleInstance::Result::Delivered);
        QTRY_COMPARE(opened.count(), 1);
        QCOMPARE(opened.first().first().toString(), QFileInfo(file).absoluteFilePath());
    }

    void noFileJustBringsTheWindowForward()
    {
        const QString name = uniqueName();
        SingleInstance primary(name);
        QVERIFY(primary.listen());
        QSignalSpy opened(&primary, &SingleInstance::openRequested);
        QSignalSpy activated(&primary, &SingleInstance::activateRequested);
        auto result = sendAsync(name, {});
        QCOMPARE(waitFor(result), SingleInstance::Result::Delivered);
        QTRY_COMPARE(activated.count(), 1);
        QCOMPARE(opened.count(), 0);
    }

    void severalLaunchesAreAllServed()
    {
        const QString name = uniqueName();
        SingleInstance primary(name);
        QVERIFY(primary.listen());
        QSignalSpy opened(&primary, &SingleInstance::openRequested);
        for (const char *file : {"a.jpg", "b.jpg", "c.jpg"}) {
            auto result = sendAsync(name, {QString::fromLatin1(file)});
            QCOMPARE(waitFor(result), SingleInstance::Result::Delivered);
        }
        QTRY_COMPARE(opened.count(), 3);
        QVERIFY(opened.at(2).first().toString().endsWith(QLatin1String("c.jpg")));
    }

    // Two instances never both listen: the second to try finds the first answering and does not take its place.
    void aSecondListenerDoesNotStealTheName()
    {
        const QString name = uniqueName();
        SingleInstance first(name), second(name);
        QVERIFY(first.listen());
        QVERIFY(!second.listen());
        QSignalSpy opened(&first, &SingleInstance::openRequested);
        auto result = sendAsync(name, {QStringLiteral("x.jpg")});
        QCOMPARE(waitFor(result), SingleInstance::Result::Delivered);
        QTRY_COMPARE(opened.count(), 1); // the first one is still the one that answers
    }

#ifndef Q_OS_WIN
    // A crash leaves the socket file behind with nobody listening: the next start clears it and takes over.
    void aStaleSocketIsReplaced()
    {
        const QString name = uniqueName();
        QFile stale(name);
        QVERIFY(stale.open(QIODevice::WriteOnly));
        stale.close();
        QVERIFY(QFileInfo::exists(name));
        SingleInstance primary(name);
        QVERIFY(primary.listen());
        QSignalSpy opened(&primary, &SingleInstance::openRequested);
        auto result = sendAsync(name, {QStringLiteral("a.jpg")});
        QCOMPARE(waitFor(result), SingleInstance::Result::Delivered);
        QTRY_COMPARE(opened.count(), 1);
    }
#endif

    // Something accepts the connection but never answers (a hung instance): the sender gives up and says so, so the
    // caller starts on its own instead of waiting forever.
    void aSilentListenerIsReportedAsNoAnswer()
    {
        const QString name = uniqueName();
        QLocalServer silent;
        QVERIFY(silent.listen(name));
        auto result = sendAsync(name, {QStringLiteral("a.jpg")}, 300);
        QCOMPARE(waitFor(result), SingleInstance::Result::NoAnswer);
    }

    void aMalformedRequestIsIgnored()
    {
        const QString name = uniqueName();
        SingleInstance primary(name);
        QVERIFY(primary.listen());
        QSignalSpy opened(&primary, &SingleInstance::openRequested);
        QSignalSpy activated(&primary, &SingleInstance::activateRequested);

        QLocalSocket socket;
        socket.connectToServer(name);
        QVERIFY(socket.waitForConnected(2000));
        socket.write("this is not json\n");
        socket.flush();
        QTRY_VERIFY_WITH_TIMEOUT(socket.bytesAvailable() > 0 || socket.state() == QLocalSocket::UnconnectedState, 3000);
        QCOMPARE(socket.readLine().trimmed(), QByteArray("bad"));
        QTest::qWait(100);
        QCOMPARE(opened.count(), 0);
        QCOMPARE(activated.count(), 0);

        // and the instance still serves the next one
        auto result = sendAsync(name, {QStringLiteral("ok.jpg")});
        QCOMPARE(waitFor(result), SingleInstance::Result::Delivered);
        QTRY_COMPARE(opened.count(), 1);
    }

    void anOversizedRequestIsDropped()
    {
        const QString name = uniqueName();
        SingleInstance primary(name);
        QVERIFY(primary.listen());
        QSignalSpy opened(&primary, &SingleInstance::openRequested);

        QLocalSocket socket;
        socket.connectToServer(name);
        QVERIFY(socket.waitForConnected(2000));
        socket.write(QByteArray(200 * 1024, 'x')); // no newline, far past the limit
        socket.flush();
        QTRY_VERIFY_WITH_TIMEOUT(socket.state() == QLocalSocket::UnconnectedState, 3000);
        QCOMPARE(opened.count(), 0);
    }
};

QTEST_MAIN(TestSingleInstance)
#include "test_single_instance.moc"
