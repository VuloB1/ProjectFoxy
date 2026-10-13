#include "DevServer.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QMutexLocker>
#include <algorithm>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTcpServer>
#include <QTcpSocket>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#endif

namespace {

QQuickItem *itemNamed(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name)
        return root;
    for (QQuickItem *child : root->childItems())
        if (QQuickItem *found = itemNamed(child, name))
            return found;
    return nullptr;
}

void listItems(QQuickItem *item, QStringList &out)
{
    if (!item->objectName().isEmpty()) {
        const QPointF at = item->mapToScene(QPointF(0, 0));
        const QVariant text = item->property("text");
        out << QStringLiteral("%1 x=%2 y=%3 w=%4 h=%5 visible=%6 enabled=%7%8")
                   .arg(item->objectName()).arg(at.x()).arg(at.y()).arg(item->width()).arg(item->height())
                   .arg(item->isVisible()).arg(item->isEnabled())
                   .arg(text.isValid() ? QStringLiteral(" text=\"%1\"").arg(text.toString()) : QString());
    }
    for (QQuickItem *child : item->childItems())
        listItems(child, out);
}

} // namespace

DevServer::DevServer(QQuickWindow *window, quint16 port, QObject *parent)
    : QObject(parent), m_window(window), m_server(new QTcpServer(this))
{
    m_clock.start();
    // Direct connections: with the threaded render loop these run on the render thread, with the GUI thread waiting.
    connect(window, &QQuickWindow::beforeSynchronizing, this, [this]() { m_syncStart = m_clock.nsecsElapsed(); }, Qt::DirectConnection);
    connect(window, &QQuickWindow::afterSynchronizing, this, [this]() {
        m_current.syncMs = (m_clock.nsecsElapsed() - m_syncStart) / 1e6;
    }, Qt::DirectConnection);
    connect(window, &QQuickWindow::beforeRendering, this, [this]() { m_renderStart = m_clock.nsecsElapsed(); }, Qt::DirectConnection);
    connect(window, &QQuickWindow::afterRendering, this, [this]() {
        m_current.renderMs = (m_clock.nsecsElapsed() - m_renderStart) / 1e6;
    }, Qt::DirectConnection);
    connect(window, &QQuickWindow::frameSwapped, this, [this]() {
        QMutexLocker lock(&m_framesLock);
        if (m_recording) {
            m_current.swapNs = m_clock.nsecsElapsed();
            m_current.width = m_window->width();
            m_current.height = m_window->height();
            m_frames.append(m_current);
        }
        m_current = Frame();
    }, Qt::DirectConnection);

    // The GUI thread's heartbeat: every 1 ms it notes how long since the last one. A big gap is that thread being busy.
    m_beat.setTimerType(Qt::PreciseTimer);
    m_beat.setInterval(1);
    connect(&m_beat, &QTimer::timeout, this, [this]() {
        const qint64 now = m_clock.nsecsElapsed();
        const int w = m_window->width(), h = m_window->height();
        if (m_recording && m_lastBeatNs) {
            ++m_beatCount;
            const double late = (now - m_lastBeatNs) / 1e6 - 1.0;
            if (late > 4)
                m_beats.append({late, w, h, w != m_lastBeatW || h != m_lastBeatH});
        }
        m_lastBeatNs = now;
        m_lastBeatW = w;
        m_lastBeatH = h;
    });
    m_beat.start();
#ifdef Q_OS_WIN
    m_guiThreadId = GetCurrentThreadId();
    m_watchdog = std::thread([this]() { watchdog(); });
#endif

    m_server->listen(QHostAddress::LocalHost, port);
    connect(m_server, &QTcpServer::newConnection, this, [this]() {
        while (QTcpSocket *socket = m_server->nextPendingConnection()) {
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() {
                while (socket->canReadLine())
                    socket->write((run(QString::fromUtf8(socket->readLine()).trimmed()) + QLatin1String("\n.\n")).toUtf8());
            });
        }
    });
}

QString DevServer::run(const QString &line)
{
    const QStringList words = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (words.isEmpty())
        return QStringLiteral("error: empty command");
    const QString &command = words.first();

    if (command == QLatin1String("items")) {
        QStringList out;
        listItems(m_window->contentItem(), out);
        return out.join(QLatin1Char('\n'));
    }
    if (command == QLatin1String("frames") && words.size() == 2) {
        if (words[1] == QLatin1String("start")) {
            QMutexLocker lock(&m_framesLock);
            m_frames.clear();
            m_beats.clear();
            m_beatCount = 0;
            m_lastBeatNs = 0;
            {
                QMutexLocker samples(&m_sampleLock);
                m_sampleCounts.clear();
                m_sampleStacks.clear();
            }
            m_recording = true;
            return QStringLiteral("recording");
        }
        if (words[1] == QLatin1String("stop"))
            return framesReport();
    }
    if (command == QLatin1String("grab") && words.size() == 2)
        return m_window->grabWindow().save(words[1]) ? QStringLiteral("ok") : QStringLiteral("error: cannot write the file");

    if ((command == QLatin1String("get") && words.size() == 3) || (command == QLatin1String("click") && words.size() == 2)) {
        QQuickItem *item = itemNamed(m_window->contentItem(), words[1]);
        if (!item)
            return QStringLiteral("error: no item named %1").arg(words[1]);
        if (command == QLatin1String("get"))
            return item->property(words[2].toUtf8().constData()).toString();
        const QPointF centre = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
        for (QEvent::Type type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
            QMouseEvent event(type, centre, centre, m_window->mapToGlobal(centre), Qt::LeftButton,
                              type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(m_window, &event);
        }
        return QStringLiteral("ok");
    }
    return QStringLiteral("error: unknown command (items | get <name> <property> | click <name> | grab <file.png> | frames start|stop)");
}

QString DevServer::framesReport()
{
    QVector<Frame> frames;
    {
        QMutexLocker lock(&m_framesLock);
        m_recording = false;
        frames = m_frames;
    }
    if (frames.size() < 2)
        return QStringLiteral("fewer than 2 frames recorded");

    // The interval of a frame is the time since the previous one was presented; gaps while nothing changed are not
    // frames the user waited for, so only intervals up to 500 ms count.
    QVector<double> gaps;
    QVector<int> worst; // indexes into frames, by interval
    for (int i = 1; i < frames.size(); ++i) {
        const double gap = (frames[i].swapNs - frames[i - 1].swapNs) / 1e6;
        if (gap <= 500)
            gaps << gap;
    }
    if (gaps.isEmpty())
        return QStringLiteral("no frame followed another within 500 ms");
    QVector<double> sorted = gaps;
    std::sort(sorted.begin(), sorted.end());
    auto percentile = [&](double q) { return sorted[qMin(int(q * sorted.size()), int(sorted.size()) - 1)]; };
    int over33 = 0, over50 = 0, over100 = 0;
    double maxSync = 0, maxRender = 0;
    for (double g : gaps) { over33 += g > 33.4; over50 += g > 50; over100 += g > 100; }
    for (const Frame &f : frames) { maxSync = qMax(maxSync, f.syncMs); maxRender = qMax(maxRender, f.renderMs); }

    QString out = QStringLiteral("frames=%1 intervals=%2 median=%3 ms p95=%4 ms max=%5 ms | over 33 ms: %6, over 50 ms: %7, over 100 ms: %8 | "
                                 "sync max %9 ms, render max %10 ms")
                      .arg(frames.size()).arg(gaps.size()).arg(percentile(0.5), 0, 'f', 1).arg(percentile(0.95), 0, 'f', 1)
                      .arg(sorted.last(), 0, 'f', 1).arg(over33).arg(over50).arg(over100).arg(maxSync, 0, 'f', 1).arg(maxRender, 0, 'f', 1);
    // The slowest frames, with what they were made of.
    QVector<int> order;
    for (int i = 1; i < frames.size(); ++i) order << i;
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return frames[a].swapNs - frames[a - 1].swapNs > frames[b].swapNs - frames[b - 1].swapNs; });
    out += QLatin1String("\nslowest frames (interval, sync, render, window size):");
    int shown = 0;
    for (int i : order) {
        const double gap = (frames[i].swapNs - frames[i - 1].swapNs) / 1e6;
        if (gap > 500) continue;
        out += QStringLiteral("\n  %1 ms  sync %2  render %3  %4x%5").arg(gap, 0, 'f', 1).arg(frames[i].syncMs, 0, 'f', 1)
                   .arg(frames[i].renderMs, 0, 'f', 1).arg(frames[i].width).arg(frames[i].height);
        if (++shown == 8) break;
    }
    // GUI thread: gaps between heartbeats longer than ~5 ms (it was busy, or the system did not run it).
    QVector<Beat> beats = m_beats;
    std::sort(beats.begin(), beats.end(), [](const Beat &a, const Beat &b) { return a.lateMs > b.lateMs; });
    // Buckets (ms late): 4-16, 16-33, 33-50, 50-100, 100-500, over 500; "while resizing" = the size changed in the gap.
    int counts[2][6] = {};
    for (const Beat &b : beats) {
        const int k = b.lateMs > 500 ? 5 : b.lateMs > 100 ? 4 : b.lateMs > 50 ? 3 : b.lateMs > 33 ? 2 : b.lateMs > 16 ? 1 : 0;
        ++counts[b.resized ? 1 : 0][k];
    }
    out += QStringLiteral("\nGUI thread heartbeat (1 ms): %1 beats, %2 late by over 4 ms").arg(m_beatCount).arg(beats.size());
    for (int resized = 1; resized >= 0; --resized)
        out += QStringLiteral("\n  %1: 4-16 ms %2 | 16-33 %3 | 33-50 %4 | 50-100 %5 | 100-500 %6 | over 500 %7")
                   .arg(resized ? QStringLiteral("size changed during the gap") : QStringLiteral("window still           "))
                   .arg(counts[resized][0]).arg(counts[resized][1]).arg(counts[resized][2])
                   .arg(counts[resized][3]).arg(counts[resized][4]).arg(counts[resized][5]);
    int shownBeats = 0;
    for (const Beat &b : beats) { // the worst gaps that happened while the size was changing
        if (!b.resized) continue;
        out += QStringLiteral("\n  late %1 ms  window %2x%3").arg(b.lateMs, 0, 'f', 1).arg(b.width).arg(b.height);
        if (++shownBeats == 8) break;
    }
    {
        QMutexLocker samples(&m_sampleLock);
        if (!m_sampleCounts.isEmpty()) {
            for (const QString &who : {QStringLiteral("GUI   "), QStringLiteral("RENDER")}) {
                QVector<QPair<int, QString>> ranked;
                int total = 0;
                for (auto it = m_sampleCounts.begin(); it != m_sampleCounts.end(); ++it) {
                    if (!it.key().startsWith(who))
                        continue;
                    ranked.append({it.value(), it.key()});
                    total += it.value();
                }
                std::sort(ranked.begin(), ranked.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
                out += QStringLiteral("%1stack samples of the %2 (for RENDER: the other threads in Qt Quick, Direct3D or DXGI) while the GUI thread was stalled (%3 in all):").arg(QLatin1Char(10)).arg(who.trimmed()).arg(total);
                for (int i = 0; i < qMin(4, int(ranked.size())); ++i)
                    out += QStringLiteral("%1  %2 x  %3").arg(QLatin1Char(10)).arg(ranked[i].first).arg(m_sampleStacks.value(ranked[i].second));
            }
        } else {
            out += QStringLiteral("%1GUI thread stack samples: none (it never stalled for 40 ms)").arg(QLatin1Char(10));
        }
    }
    return out;
}

DevServer::~DevServer()
{
    m_stopWatchdog = true;
    if (m_watchdog.joinable())
        m_watchdog.join();
}

#ifdef Q_OS_WIN
namespace {

QString describe(HANDLE process, DWORD64 address)
{
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(address), &module);
    wchar_t path[MAX_PATH] = L"?";
    if (module)
        GetModuleFileNameW(module, path, MAX_PATH);
    const QString name = QFileInfo(QString::fromWCharArray(path)).fileName();
    alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 256];
    auto *symbol = reinterpret_cast<SYMBOL_INFO *>(buffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 255;
    DWORD64 displacement = 0;
    if (SymFromAddr(process, address, &displacement, symbol))
        return QStringLiteral("%1!%2").arg(name, QString::fromLatin1(&symbol->Name[0])); // not the array itself: Name[1] would be taken as one character
    return QStringLiteral("%1+0x%2").arg(name).arg(address - reinterpret_cast<DWORD64>(module), 0, 16);
}

} // namespace

// Raw return addresses of a thread (it is suspended for the time: nothing in here may take a lock it could hold).
static int captureStack(HANDLE thread, DWORD64 *addresses, int capacity)
{
    int count = 0;
    if (SuspendThread(thread) == DWORD(-1))
        return 0;
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_FULL;
    if (GetThreadContext(thread, &context)) {
        while (count < capacity && context.Rip) {
            addresses[count++] = context.Rip;
            DWORD64 base = 0;
            RUNTIME_FUNCTION *entry = RtlLookupFunctionEntry(context.Rip, &base, nullptr);
            if (!entry) {
                context.Rip = *reinterpret_cast<DWORD64 *>(context.Rsp);
                context.Rsp += 8;
            } else {
                void *handlerData = nullptr;
                DWORD64 frame = 0;
                RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, context.Rip, entry, &context, &handlerData, &frame, nullptr);
            }
        }
    }
    ResumeThread(thread);
    return count;
}

void DevServer::watchdog()
{
    const HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, nullptr, TRUE);
    HANDLE gui = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, m_guiThreadId);
    if (!gui)
        return;
    const DWORD self = GetCurrentThreadId();
    while (!m_stopWatchdog) {
        Sleep(5);
        const qint64 last = m_lastBeatNs;
        if (!m_recording || !last || (m_clock.nsecsElapsed() - last) / 1e6 < 40)
            continue;

        auto record = [&](const char *who, const DWORD64 *addresses, int count) {
            if (count == 0)
                return;
            QStringList frames;
            for (int i = 0; i < count; ++i)
                frames << describe(process, addresses[i]);
            const QString joined = frames.join(QStringLiteral(" < "));
            if (QLatin1String(who) != QLatin1String("GUI   ")
                && !joined.contains(QLatin1String("Qt6Quick")) && !joined.contains(QLatin1String("d3d11"))
                && !joined.contains(QLatin1String("dxgi")) && !joined.contains(QLatin1String("dwm")))
                return; // an idle pool thread or the like
            const QString key = QLatin1String(who) + QLatin1Char(' ') + frames.mid(0, 4).join(QLatin1Char('|'));
            QMutexLocker samples(&m_sampleLock);
            ++m_sampleCounts[key];
            if (!m_sampleStacks.contains(key))
                m_sampleStacks[key] = joined;
        };

        DWORD64 addresses[14];
        record("GUI   ", addresses, captureStack(gui, addresses, 14));
        // Every other thread of the process (this one and the GUI one aside): the render thread is one of them.
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
            continue;
        THREADENTRY32 entry = {sizeof entry};
        for (BOOL ok = Thread32First(snapshot, &entry); ok; ok = Thread32Next(snapshot, &entry)) {
            if (entry.th32OwnerProcessID != GetCurrentProcessId() || entry.th32ThreadID == self || entry.th32ThreadID == m_guiThreadId)
                continue;
            HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID);
            if (!thread)
                continue;
            record("RENDER", addresses, captureStack(thread, addresses, 14));
            CloseHandle(thread);
        }
        CloseHandle(snapshot);
    }
    CloseHandle(gui);
}
#else
void DevServer::watchdog() {}
#endif
