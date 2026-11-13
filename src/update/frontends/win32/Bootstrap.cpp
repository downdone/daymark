#include "update/platform/windows/Engine.h"
#include "update/platform/windows/HostSettings.h"
#include <shellapi.h>
#include <algorithm>
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    using namespace daymark::update;
    fs::path root;
    try {
        root = win::executable().parent_path();
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
        auto settings=win::hostSettings(root);
        if(fs::exists(root/"current.json") && win::nativeTool(settings,"daymark-update-cli.exe").parent_path()!=root &&
           std::find(arguments.begin(),arguments.end(),L"--version")==arguments.end() &&
           std::find(arguments.begin(),arguments.end(),L"--help")==arguments.end()){
            std::vector<std::wstring> forwarded{L"launch",L"--root",root.native()};
            forwarded.insert(forwarded.end(),arguments.begin(),arguments.end());
            if(const auto exit=win::forwardNative(settings,"daymark-update-cli.exe",forwarded))return static_cast<int>(*exit);
        }
        win::Engine engine(settings, cancel);
        engine.launch(arguments);
        return 0;
    } catch (const Failure &e) {
        if (!root.empty())
            win::logEvent(root, {{"event", "bootstrap.failed"}, {"error", e.key},
                                 {"platform_error", e.platformError}, {"exit_code", e.exitCode}});
        std::string advice;
        if (e.key == "update.instance_busy")
            advice = "启动或更新操作暂未结束，请稍候再试。若托盘已有 Daymark，请从托盘打开。";
        else if (e.key == "update.lock_access_denied" || e.key == "update.directory_read_only")
            advice = "无法写入应用目录。请将完整便携包解压到当前用户可写的本地文件夹，"
                     "再运行根目录的 daymark.exe。";
        else if (e.key == "update.lock_file_in_use")
            advice = "锁文件被其他程序占用。请等待解压或文件扫描结束后再试。";
        else
            advice = "启动未完成，请保留应用目录和数据，并反馈下方错误详情。";
        auto message = win::wide("Daymark 暂时无法启动。\n\n" + advice + "\n\n错误：" + e.key +
                                 (e.platformError ? "\nWindows 错误：" + std::to_string(e.platformError) : "") +
                                 "\n本地诊断：update-state\\logs（目录可写时）");
        MessageBoxW(nullptr, message.c_str(), L"Daymark", MB_OK | MB_ICONERROR);
        return e.exitCode;
    } catch (...) {
        if (!root.empty())
            win::logEvent(root, {{"event", "bootstrap.failed"}, {"error", "update.internal_error"}});
        MessageBoxW(nullptr, L"Daymark could not recover the application installation.", L"Daymark",
                    MB_OK | MB_ICONERROR);
        return 41;
    }
}
