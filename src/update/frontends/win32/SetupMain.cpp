#include "update/platform/windows/Setup.h"
#include <iostream>
#include <map>
#include <commctrl.h>
int wmain(int argc, wchar_t **argv) {
  using namespace daymark::update;
  try {
    if (argc < 2)
      fail("update.invalid_arguments", 2);
    const auto command = win::utf8(argv[1]);
    std::map<std::string, std::wstring> options;
    for (int i = 2; i < argc; ++i) {
      const auto key = win::utf8(argv[i]);
      if (key != "--root" && key != "--source" && key != "--parent-window"
#ifdef DAYMARK_UPDATE_FIXTURE
          && key != "--fixture-trust" && key != "--fixture-failure" && key != "--fixture-confirm"
#endif
      )
        fail("update.invalid_arguments", 2);
      if (options.contains(key) || ++i >= argc)
        fail("update.invalid_arguments", 2);
      options[key] = argv[i];
    }
    if (!options.contains("--root"))
      fail("update.invalid_arguments", 2);
    win::Settings settings{fs::path(options["--root"]), win::compiledKeys()};
#ifdef DAYMARK_UPDATE_FIXTURE
    if (options.contains("--fixture-trust")) {
      auto key = parseJson(win::read(options["--fixture-trust"]));
      settings.keys = {{field(key, "key_id"), unhex(field(key, "public_xy"))}};
    }
    if (options.contains("--fixture-failure")) {
      auto mode = options["--fixture-failure"];
      settings.checkpoint = [mode](std::string_view point) {
        if (point == "setup_old_moved") {
          if (mode == L"crash")
            ExitProcess(91);
          fail("setup.fixture_failure", 40);
        }
      };
    }
#endif
    win::SetupManager manager(settings);
    Json result;
    if (command == "check-idle" && !options.contains("--source"))
      result = manager.checkIdle(settings.root);
    else if (command == "confirm-close-running" && !options.contains("--source"))
      result = manager.closeRunning(settings.root, [&](unsigned) {
#ifdef DAYMARK_UPDATE_FIXTURE
        if (options.contains("--fixture-confirm"))
          return options["--fixture-confirm"] == L"yes";
#endif
        const TASKDIALOG_BUTTON buttons[] = {{101, L"强制关闭并继续"}, {IDCANCEL, L"取消安装"}};
        TASKDIALOGCONFIG dialog{};
        dialog.cbSize = sizeof(dialog);
        if(options.contains("--parent-window")){
          const auto handle=std::stoull(options["--parent-window"],nullptr,0);
          dialog.hwndParent=reinterpret_cast<HWND>(static_cast<std::uintptr_t>(handle));
          if(!IsWindow(dialog.hwndParent))fail("setup.confirmation_failed",30);
        }
        dialog.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
        dialog.pszWindowTitle = L"Daymark 安装";
        dialog.pszMainIcon = TD_WARNING_ICON;
        dialog.pszMainInstruction = L"Daymark 正在运行";
        dialog.pszContent = L"强制关闭可能丢失尚未保存的编辑。\n是否关闭此安装目录中的 Daymark，然后继续安装？";
        dialog.cButtons = 2;
        dialog.pButtons = buttons;
        dialog.nDefaultButton = IDCANCEL;
        int selected = IDCANCEL;
        if (FAILED(TaskDialogIndirect(&dialog, &selected, nullptr, nullptr)))
          fail("setup.confirmation_failed", 30);
        return selected == 101;
      });
    else if (command == "verify-bundle" && options.contains("--source"))
      result = manager.verifyBundle(options["--source"]);
    else if (command == "install" && options.contains("--source"))
      result = manager.install(options["--source"], settings.root);
    else if (command == "uninstall" && !options.contains("--source"))
      result = manager.remove(settings.root);
    else if (command == "finish-install" && !options.contains("--source"))
      result = manager.finish(settings.root);
    else if (command == "seal-uninstaller" && !options.contains("--source"))
      result = manager.sealUninstaller(settings.root);
    else
      fail("update.invalid_arguments", 2);
    std::cout << result.dump() << std::endl;
    return 0;
  } catch (const Failure &error) {
    std::cout << Json{{"format", 1},
                      {"result", "failed"},
                      {"error", error.key},
                      {"code", win::setupDiagnosticCode(error.key)},
                      {"platform_error", error.platformError},
                      {"exit_code", error.exitCode}}
                     .dump()
              << std::endl;
    return error.exitCode;
  } catch (const fs::filesystem_error &error) {
    std::cout << Json{{"format", 1},
                      {"result", "failed"},
                      {"error", "setup.filesystem"},
                      {"code", win::setupDiagnosticCode("setup.filesystem")},
                      {"platform_error", error.code().value()},
                      {"exit_code", 31}}
                     .dump()
              << std::endl;
    return 31;
  } catch (...) {
    std::cout << Json{{"format", 1},
                      {"result", "failed"},
                      {"error", "setup.internal"},
                      {"code", win::setupDiagnosticCode("setup.internal")},
                      {"exit_code", 40}}
                     .dump()
              << std::endl;
    return 40;
  }
}
