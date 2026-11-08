#include "platform/common/WindowChrome.h"
// Win32 base types must precede Common Controls (including after formatting).
// clang-format off
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
// clang-format on
#include <cstring>

namespace daymark {
namespace {
LRESULT CALLBACK chromeSubclass(HWND hwnd, UINT message, WPARAM wParam,
                                LPARAM lParam, UINT_PTR id,
                                DWORD_PTR reference) {
    auto *binding = reinterpret_cast<WindowChrome *>(reference);
    MSG native{};
    native.hwnd = hwnd;
    native.message = message;
    native.wParam = wParam;
    native.lParam = lParam;
    qintptr result = 0;
    if (binding->handleNativeMessage("windows_generic_MSG", &native, &result))
        return result;
    if (message == WM_NCDESTROY)
        RemoveWindowSubclass(hwnd, chromeSubclass, id);
    return DefSubclassProc(hwnd, message, wParam, lParam);
}
} // namespace
void WindowChrome::detachNative() {
    if (m_nativeId)
        RemoveWindowSubclass(reinterpret_cast<HWND>(m_nativeId), chromeSubclass,
                             reinterpret_cast<UINT_PTR>(this));
    m_nativeId = 0;
}
QRect WindowChrome::nativeFrameGeometry() const {
    if (!m_controller || !m_controller->supported())
        return {};
    if (!m_window || !m_window->handle())
        return {};
    RECT frame{};
    if (!GetWindowRect(reinterpret_cast<HWND>(m_window->winId()), &frame))
        return {};
    return QRect(frame.left, frame.top, frame.right - frame.left,
                 frame.bottom - frame.top);
}
QByteArray WindowChrome::nativePlacement() const {
    if (!m_controller || !m_controller->supported() || !m_window || !m_window->handle())
        return {};
    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    if (!GetWindowPlacement(reinterpret_cast<HWND>(m_window->winId()), &placement))
        return {};
    return QByteArray(reinterpret_cast<const char *>(&placement), sizeof(placement));
}
bool WindowChrome::restoreNativePlacement(const QByteArray &bytes, bool visible) {
    if (!m_nativeId || bytes.size() != sizeof(WINDOWPLACEMENT))
        return true;
    WINDOWPLACEMENT placement{};
    std::memcpy(&placement, bytes.constData(), sizeof(placement));
    // Preserve native maximize/minimize state and its normal restore rectangle
    // across the non-client recalculation, without activating hidden tools.
    if (!visible)
        placement.showCmd = SW_HIDE;
    if (SetWindowPlacement(reinterpret_cast<HWND>(m_nativeId), &placement)) {
        return true;
    }
    m_nativeError = GetLastError();
    return false;
}
bool WindowChrome::restoreNativeFrameGeometry(const QRect &frame) {
    if (!m_nativeId)
        return true;
    if (SetWindowPos(reinterpret_cast<HWND>(m_nativeId), nullptr, frame.x(),
                     frame.y(), frame.width(), frame.height(),
                     SWP_NOZORDER | SWP_NOACTIVATE))
        return true;
    m_nativeError = GetLastError();
    return false;
}
bool WindowChrome::configureNative() {
    m_nativeError = 0;
    if (!m_controller || !m_controller->supported())
        return true;
    if (!m_window || !m_window->handle())
        return true;
    const auto hwnd = reinterpret_cast<HWND>(m_window->winId());
    constexpr LONG_PTR frameMask = WS_CAPTION | WS_THICKFRAME | WS_SYSMENU |
                                   WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
    if (m_nativeId != reinterpret_cast<quintptr>(hwnd))
        m_originalFrameStyle = GetWindowLongPtrW(hwnd, GWL_STYLE) & frameMask;
    m_nativeId = reinterpret_cast<quintptr>(hwnd);
    if (!SetWindowSubclass(hwnd, chromeSubclass,
                           reinterpret_cast<UINT_PTR>(this),
                           reinterpret_cast<DWORD_PTR>(this))) {
        m_nativeError = GetLastError();
        return false;
    }
    if (!m_custom && m_pressed && GetCapture() == hwnd)
        ReleaseCapture();
    auto style = (GetWindowLongPtrW(hwnd, GWL_STYLE) & ~frameMask) |
                 LONG_PTR(m_originalFrameStyle);
    if (m_custom) {
        style |= WS_CAPTION | WS_THICKFRAME | WS_SYSMENU;
        if (m_window->flags().testFlag(Qt::WindowMaximizeButtonHint))
            style |= WS_MAXIMIZEBOX;
        if (m_window->flags().testFlag(Qt::WindowMinimizeButtonHint))
            style |= WS_MINIMIZEBOX;
    }
    SetLastError(0);
    if (!SetWindowLongPtrW(hwnd, GWL_STYLE, style) && GetLastError()) {
        m_nativeError = GetLastError();
        return false;
    }
    const BOOL dark = m_dark;
    // Optional DWM decorations may be unsupported; never weaken frame behavior.
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
    if (!SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                          SWP_FRAMECHANGED)) {
        m_nativeError = GetLastError();
        return false;
    }
    return true;
}
void WindowChrome::popupSystemMenu(const QPoint &point) {
    const auto hwnd = reinterpret_cast<HWND>(m_nativeId);
    if (!hwnd)
        return;
    auto menu = GetSystemMenu(hwnd, FALSE);
    if (!menu)
        return;
    const UINT command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                          point.x(), point.y(), hwnd, nullptr);
    if (command)
        PostMessageW(hwnd, WM_SYSCOMMAND, command, 0);
}
void WindowChrome::showSystemMenu() {
    if (!m_window || !m_nativeId)
        return;
    POINT at{8, LONG(m_titleBar
                         ? m_titleBar->height() * m_window->devicePixelRatio()
                         : 32)};
    ClientToScreen(reinterpret_cast<HWND>(m_nativeId), &at);
    popupSystemMenu(QPoint(at.x, at.y));
}
bool WindowChrome::handleNativeMessage(const QByteArray &type, void *message,
                                       qintptr *result) {
    if (!m_custom || !m_nativeId || !m_window || !message || !result ||
        (type != "windows_generic_MSG" && type != "windows_dispatcher_MSG"))
        return false;
    auto *msg = static_cast<MSG *>(message);
    if (reinterpret_cast<quintptr>(msg->hwnd) != m_nativeId)
        return false;
    auto local = [this](POINT at) {
        ScreenToClient(reinterpret_cast<HWND>(m_nativeId), &at);
        return QPointF(at.x / m_window->devicePixelRatio(),
                       at.y / m_window->devicePixelRatio());
    };
    switch (msg->message) {
    case WM_NCCALCSIZE: {
        if (IsZoomed(msg->hwnd)) {
            MONITORINFO monitor{};
            monitor.cbSize = sizeof(monitor);
            if (GetMonitorInfoW(
                    MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST),
                    &monitor)) {
                auto *rect =
                    msg->wParam
                        ? &reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam)
                               ->rgrc[0]
                        : reinterpret_cast<RECT *>(msg->lParam);
                *rect = monitor.rcWork;
            }
        }
        *result = 0;
        return true;
    }
    case WM_NCHITTEST: {
        const UINT dpi = GetDpiForWindow(msg->hwnd);
        const qreal border = (GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi) +
                              GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi)) /
                             m_window->devicePixelRatio();
        *result = hitTest(
            local({GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)}),
            qMax(1.0, border));
        return true;
    }
    case WM_NCMOUSEMOVE: {
        setInteraction(msg->wParam == HTMAXBUTTON, m_pressed);
        TRACKMOUSEEVENT track{sizeof(TRACKMOUSEEVENT),
                              TME_LEAVE | TME_NONCLIENT, msg->hwnd, 0};
        TrackMouseEvent(&track);
        return false; // DefWindowProc retains the Windows 11 Snap hover
                      // mechanism.
    }
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONDBLCLK:
        if (msg->wParam == HTMAXBUTTON) {
            setInteraction(true, true);
            SetCapture(msg->hwnd);
            *result = 0;
            return true;
        }
        break;
    case WM_MOUSEMOVE:
        if (m_pressed) {
            POINT at{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
            ClientToScreen(msg->hwnd, &at);
            setInteraction(contains(m_maximizeButton, local(at)), true);
            *result = 0;
            return true;
        }
        break;
    case WM_NCLBUTTONUP:
    case WM_LBUTTONUP:
        if (m_pressed) {
            POINT at{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
            if (msg->message == WM_LBUTTONUP)
                ClientToScreen(msg->hwnd, &at);
            const bool trigger = contains(m_maximizeButton, local(at));
            setInteraction(trigger, false);
            ReleaseCapture();
            if (trigger)
                toggleMaximize();
            *result = 0;
            return true;
        }
        break;
    case WM_NCMOUSELEAVE:
        setInteraction(false, m_pressed);
        break;
    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
        setInteraction(false, false);
        break;
    case WM_NCRBUTTONUP:
        if (msg->wParam == HTCAPTION) {
            popupSystemMenu(
                QPoint(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)));
            *result = 0;
            return true;
        }
        break;
    case WM_SYSKEYDOWN:
        if (msg->wParam == VK_SPACE) {
            showSystemMenu();
            *result = 0;
            return true;
        }
        break;
    }
    return false;
}
} // namespace daymark
