#include "TrayIcon.h"

#include <QQuickWindow>

#ifdef Q_OS_WIN
#include <malloc.h>
#include <windows.h>
#include <shellapi.h>

namespace {
constexpr UINT kCallback = WM_APP + 1;
constexpr UINT kOpen = 1, kQuit = 2;
const wchar_t kClass[] = L"ProjectFoxyTray";
}

struct TrayIcon::Native
{
    HWND hwnd = nullptr;
    UINT taskbarCreated = 0;
    TrayIcon *owner = nullptr;

    void add()
    {
        NOTIFYICONDATAW data{sizeof(data)};
        data.hWnd = hwnd;
        data.uID = 1;
        data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        data.uCallbackMessage = kCallback;
        data.hIcon = LoadIconW(GetModuleHandleW(nullptr), L"IDI_ICON1");
        owner->m_tip.left(120).toWCharArray(data.szTip);
        Shell_NotifyIconW(NIM_ADD, &data);
    }
    void remove()
    {
        NOTIFYICONDATAW data{sizeof(data)};
        data.hWnd = hwnd;
        data.uID = 1;
        Shell_NotifyIconW(NIM_DELETE, &data);
    }
    void menu()
    {
        HMENU m = CreatePopupMenu();
        AppendMenuW(m, MF_STRING, kOpen, reinterpret_cast<LPCWSTR>(owner->m_openText.utf16()));
        AppendMenuW(m, MF_STRING, kQuit, reinterpret_cast<LPCWSTR>(owner->m_quitText.utf16()));
        POINT p;
        GetCursorPos(&p);
        SetForegroundWindow(hwnd); // otherwise the menu does not close when clicking elsewhere
        const UINT cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, hwnd, nullptr);
        PostMessageW(hwnd, WM_NULL, 0, 0);
        DestroyMenu(m);
        if (cmd == kOpen)
            emit owner->activated();
        else if (cmd == kQuit)
            emit owner->quitRequested();
    }
};

static LRESULT CALLBACK trayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    auto *n = reinterpret_cast<TrayIcon::Native *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (n && msg == kCallback) {
        if (LOWORD(lp) == WM_LBUTTONUP || LOWORD(lp) == WM_LBUTTONDBLCLK)
            emit n->owner->activated();
        else if (LOWORD(lp) == WM_RBUTTONUP)
            n->menu();
        return 0;
    }
    if (n && msg == n->taskbarCreated && n->owner->visible()) // Explorer restarted: the icon is gone, put it back
        n->add();
    return DefWindowProcW(hwnd, msg, wp, lp);
}

TrayIcon::TrayIcon(QObject *parent) : QObject(parent), m_native(new Native)
{
    m_native->owner = this;
    m_native->taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    WNDCLASSW wc{};
    wc.lpfnWndProc = trayProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClass;
    RegisterClassW(&wc);
    m_native->hwnd = CreateWindowExW(0, kClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    SetWindowLongPtrW(m_native->hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(m_native));
}

TrayIcon::~TrayIcon()
{
    if (m_visible)
        m_native->remove();
    DestroyWindow(m_native->hwnd);
    delete m_native;
}

bool TrayIcon::available() const { return true; }

void TrayIcon::trimMemory()
{
    _heapmin(); // the free blocks of the C heap go back to Windows (the commit), not only out of the working set
    SetProcessWorkingSetSize(GetCurrentProcess(), SIZE_MAX, SIZE_MAX);
}

void TrayIcon::setGraphicsPersistent(QQuickWindow *window, bool on)
{
    window->setPersistentGraphics(on);
    window->setPersistentSceneGraph(on);
}

void TrayIcon::setVisible(bool on)
{
    if (on == m_visible)
        return;
    m_visible = on;
    on ? m_native->add() : m_native->remove();
    emit visibleChanged();
}

#else

struct TrayIcon::Native {};
TrayIcon::TrayIcon(QObject *parent) : QObject(parent) {}
TrayIcon::~TrayIcon() = default;
bool TrayIcon::available() const { return false; }
void TrayIcon::setVisible(bool) {}
void TrayIcon::trimMemory() {}
void TrayIcon::setGraphicsPersistent(QQuickWindow *, bool) {}

#endif
