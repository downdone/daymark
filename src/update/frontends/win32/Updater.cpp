#include "update/platform/windows/Engine.h"
#include "update/platform/windows/HostSettings.h"
#include <commctrl.h>
#include <memory>

namespace {
using namespace daymark::update;
win::Settings config;
Cancellation cancel;
std::unique_ptr<win::Engine> engine;
HWND label = nullptr, progress = nullptr, primary = nullptr, secondary = nullptr;
HFONT uiFont = nullptr;
UINT currentDpi = 96;
int dip(int value) { return MulDiv(value, static_cast<int>(currentDpi), 96); }
void layout(HWND window, UINT dpi) {
    currentDpi = dpi;
    LOGFONTW font{};
    font.lfHeight = -dip(13);
    wcscpy_s(font.lfFaceName, L"Segoe UI");
    auto replacement = CreateFontIndirectW(&font);
    for (HWND child : {label, primary, secondary})
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(replacement), TRUE);
    if (uiFont)
        DeleteObject(uiFont);
    uiFont = replacement;
    MoveWindow(label, dip(20), dip(22), dip(410), dip(48), TRUE);
    MoveWindow(progress, dip(20), dip(80), dip(410), dip(20), TRUE);
    MoveWindow(primary, dip(190), dip(128), dip(115), dip(32), TRUE);
    MoveWindow(secondary, dip(315), dip(128), dip(115), dip(32), TRUE);
    InvalidateRect(window, nullptr, TRUE);
}
bool isReady = false, isBusy = false;
std::wstring message(const Json &state) {
    if (state.value("ready", false))
        return L"更新已安装，重启后生效。";
    auto key = state.value("state", std::string());
    if (key == "checking")
        return L"正在检查更新…";
    if (key == "downloading")
        return L"正在下载更新…";
    if (key == "verifying")
        return L"正在验证更新…";
    if (key == "staging")
        return L"正在安装新版本…";
    if (key == "starting" || key == "waiting_for_exit")
        return L"正在完成更新…";
    if (key == "up_to_date")
        return L"已是最新可安装版本。";
    if (key == "running")
        return L"新版已正常运行。";
    if (key == "cancelled")
        return L"已取消更新准备。";
    if (state.contains("error"))
        return L"更新未完成，可重试或从关于页查看原因。";
    return L"Daymark 每天自动检查并准备更新。";
}
void command(std::wstring_view value) {
    win::spawn(config.root / L"daymark-update-cli.exe",
               {std::wstring(value), L"--root", config.root.native(), L"--json"});
}
LRESULT CALLBACK windowProcedure(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        auto font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        label = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 20, 22, 410, 48, window,
                              nullptr, nullptr, nullptr);
        progress = CreateWindowExW(0, PROGRESS_CLASSW, L"", WS_CHILD | WS_VISIBLE, 20, 80, 410, 20,
                                   window, nullptr, nullptr, nullptr);
        SendMessageW(progress, PBM_SETRANGE32, 0, 1000);
        primary = CreateWindowW(L"BUTTON", L"检查更新",
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 190, 128,
                                115, 32, window, reinterpret_cast<HMENU>(1), nullptr, nullptr);
        secondary = CreateWindowW(L"BUTTON", L"稍后", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 315, 128,
                                  115, 32, window, reinterpret_cast<HMENU>(2), nullptr, nullptr);
        for (HWND child : {label, primary, secondary})
            SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        layout(window, GetDpiForWindow(window));
        SetTimer(window, 1, 250, nullptr);
        return 0;
    }
    case WM_TIMER:
        try {
            auto s = engine->status();
            isReady = s.value("ready", false);
            auto phase = s.value("state", std::string());
            isBusy = phase == "checking" || phase == "downloading" || phase == "verifying" ||
                     phase == "staging" || phase == "waiting_for_exit" || phase == "starting";
            SetWindowTextW(label, message(s).c_str());
            SetWindowTextW(primary, isReady ? L"立即重启" : L"检查更新");
            SetWindowTextW(secondary, isBusy ? L"取消准备" : L"稍后");
            EnableWindow(primary, !isBusy);
            auto total = s.value("total", std::uint64_t(0));
            auto done = s.value("received", std::uint64_t(0));
            SendMessageW(progress, PBM_SETPOS,
                         total ? static_cast<WPARAM>(1000.0 * double(done) / double(total)) : 0, 0);
        } catch (...) {
            SetWindowTextW(label, L"无法读取更新状态，请检查安装目录权限。");
        }
        return 0;
    case WM_COMMAND:
        try {
            if (LOWORD(wp) == 1)
                command(isReady ? L"apply" : L"prepare");
            else if (LOWORD(wp) == 2) {
                if (isBusy)
                    command(L"cancel");
                else
                    DestroyWindow(window);
            }
        } catch (...) {
            MessageBoxW(window, L"无法启动更新操作。", L"Daymark 更新", MB_OK | MB_ICONERROR);
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DPICHANGED: {
        auto *rectangle = reinterpret_cast<RECT *>(lp);
        SetWindowPos(window, nullptr, rectangle->left, rectangle->top,
                     rectangle->right - rectangle->left, rectangle->bottom - rectangle->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        layout(window, HIWORD(wp));
        return 0;
    }
    case WM_DESTROY:
        KillTimer(window, 1);
        if (uiFont) {
            DeleteObject(uiFont);
            uiFont = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, msg, wp, lp);
}
} // namespace
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    try {
        config = win::hostSettings(win::installRoot(win::executable()));
        if(const auto exit=win::forwardNative(config,"daymark-updater.exe",{}))return static_cast<int>(*exit);
        engine = std::make_unique<win::Engine>(config, cancel);
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_PROGRESS_CLASS};
        InitCommonControlsEx(&controls);
        WNDCLASSW type{};
        type.hInstance = instance;
        type.lpszClassName = L"DaymarkUpdater";
        type.lpfnWndProc = windowProcedure;
        type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        type.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        type.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
        RegisterClassW(&type);
        currentDpi = GetDpiForSystem();
        auto window =
            CreateWindowExW(WS_EX_CONTROLPARENT, type.lpszClassName, L"Daymark 更新",
                            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT,
                            CW_USEDEFAULT, dip(470), dip(220), nullptr, nullptr, instance, nullptr);
        if (!window)
            return 41;
        ShowWindow(window, show);
        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            if (!IsDialogMessageW(window, &msg)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
        return static_cast<int>(msg.wParam);
    } catch (...) {
        MessageBoxW(nullptr, L"无法打开更新器，请检查安装目录。", L"Daymark 更新",
                    MB_OK | MB_ICONERROR);
        return 41;
    }
}
