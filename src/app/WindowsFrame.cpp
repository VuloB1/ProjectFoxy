#include "WindowsFrame.h"

#include <QAbstractNativeEventFilter>
#include <QGuiApplication>
#include <QPointer>
#include <QTimer>
#include <QQuickItem>
#include <QQuickWindow>

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

namespace {

// The visual tree, by objectName (findChild follows QObject parents, which are not always the visual ones).
QQuickItem *itemNamed(QQuickItem *root, const char *name)
{
    if (root->objectName() == QLatin1String(name))
        return root;
    for (QQuickItem *child : root->childItems())
        if (QQuickItem *found = itemNamed(child, name))
            return found;
    return nullptr;
}

// The deepest visible item under `scenePoint` (window coordinates, in Qt's units).
QQuickItem *deepestAt(QQuickItem *root, QPointF scenePoint)
{
    QQuickItem *item = root;
    QPointF local = root->mapFromScene(scenePoint);
    while (QQuickItem *child = item->childAt(local.x(), local.y())) {
        local = item->mapToItem(child, local);
        item = child;
    }
    return item;
}

// A button (or anything that says `captionInteractive: true`) inside the title bar: Windows must leave the mouse to Qt
// there instead of treating it as caption, or a click would start a window drag.
bool interactiveInBar(QQuickItem *bar, QPointF scenePoint)
{
    for (QQuickItem *item = deepestAt(bar, scenePoint); item && item != bar; item = item->parentItem())
        if (item->inherits("QQuickAbstractButton") || item->property("captionInteractive").toBool())
            return true;
    return false;
}

class Filter : public QAbstractNativeEventFilter
{
public:
    explicit Filter(QQuickWindow *window)
        : m_window(window), m_hwnd(reinterpret_cast<HWND>(window->winId())) {}

    // Qt passes a null `result` for messages that come through the thread's message queue (a posted mouse-up of the
    // caption buttons does) instead of the window procedure, so it must be checked before it is written.
    static void setResult(qintptr *result, qintptr value)
    {
        if (result)
            *result = value;
    }

    bool nativeEventFilter(const QByteArray &type, void *message, qintptr *result) override
    {
        if (type != "windows_generic_MSG" || !m_window)
            return false;
        MSG *msg = static_cast<MSG *>(message);
        if (msg->hwnd != m_hwnd)
            return false;

        switch (msg->message) {
        case WM_NCCALCSIZE:
            if (!msg->wParam)
                return false;
            // The client area is the whole window. Maximised, a window that has a caption (see installWindowsFrame) is
            // laid out by Windows with its frame hanging over the screen edges; the client is the monitor's work area
            // instead (and, as in WM_GETMINMAXINFO, an auto-hiding taskbar keeps one pixel free at the bottom).
            if (IsZoomed(m_hwnd)) {
                MONITORINFO monitor = {sizeof monitor};
                if (GetMonitorInfo(MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST), &monitor)) {
                    auto *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
                    params->rgrc[0] = monitor.rcWork;
                    if (EqualRect(&monitor.rcWork, &monitor.rcMonitor))
                        params->rgrc[0].bottom -= 1;
                }
            }
            setResult(result, 0);
            return true;

        // Maximised, the window is the monitor's work area (by default it would overhang the screen by its frame and
        // cover the taskbar). Qt's own limits are not applied when this answers, so the minimum size is here too.
        case WM_GETMINMAXINFO: {
            auto *info = reinterpret_cast<MINMAXINFO *>(msg->lParam);
            MONITORINFO monitor = {sizeof monitor};
            if (GetMonitorInfo(MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST), &monitor)) {
                const RECT &work = monitor.rcWork;
                info->ptMaxPosition = {work.left - monitor.rcMonitor.left, work.top - monitor.rcMonitor.top};
                info->ptMaxSize = {work.right - work.left, work.bottom - work.top};
                // shortcut: an auto-hiding taskbar leaves no work-area gap, so one pixel is kept free at the
                // bottom for it to be reachable; it is the usual place, not checked for the other edges.
                if (EqualRect(&work, &monitor.rcMonitor))
                    info->ptMaxSize.y -= 1;
            }
            const double scale = m_window->devicePixelRatio();
            info->ptMinTrackSize = {qRound(m_window->minimumWidth() * scale), qRound(m_window->minimumHeight() * scale)};
            setResult(result, 0);
            return true;
        }

        case WM_NCHITTEST: {
            const LRESULT hit = hitTest(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));
            setResult(result, hit);
            setMaximizeHover(hit == HTMAXBUTTON);
            return true;
        }

        case WM_NCMOUSEMOVE:
            if (msg->wParam == HTMAXBUTTON) {
                TRACKMOUSEEVENT track = {sizeof track, TME_LEAVE | TME_NONCLIENT, m_hwnd, 0};
                TrackMouseEvent(&track);
            }
            return false;

        case WM_NCMOUSELEAVE:
            setMaximizeHover(false);
            return false;

        // The maximise button is not "client" for Qt, so its click arrives here.
        case WM_NCLBUTTONDOWN:
            if (msg->wParam == HTMAXBUTTON) {
                setResult(result, 0);
                return true;
            }
            return false;
        case WM_NCLBUTTONUP:
            if (msg->wParam == HTMAXBUTTON) {
                ShowWindow(m_hwnd, IsZoomed(m_hwnd) ? SW_RESTORE : SW_MAXIMIZE);
                setResult(result, 0);
                return true;
            }
            return false;
        }
        return false;
    }

private:
    LRESULT hitTest(int screenX, int screenY) const
    {
        POINT origin = {0, 0};
        ClientToScreen(m_hwnd, &origin);
        RECT client;
        GetClientRect(m_hwnd, &client);
        const int x = screenX - origin.x, y = screenY - origin.y;
        const int w = client.right, h = client.bottom;
        if (x < 0 || y < 0 || x >= w || y >= h)
            return HTNOWHERE;

        const UINT dpi = GetDpiForWindow(m_hwnd);
        if (!IsZoomed(m_hwnd)) {
            const int b = GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
            const bool left = x < b, right = x >= w - b, top = y < b, bottom = y >= h - b;
            if (top && left) return HTTOPLEFT;
            if (top && right) return HTTOPRIGHT;
            if (bottom && left) return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left) return HTLEFT;
            if (right) return HTRIGHT;
            if (top) return HTTOP;
            if (bottom) return HTBOTTOM;
        }

        const double scale = m_window->devicePixelRatio();
        const double qx = x / scale, qy = y / scale;
        if (const QQuickItem *bar = itemNamed(m_window->contentItem(), "titleBar")) {
            if (qy < bar->height()) {
                // The three caption buttons are the right end of the bar; the maximise one is the middle.
                if (const QQuickItem *max = itemNamed(m_window->contentItem(), "maximizeButton")) {
                    const QPointF at = max->mapToScene(QPointF(0, 0));
                    if (qx >= at.x() && qx < at.x() + max->width())
                        return HTMAXBUTTON;
                    if (qx >= at.x() - max->width() && qx < at.x() + max->width() * 2)
                        return HTCLIENT; // minimise / close: Qt delivers them to the QML buttons
                }
                if (interactiveInBar(const_cast<QQuickItem *>(bar), QPointF(qx, qy)))
                    return HTCLIENT; // a tool button: Qt delivers it to the QML
                return HTCAPTION;
            }
        }
        return HTCLIENT;
    }

    void setMaximizeHover(bool on) const
    {
        if (auto *max = itemNamed(m_window->contentItem(), "maximizeButton"))
            if (max->property("nativeHover").toBool() != on)
                max->setProperty("nativeHover", on);
    }

    QPointer<QQuickWindow> m_window;
    HWND m_hwnd;
};

} // namespace

void installWindowsFrame(QQuickWindow *window)
{
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    auto *filter = new Filter(window);
    qApp->installNativeEventFilter(filter);
    QObject::connect(window, &QObject::destroyed, qApp, [filter]() {
        qApp->removeNativeEventFilter(filter);
        delete filter;
    });

#ifdef PROJECTFOXY_DEV_SERVER
    const QByteArray skip = qgetenv("PROJECTFOXY_FRAME_SKIP"); // "round", "dwm", "style": to find which one of these shows
#else
    const QByteArray skip;
#endif
    const DWORD round = 2; // DWMWCP_ROUND
    if (!skip.contains("round"))
        DwmSetWindowAttribute(hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &round, sizeof round);
    // Until the first frame is drawn Windows paints the window with the class background, white: paint it
    // the theme's colour instead (the window's own colour is that, see Main.qml).
    const QColor background = window->color();
    SetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND,
                     reinterpret_cast<LONG_PTR>(CreateSolidBrush(RGB(background.red(), background.green(), background.blue()))));
    // Keep the window out of sight (cloaked: it is still there and still renders) until its first frame has been
    // drawn: before that Windows shows it white for a moment.
    const BOOL cloak = TRUE;
    DwmSetWindowAttribute(hwnd, 13 /* DWMWA_CLOAK */, &cloak, sizeof cloak);
    auto reveal = [hwnd]() {
        const BOOL off = FALSE;
        DwmSetWindowAttribute(hwnd, 13 /* DWMWA_CLOAK */, &off, sizeof off);
    };
    auto *once = new QMetaObject::Connection;
    *once = QObject::connect(window, &QQuickWindow::frameSwapped, window, [reveal, once]() {
        QObject::disconnect(*once);
        delete once;
        reveal();
    });
    QTimer::singleShot(2000, window, reveal); // never leave it hidden if no frame ever comes
    const MARGINS thin = {1, 1, 1, 1};
    if (!skip.contains("dwm"))
        DwmExtendFrameIntoClientArea(hwnd, &thin);

    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    if (!skip.contains("style"))
        style |= WS_CAPTION | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU; // WS_CAPTION: Windows animates minimise and restore only for windows that have a caption
    SetWindowLongPtr(hwnd, GWL_STYLE, style);
    // Make Windows ask for the client area again (WM_NCCALCSIZE).
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
