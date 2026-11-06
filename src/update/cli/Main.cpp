#include "update/platform/windows/HostSettings.h"
#include <iostream>
#include <map>
#include <thread>
namespace {
daymark::update::Cancellation *activeCancel = nullptr;
BOOL WINAPI stop(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) {
        if (activeCancel)
            activeCancel->requested = true;
        return TRUE;
    }
    return FALSE;
}
void saveFailure(const daymark::update::fs::path &root, const daymark::update::Json &result) {
    using namespace daymark::update;
    try {
        win::logEvent(root,result);
        if (!fs::exists(root / "current.json"))
            return;
        win::FileLock lock(root / "update-state/locks/state", true);
        const auto file = root / "update-state/status.json";
        auto previous =
            fs::exists(file) ? parseJson(win::read(file, 131072), 131072) : Json::object();
        const auto owner = previous.value("owner_pid", DWORD(0));
        if (owner && owner != GetCurrentProcessId()) {
            win::Handle process(OpenProcess(SYNCHRONIZE, FALSE, owner));
            if (process && WaitForSingleObject(process.value, 0) == WAIT_TIMEOUT)
                return;
        }
        auto next = result;
        next["revision"] = previous.value("revision", std::uint64_t(0)) + 1;
        win::writeAtomic(file, next.dump());
        if (result.contains("retry_at_utc_seconds")) {
            const auto preferences = root / "update-state/preferences.json";
            auto value =
                fs::exists(preferences) ? parseJson(win::read(preferences)) : Json::object();
            value["not_before"] = result["retry_at_utc_seconds"];
            value["rate_limit_until"] = result["retry_at_utc_seconds"];
            win::writeAtomic(preferences, value.dump());
        }
    } catch (...) {
    }
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    using namespace daymark::update;
    Cancellation cancellation;
    activeCancel = &cancellation;
    SetConsoleCtrlHandler(stop, TRUE);
    fs::path root = win::installRoot(win::executable());
    std::string requestedCommand;
    bool explicitOld = false;
    try {
        const std::string command = argc > 1 ? win::utf8(argv[1]) : "help";
        requestedCommand = command;
        if (command == "help" || command == "--help" || command == "-h") {
            std::cout << "Daymark Windows updater\nCommands: capabilities, status, check, prepare, "
                         "cancel, cancel-ready, auto --enabled true|false, apply, recover, "
                         "launch\nOptions: --root <managed-directory>, --json, --data-dir "
                         "<profile-directory>\nPublisher tools: keygen <local-directory>, hash "
                         "<file>, sign <private-key.json> <manifest> [domain]\n";
            return 0;
        }
        if (command == "capabilities") {
            std::cout << Json{{"format", 1},
                              {"platform", "windows-x64"},
                              {"runtime", runtimeFamily()},
                              {"bootstrap_protocol", 1},
                              {"os_build", win::osBuild()},
                              {"qt_required", false},
                              {"trust_configured", !win::compiledKeys().empty()}}
                             .dump()
                      << '\n';
            return 0;
        }
        if (command == "hash" && argc == 3) {
            std::cout << win::fileHash(fs::path(argv[2])) << '\n';
            return 0;
        }
        if (command == "keygen" && argc == 3) {
            auto bytes = win::generatePrivate();
            auto key = win::publicKey(bytes, "daymark-" + win::randomHex(6));
            fs::path dir(argv[2]);
            if (fs::exists(dir / "private.json"))
                fail("update.key_already_exists", 31);
            win::writeAtomic(
                dir / "private.json",
                Json{{"key_id", key.id}, {"private_blob", encodeBase64(bytes)}}.dump());
            win::writeAtomic(dir / "public.json",
                             Json{{"key_id", key.id}, {"public_xy", hex(key.xy)}}.dump());
            win::writeAtomic(
                dir / "public-keys.json",
                Json{{"keys", Json::array({Json{{"key_id", key.id}, {"public_xy", hex(key.xy)}}})}}
                    .dump());
            std::cout << Json{{"result", "key_created"}, {"key_id", key.id}}.dump() << '\n';
            return 0;
        }
        if (command == "sign" && (argc == 4 || argc == 5)) {
            auto key = parseJson(win::read(argv[2]));
            auto raw = win::read(argv[3]);
            auto domain = argc == 5 ? win::utf8(argv[4]) : "Daymark.UpdateManifest.v1";
            if (domain != "Daymark.UpdateManifest.v1" && domain != "Daymark.BootstrapPayload.v1")
                fail("update.invalid_arguments", 2);
            win::writeAtomic(fs::path(argv[3]).concat(L".sig"),
                             win::sign(raw, decodeBase64(field(key, "private_blob")),
                                       field(key, "key_id"), domain));
            return 0;
        }
        std::map<std::string, std::wstring> options;
        for (int i = 2; i < argc; ++i) {
            auto key = win::utf8(argv[i]);
            if (key == "--json" || key == "--auto") {
                options[key] = L"1";
                continue;
            }
            if (key != "--root" && key != "--old-pid" && key != "--old-stamp" &&
                key != "--data-dir" && key != "--enabled"
#ifdef DAYMARK_UPDATE_FIXTURE
                && key != "--fixture-api" && key != "--fixture-trust"
#endif
            )
                fail("update.invalid_arguments", 2);
            if (++i >= argc)
                fail("update.invalid_arguments", 2);
            options[key] = argv[i];
        }
        if (options.contains("--root"))
            root = fs::path(options["--root"]);
        win::Settings settings = win::hostSettings(root);
#ifdef DAYMARK_UPDATE_FIXTURE
        if (options.contains("--fixture-api")) {
            settings.api = win::utf8(options["--fixture-api"]);
            settings.network.loopbackFixture = true;
            if (!settings.api.starts_with("http://127.0.0.1:"))
                fail("update.invalid_url");
        }
        if (options.contains("--fixture-trust")) {
            auto trust = parseJson(win::read(options["--fixture-trust"]));
            settings.keys = {{field(trust, "key_id"), unhex(field(trust, "public_xy"))}};
        }
#endif
        explicitOld = options.contains("--old-pid");
        win::Engine engine(settings, cancellation, [](const Json &event) {
            std::cout << event.dump() << '\n' << std::flush;
        });
        if (command == "initialize") {
            std::cout << engine.current().dump() << '\n';
            return 0;
        }
        if (command == "recover") {
            win::FileLock job(root / "update-state/locks/job", true);
            engine.recover();
            return 0;
        }
        if (command == "status") {
            std::cout << engine.status().dump() << '\n';
            return 0;
        }
        if (command == "cancel") {
            auto status = engine.status();
            win::writeAtomic(root / "update-state/cancel-request.json",
                             Json{{"owner_pid", status.value("owner_pid", std::uint64_t(0))},
                                  {"owner_stamp", status.value("owner_stamp", std::string())}}
                                 .dump());
            return 0;
        }
        if (command == "cancel-ready") {
            engine.cancelReady();
            return 0;
        }
        if (command == "auto") {
            if (!options.contains("--enabled") ||
                (options["--enabled"] != L"true" && options["--enabled"] != L"false"))
                fail("update.invalid_arguments", 2);
            engine.setAuto(options["--enabled"] == L"true");
            if (options["--enabled"] == L"false")
                win::writeAtomic(
                    root / "update-state/cancel-request.json",
                    Json{{"owner_pid", engine.status().value("owner_pid", std::uint64_t(0))},
                         {"owner_stamp", engine.status().value("owner_stamp", std::string())}}
                        .dump());
            return 0;
        }
        if (command == "launch") {
            std::vector<std::wstring> args;
            if (options.contains("--data-dir"))
                args = {L"--data-dir", options["--data-dir"]};
            engine.launch(args);
            return 0;
        }
        if (command != "check" && command != "prepare" && command != "apply")
            fail("update.invalid_arguments", 2);
        win::FileLock job(root / "update-state/locks/job", true);
        std::jthread monitor([&](std::stop_token token) {
            while (!token.stop_requested()) {
                try {
                    auto p = root / "update-state/cancel-request.json";
                    if (fs::exists(p)) {
                        auto j = parseJson(win::read(p));
                        if (j.value("owner_pid", std::uint64_t(0)) == GetCurrentProcessId() &&
                            j.value("owner_stamp", std::string()) ==
                                win::processStamp(GetCurrentProcess()))
                            cancellation.requested = true;
                    }
                } catch (...) {
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
        if (command == "check" || command == "prepare") {
#ifndef DAYMARK_UPDATE_FIXTURE
            if (!engine.shouldCheck(!options.contains("--auto"))) {
                std::cout << "{\"result\":\"deferred\"}\n";
                return 0;
            }
#endif
            if (options.contains("--auto") && !engine.shouldCheck(false)) {
                std::cout << "{\"result\":\"deferred\"}\n";
                return 0;
            }
            if (engine.status().value("ready", false)) {
                std::cout << engine.status().dump() << '\n';
                return 0;
            }
            auto release = engine.check(!options.contains("--auto"));
            if (release && command == "prepare")
                engine.prepare(*release);
            std::cout << Json{{"result", release ? (command == "prepare" ? "ready" : "available")
                                                 : "no_update"}}
                             .dump()
                      << '\n';
            return 0;
        }
        std::vector<std::wstring> args;
        if (options.contains("--data-dir"))
            args = {L"--data-dir", options["--data-dir"]};
        DWORD old = 0;
        if (options.contains("--old-pid")) {
            auto n = std::stoull(options["--old-pid"]);
            if (n > 0xffffffff)
                fail("update.invalid_arguments", 2);
            old = static_cast<DWORD>(n);
        }
        engine.apply(args, old,
                     options.contains("--old-stamp") ? win::utf8(options["--old-stamp"]) : "");
        std::cout << "{\"result\":\"applied\"}\n";
        return 0;
    } catch (const Failure &error) {
        if (requestedCommand == "apply" && !explicitOld && error.exitCode == 30 &&
            win::requestRestart(root)) {
            std::cout << "{\"result\":\"restart_requested\"}\n";
            return 0;
        }
        Json result{{"format", 1},
                    {"state", error.exitCode == 50 ? "cancelled" : "error"},
                    {"result", "error"},
                    {"error", error.key},
                    {"owner_pid", GetCurrentProcessId()}};
        if (error.retryAtUtcSeconds)
            result["retry_at_utc_seconds"] = error.retryAtUtcSeconds;
        if (error.platformError)
            result["platform_error"] = error.platformError;
        std::cout << result.dump() << '\n';
        saveFailure(root, result);
        return error.exitCode;
    } catch (const std::exception &) {
        Json result{{"format", 1},
                    {"state", "error"},
                    {"result", "error"},
                    {"error", "update.internal_error"},
                    {"owner_pid", GetCurrentProcessId()}};
        std::cout << result.dump() << '\n';
        saveFailure(root, result);
        return 41;
    }
}
