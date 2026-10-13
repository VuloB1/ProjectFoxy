#include "WindowsFrame.h"

#include <QAbstractNativeEventFilter>
#include <QGuiApplication>
#include <QPointer>
#include <QQuickItem>

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

class Filter : public QAbstractNativeEventFilter
{
public:
    explicit Filter(QQuickWindow *window)
        : m_window(window), m_hwnd(reinterpret_cast<HWND>(window->winId())) {}

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
            // The client area is the whole window (maximised too, see WM_GETMINMAXINFO).
            *result = 0;
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
            *result = 0;
            return true;
        }

        case WM_NCHITTEST: {
            *result = hitTest(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));
            setMaximizeHover(*result == HTMAXBUTTON);
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
                *result = 0;
                return true;
            }
            return false;
        case WM_NCLBUTTONUP:
            if (msg->wParam == HTMAXBUTTON) {
                ShowWindow(m_hwnd, IsZoomed(m_hwnd) ? SW_RESTORE : SW_MAXIMIZE);
                *result = 0;
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
    const MARGINS thin = {1, 1, 1, 1};
    if (!skip.contains("dwm"))
        DwmExtendFrameIntoClientArea(hwnd, &thin);

    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    if (!skip.contains("style"))
        style |= WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);
    // Make Windows ask for the client area again (WM_NCCALCSIZE).
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
