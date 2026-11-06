#include "update/platform/windows/Engine.h"
#include "update/platform/windows/Ipc.h"
#include <iostream>
#include <thread>
int wmain(int argc, wchar_t **argv) {
    using namespace daymark::update;
    try {
        if (argc > 1 && std::wstring(argv[1]) == L"--version") {
            std::cout << "Daymark " << FIXTURE_VERSION << '\n';
            return 0;
        }
        if (argc > 1 && std::wstring(argv[1]) == L"--update-preflight")
            return 0;
        auto root = win::installRoot(win::executable());
        win::Handle pipe;
        Json context;
        for (int i = 1; i + 1 < argc; ++i)
            if (std::wstring(argv[i]) == L"--update-handoff") {
                win::Handle mapping(reinterpret_cast<HANDLE>(std::stoull(argv[++i])));
                const auto *data = static_cast<const char *>(
                    MapViewOfFile(mapping.value, FILE_MAP_READ, 0, 0, 16384));
                if (!data)
                    return 2;
                context = parseJson(std::string_view(data, std::char_traits<char>::length(data)));
                UnmapViewOfFile(data);
                auto name = win::wide(field(context, "pipe"));
                if (!WaitNamedPipeW(name.c_str(), 5000))
                    return 3;
                pipe =
                    win::Handle(CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                            OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr));
                if (!pipe)
                    return 4;
                ipc::send(pipe.value, Json{{"type", "HELLO"},
                                           {"nonce", context["nonce"]},
                                           {"version", FIXTURE_VERSION}}
                                          .dump());
                auto accept = parseJson(ipc::receive(pipe.value));
                if (accept["type"] != "ACCEPT")
                    return 5;
            }
        win::FileLock runtime(root / "update-state/locks/runtime", false);
        if (!context.empty()) {
            ipc::send(pipe.value, Json{{"type", "READY"}, {"nonce", context["nonce"]}}.dump());
            auto commit = parseJson(ipc::receive(pipe.value));
            auto current = parseJson(win::read(root / "current.json"));
            if (commit["type"] != "COMMIT" ||
                current["transaction_id"] != context["transaction_id"] ||
                current["generation"] != context["generation"])
                return 6;
            ipc::send(pipe.value, Json{{"type", "RUNNING"}, {"nonce", context["nonce"]}}.dump());
        }
        win::writeAtomic(root /
                             ("fixture-running-" + std::to_string(GetCurrentProcessId()) + ".json"),
                         Json{{"pid", GetCurrentProcessId()},
                              {"stamp", win::processStamp(GetCurrentProcess())},
                              {"version", FIXTURE_VERSION}}
                             .dump());
        for (unsigned i = 0; i < 600; ++i) {
            if (std::filesystem::exists(root /
                                        ("fixture-exit-" + std::to_string(GetCurrentProcessId()))))
                return 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return 0;
    } catch (...) {
        return 40;
    }
}
