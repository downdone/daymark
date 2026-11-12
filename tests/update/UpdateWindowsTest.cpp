#include "update/platform/windows/Engine.h"
#include <future>
#include <iostream>
#include <thread>
using namespace daymark::update;
int main() {
    try {
        const auto root = fs::temp_directory_path() / ("daymark-update-io-" + win::randomHex());
        fs::create_directories(root);
        struct Cleanup {
            fs::path root;
            ~Cleanup() {
                std::error_code ignored;
                fs::remove_all(root, ignored);
            }
        } cleanup{root};
        {
            Cancellation cancellation;
            win::Engine engine(win::Settings{root, {}}, cancellation);
            const auto preferences=root/"update-state/preferences.json";
            const auto now=std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            win::writeAtomic(preferences,Json{{"last_attempt",now-3600}}.dump());
            if(engine.shouldCheck(false) || !engine.shouldCheck(true))return 21;
            win::writeAtomic(preferences,Json{{"last_attempt",now-86401}}.dump());
            if(!engine.shouldCheck(false))return 22;
            win::writeAtomic(preferences,Json{{"last_attempt",now},{"cancelled_until",now+86400}}.dump());
            if(!engine.shouldCheck(true))return 23;
            win::writeAtomic(preferences,Json{{"rate_limit_until",now+60}}.dump());
            if(engine.shouldCheck(true))return 24;
        }
        const auto lockPath = root / "update-state/locks/launch";
        {
            win::FileLock first(lockPath, true);
            try {
                win::FileLock second(lockPath, true, 40);
                return 11;
            } catch (const Failure &error) {
                if (error.key != "update.instance_busy" || error.exitCode != 30 ||
                    error.platformError != ERROR_LOCK_VIOLATION)
                    return 12;
            }
        }
        {
            std::promise<void> locked;
            auto ready = locked.get_future();
            std::jthread shortOwner([&] {
                win::FileLock first(lockPath, true);
                locked.set_value();
                Sleep(60);
            });
            ready.wait();
            win::FileLock afterRelease(lockPath, true, 1000);
        }
        {
            win::FileLock first(lockPath, false);
            win::FileLock shared(lockPath, false);
            try {
                win::FileLock exclusive(lockPath, true);
                return 13;
            } catch (const Failure &error) {
                if (error.platformError != ERROR_LOCK_VIOLATION)
                    return 14;
            }
        }
        if (!SetFileAttributesW(lockPath.c_str(), FILE_ATTRIBUTE_READONLY))
            return 15;
        bool readOnlyRejected = false;
        try {
            win::FileLock denied(lockPath, true);
        } catch (const Failure &error) {
            readOnlyRejected = error.key == "update.lock_access_denied" && error.exitCode == 31 &&
                               error.platformError == ERROR_ACCESS_DENIED;
        }
        SetFileAttributesW(lockPath.c_str(), FILE_ATTRIBUTE_NORMAL);
        if (!readOnlyRejected)
            return 16;
        {
            win::Handle other(CreateFileW(lockPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                          nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
            if (!other)
                return 17;
            try {
                win::FileLock denied(lockPath, true, 1000);
                return 18;
            } catch (const Failure &error) {
                if (error.key != "update.lock_file_in_use" || error.exitCode != 31 ||
                    error.platformError != ERROR_SHARING_VIOLATION)
                    return 19;
            }
        }
        const auto state = root / "state.json";
        win::writeAtomic(state, "old");
        std::promise<void> opened;
        auto ready = opened.get_future();
        std::jthread shortReader([&] {
            win::Handle file(CreateFileW(state.c_str(), GENERIC_READ,
                                         FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                         0, nullptr));
            opened.set_value();
            Sleep(60);
        });
        ready.wait();
        win::writeAtomic(state, "new");
        shortReader.join();
        if (win::read(state) != "new")
            return 6;
        win::Handle longReader(CreateFileW(state.c_str(), GENERIC_READ,
                                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                           OPEN_EXISTING, 0, nullptr));
        bool blocked = false;
        try {
            win::writeAtomic(state, "must-not-replace");
        } catch (const Failure &error) {
            blocked = error.key == "update.file_write" &&
                      (error.platformError == ERROR_SHARING_VIOLATION ||
                       error.platformError == ERROR_ACCESS_DENIED);
        }
        if (!blocked || win::read(state) != "new")
            return 7;
        longReader = win::Handle();
        if (std::distance(fs::directory_iterator(root), fs::directory_iterator{}) != 2)
            return 8;
        win::logEvent(root,{{"event","synthetic.failure"},{"platform_error",ERROR_ACCESS_DENIED}});
        const auto logs=root/"update-state/logs";
        const auto entry=*fs::directory_iterator(logs);
        const auto records=win::read(entry.path());
        const auto previousLine=records.find_last_of('\n',records.size()-2);
        const auto evidence=Json::parse(records.substr(previousLine==std::string::npos?0:previousLine+1));
        if(evidence["platform_error"]!=ERROR_ACCESS_DENIED || evidence["event"]!="synthetic.failure" || !evidence.contains("at_utc"))return 9;
        win::writeAtomic(logs/"keep.txt","user-owned");
        win::logEvent(root,{{"event","synthetic.complete"}});
        if(win::read(logs/"keep.txt")!="user-owned")return 10;
        auto secret = win::generatePrivate();
        auto key = win::publicKey(secret, "unit-test-only");
        std::string body = "{\"format\":1,\"payload\":\"synthetic\"}";
        auto signature = win::sign(body, secret, key.id, "Daymark.BootstrapPayload.v1");
        auto verified = win::verifySignature(body, signature, {key}, "Daymark.BootstrapPayload.v1");
        if (verified["payload"] != "synthetic")
            return 1;
        bool rejected = false;
        try {
            win::verifySignature(body + " ", signature, {key}, "Daymark.BootstrapPayload.v1");
        } catch (const Failure &) {
            rejected = true;
        }
        if (!rejected)
            return 2;
        rejected = false;
        try {
            win::verifySignature(body, signature, {key}, "Daymark.UpdateManifest.v1");
        } catch (const Failure &) {
            rejected = true;
        }
        if (!rejected)
            return 3;
        if (hex(win::sha256("abc")) !=
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
            return 4;
        std::cout
            << "PASS: actual Windows locks/timeout/release/read-only/sharing diagnostics, CNG and SHA256\n";
        return 0;
    } catch (const Failure &e) {
        std::cerr << e.key << " platform_error=" << e.platformError << '\n';
        return 5;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 5;
    }
}
