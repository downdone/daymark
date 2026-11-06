#pragma once
#include "Transport.h"
#include <memory>

namespace daymark::update::win {
struct Settings {
    fs::path root;
    std::vector<TrustKey> keys;
    NetworkPolicy network;
    std::string api = "https://api.github.com/repos/downdone/daymark";
    std::function<void(std::string_view)> checkpoint;
    bool installationOperation = false;
};
class FileLock {
    Handle handle;
    OVERLAPPED overlapped{};

  public:
    FileLock(const fs::path &, bool exclusive, DWORD timeoutMs = 0);
    FileLock(const FileLock &) = delete;
    FileLock &operator=(const FileLock &) = delete;
    ~FileLock();
    void release();
};
struct Child {
    Handle process;
    DWORD pid = 0;
};
Child spawn(const fs::path &, const std::vector<std::wstring> &, bool hidden = true);
std::string processStamp(HANDLE);
Json verifySignature(std::string_view raw, std::string_view signature,
                     const std::vector<TrustKey> &, std::string_view domain);
class Engine {
    Settings settings;
    std::unique_ptr<FileLock> installationLease;
    Cancellation &cancel;
    Notify notify;
    std::string loggedState;
    void publish(std::string_view state, Json extra = {});
    Release trusted(std::string raw, std::string signature) const;
    Json descriptor(std::string_view relative, bool full, bool verify = true) const;
    void stage(const Release &, const fs::path &);
    void cleanOld(const Json &journal);

  public:
    Engine(Settings, Cancellation &, Notify = {});
    std::vector<FileEntry> ownedFiles() const;
    Version installedVersion() const;
    Json current();
    Json status() const;
    std::optional<Release> check(bool manual = false);
    void prepare(const Release &);
    void apply(const std::vector<std::wstring> &applicationArgs = {}, DWORD oldPid = 0,
               std::string oldStamp = {});
    void launch(const std::vector<std::wstring> &applicationArgs = {});
    void recover();
    void cancelReady();
    void setAuto(bool enabled);
    bool shouldCheck(bool manual) const;
    const Settings &configuration() const { return settings; }
};
std::vector<TrustKey> compiledKeys();
fs::path installRoot(const fs::path &executablePath);
bool requestRestart(const fs::path &root);
} // namespace daymark::update::win
