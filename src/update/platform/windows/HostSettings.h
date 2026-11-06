#pragma once
#include "Engine.h"
namespace daymark::update::win {
inline Settings hostSettings(const fs::path &root) {
    Settings settings{root, compiledKeys()};
#ifdef DAYMARK_UPDATE_FIXTURE
    // This code is absent from every production target. Only loopback fixtures
    // may inject a temporary trust key or API endpoint.
    auto path = root / "update-fixture.json";
    if (fs::exists(path)) {
        auto fixture = parseJson(read(path));
        settings.api = field(fixture, "api");
        if (!settings.api.starts_with("http://127.0.0.1:"))
            fail("update.invalid_url");
        settings.network.loopbackFixture = true;
        settings.keys = {{field(fixture, "key_id"), unhex(field(fixture, "public_xy"))}};
        const auto crash = fixture.value("crash_at", std::string());
        if (!crash.empty())
            settings.checkpoint = [crash, root](std::string_view point) {
                if (point == crash) {
                    writeAtomic(
                        root / "update-state/fixture-checkpoint.json",
                        Json{{"point", std::string(point)}, {"worker_pid", GetCurrentProcessId()}}
                            .dump());
                    ExitProcess(91);
                }
                if (crash == "throw_" + std::string(point))
                    fail("update.fixture_failure", 40);
                if(crash=="cancel_"+std::string(point))
                    fail("update.cancelled",50);
            };
    }
#endif
    return settings;
}
} // namespace daymark::update::win
