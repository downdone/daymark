#include "update/platform/windows/Setup.h"
#include <iostream>
#include <map>
int wmain(int argc, wchar_t **argv) {
  using namespace daymark::update;
  try {
    if (argc < 2)
      fail("update.invalid_arguments", 2);
    const auto command = win::utf8(argv[1]);
    std::map<std::string, std::wstring> options;
    for (int i = 2; i < argc; ++i) {
      const auto key = win::utf8(argv[i]);
      if (key != "--root" && key != "--source"
#ifdef DAYMARK_UPDATE_FIXTURE
          && key != "--fixture-trust" && key != "--fixture-failure"
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
