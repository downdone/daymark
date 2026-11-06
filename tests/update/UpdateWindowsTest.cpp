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
        if (std::distance(fs::directory_iterator(root), fs::directory_iterator{}) != 1)
            return 8;
        win::logEvent(root,{{"event","synthetic.failure"},{"platform_error",ERROR_ACCESS_DENIED}});
        const auto logs=root/"update-state/logs";
        const auto entry=*fs::directory_iterator(logs);
        const auto evidence=Json::parse(win::read(entry.path()));
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
            << "PASS: actual Windows CNG keygen/sign/verify/tamper/domain and SHA256 vectors\n";
        return 0;
    } catch (const Failure &e) {
        std::cerr << e.key << " platform_error=" << e.platformError << '\n';
        return 5;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 5;
    }
}
