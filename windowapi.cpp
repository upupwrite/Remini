#include "windowapi.h"

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
WindowApi& WindowApi::instance()
{
    static WindowApi s_instance;
    return s_instance;
}

WindowApi::WindowApi(QObject *parent)
    : QObject(parent)
{
}

WindowApi::~WindowApi()
{
    cleanUp();
}

// ============================ Windows ============================
#ifdef Q_OS_WIN

HHOOK WindowApi::SetWindowsHookExInvoke(int idHook, HOOKPROC lpfn, HINSTANCE hmod, DWORD dwThreadId)
{
    return SetWindowsHookExW(idHook, lpfn, hmod, dwThreadId);
}

BOOL WindowApi::UnhookWindowsHookExInoke(HHOOK hhk)
{
    return UnhookWindowsHookEx(hhk);
}

LRESULT WindowApi::CallNextHookExInvoke(HHOOK hhk, int nCode, WPARAM wParam, LPARAM lParam)
{
    return CallNextHookEx(hhk, nCode, wParam, lParam);
}

WId WindowApi::GetForegroundWindowInvoke()
{
    return reinterpret_cast<WId>(GetForegroundWindow());
}

void WindowApi::installHook()
{
    keyboardProcHook = SetWindowsHookExInvoke(WH_KEYBOARD_LL, detectKeys,
                                              GetModuleHandle(nullptr), 0);
}

void WindowApi::cleanUp()
{
    if (keyboardProcHook) {
        UnhookWindowsHookExInoke(keyboardProcHook);
        keyboardProcHook = nullptr;
    }
}

LRESULT WindowApi::detectKeys(int code, WPARAM wParam, LPARAM lParam)
{
    if (code >= 0) {
        bool isKeyDown = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
        bool isKeyUp   = wParam == WM_KEYUP   || wParam == WM_SYSKEYUP;

        KBDLLHOOKSTRUCT *kbStruct = (KBDLLHOOKSTRUCT*)lParam;
        DWORD vkCode = kbStruct->vkCode;

        if (vkCode == KEY_J || vkCode == KEY_ALT) {
            if (isKeyDown) {
                if (vkCode == KEY_J)   isKeyJPressedDown   = true;
                if (vkCode == KEY_ALT) isKeyAltPressedDown = true;
            }
            if (isKeyUp) {
                if (vkCode == KEY_J)   isKeyJPressedDown   = false;
                if (vkCode == KEY_ALT) isKeyAltPressedDown = false;
            }
            if (isKeyAltPressedDown && isKeyJPressedDown) {
                emit WindowApi::instance().showApp();
                isKeyJPressedDown = false;
                return 1;
            }
        }
    }
    return instance().CallNextHookExInvoke(keyboardProcHook, code, wParam, lParam);
}

// ============================ Linux ============================
#elif defined(Q_OS_LINUX)

// Qt headers first.
#include <QGuiApplication>
#include <QTimer>
#include <QString>

// X11 last, so its macros (Bool, None, Status, ...) cannot break Qt headers.
#include <X11/Xlib.h>
#include <X11/keysym.h>

bool WindowApi::isWayland() const
{
    const QString platform = QGuiApplication::platformName();
    return platform.contains(QStringLiteral("wayland"), Qt::CaseInsensitive);
}

void WindowApi::installHook()
{
    if (isWayland()) {
        hookInstalled = false;
        return;
    }

    Display *dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        hookInstalled = false;
        return;
    }
    display = dpy;

    Window root = DefaultRootWindow(dpy);
    rootWindow  = static_cast<unsigned long>(root);
    keycodeJ    = XKeysymToKeycode(dpy, XK_j);

    const unsigned int locks[] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
    for (unsigned int lock : locks) {
        XGrabKey(dpy, keycodeJ, Mod1Mask | lock, root,
                 True, GrabModeAsync, GrabModeAsync);
    }
    XFlush(dpy);

    timer = new QTimer(this);
    timer->setInterval(50);
    connect(timer, &QTimer::timeout, this, &WindowApi::pollEvents);
    timer->start();

    hookInstalled = true;
}

void WindowApi::cleanUp()
{
    if (!hookInstalled)
        return;

    if (timer) {
        timer->stop();
        timer->deleteLater();
        timer = nullptr;
    }

    Display *dpy = static_cast<Display*>(display);
    if (dpy) {
        Window root = static_cast<Window>(rootWindow);
        const unsigned int locks[] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
        for (unsigned int lock : locks) {
            XUngrabKey(dpy, keycodeJ, Mod1Mask | lock, root);
        }
        XCloseDisplay(dpy);
        display = nullptr;
    }

    hookInstalled = false;
}

WId WindowApi::GetForegroundWindowInvoke()
{
    if (isWayland())
        return 0;

    Display *dpy = static_cast<Display*>(display);
    if (!dpy) {
        dpy = XOpenDisplay(nullptr);
        if (!dpy)
            return 0;
        display = dpy;
    }

    Window focused = 0;
    int revert = 0;
    XGetInputFocus(dpy, &focused, &revert);
    return static_cast<WId>(focused);
}

void WindowApi::pollEvents()
{
    Display *dpy = static_cast<Display*>(display);
    if (!dpy)
        return;

    while (XPending(dpy)) {
        XEvent ev;
        XNextEvent(dpy, &ev);
        if (ev.type == KeyPress)
            emit showApp();
    }
}

#endif
