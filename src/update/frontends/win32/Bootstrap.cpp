#include "update/platform/windows/Engine.h"
#include "update/platform/windows/HostSettings.h"
#include <shellapi.h>
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    using namespace daymark::update;
    try {
        int count = 0;
        auto **values = CommandLineToArgvW(GetCommandLineW(), &count);
        std::vector<std::wstring> arguments;
        for (int i = 1; i < count; ++i) {
            std::wstring value = values[i];
            if (value == L"--data-dir" && i + 1 < count) {
                arguments.push_back(value);
                arguments.push_back(values[++i]);
            } else if (value == L"--help" || value == L"--version")
                arguments.push_back(value);
            else {
                LocalFree(values);
                fail("update.invalid_arguments", 2);
            }
        }
        LocalFree(values);
        Cancellation cancel;
        win::Engine engine(win::hostSettings(win::executable().parent_path()), cancel);
        engine.launch(arguments);
        return 0;
    } catch (const Failure &e) {
        auto message = win::wide(
            "Daymark could not start safely.\n" + e.key +
            "\nPlease keep your data and reinstall the portable application if necessary.");
        MessageBoxW(nullptr, message.c_str(), L"Daymark", MB_OK | MB_ICONERROR);
        return e.exitCode;
    } catch (...) {
        MessageBoxW(nullptr, L"Daymark could not recover the application installation.", L"Daymark",
                    MB_OK | MB_ICONERROR);
        return 41;
    }
}
