#pragma once

#include <QElapsedTimer>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QTimer>
#include <QVector>
#include <atomic>
#include <thread>

class QQuickWindow;
class QTcpServer;

// Development only (built with -DPROJECTFOXY_DEV_SERVER=ON, started by setting PROJECTFOXY_DEV_PORT): a line-based
// text channel on 127.0.0.1 to look at and drive the real window from a script (tools/devctl.py). Never in a release.
//   items                       every item with an objectName: name, scene rect, visible, enabled, text
//   get <name> <property>       a property of the item
//   click <name>                a left click in the middle of the item
//   grab <file.png>             the window as a PNG
//   frames start | frames stop  records how long every frame takes to prepare (sync), draw (render) and arrive after the
//                               previous one while you use the window, and how late the GUI thread answers a 1 ms
//                               heartbeat (a late one means that thread was busy); stop reports the numbers and the worst
class DevServer : public QObject
{
public:
    DevServer(QQuickWindow *window, quint16 port, QObject *parent);
    ~DevServer() override;

private:
    QString run(const QString &line);
    QString framesReport();

    struct Frame { qint64 swapNs = 0; double syncMs = 0, renderMs = 0; int width = 0, height = 0; };
    QMutex m_framesLock;
    QVector<Frame> m_frames;
    QElapsedTimer m_clock;
    qint64 m_syncStart = 0, m_renderStart = 0;
    Frame m_current;
    std::atomic<bool> m_recording{false};
    QTimer m_beat;
    std::atomic<qint64> m_lastBeatNs{0};
    struct Beat { double lateMs; int width, height; bool resized; }; // resized: the size changed during the gap
    int m_lastBeatW = 0, m_lastBeatH = 0;
    QVector<Beat> m_beats; // GUI thread only, except while reporting
    qint64 m_beatCount = 0;

    // Windows: a thread that, while the GUI thread has not answered its heartbeat for 40 ms, notes what that thread is
    // executing (module!function of the top frames). Tells apart waiting on the GPU / the system from work in the program.
    void watchdog();
    std::thread m_watchdog;
    std::atomic<bool> m_stopWatchdog{false};
    unsigned long m_guiThreadId = 0;
    QMutex m_sampleLock;
    QMap<QString, int> m_sampleCounts;
    QMap<QString, QString> m_sampleStacks;

    QQuickWindow *m_window;
    QTcpServer *m_server;
};
