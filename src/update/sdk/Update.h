#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace daymark::update {
namespace fs = std::filesystem;
using Json = nlohmann::json;
using Bytes = std::vector<unsigned char>;
struct Failure final : std::runtime_error {
    std::string key;
    int exitCode;
    std::int64_t retryAtUtcSeconds = 0;
    std::uint32_t platformError = 0;
    explicit Failure(std::string key, int code = 20);
};
[[noreturn]] void fail(std::string_view key, int code = 20);
struct Version {
    std::uint32_t major = 0, minor = 0, patch = 0;
    auto operator<=>(const Version &) const = default;
    std::string text() const;
    static Version parse(std::string_view);
};
Json parseJson(std::string_view, std::size_t limit = 65536);
std::string field(const Json &, std::string_view key, std::size_t maxLength = 4096);
std::uint64_t number(const Json &, std::string_view key, std::uint64_t maximum);
bool safeRelative(std::string_view);
std::string versionKey(const Json &manifest);
std::string runtimeFamily();

struct TrustKey {
    std::string id;
    Bytes xy;
};
struct Platform {
    std::function<Bytes(std::string_view)> hash;
    std::function<bool(const TrustKey &, const Bytes &, const Bytes &)> verify;
};
struct Release {
    Json manifest;
    std::string raw, signature;
    Version version;
};
Release verifyRelease(std::string raw, std::string signature, const std::vector<TrustKey> &,
                      const Platform &);
void checkCompatibility(const Release &, const Version &installed, std::string_view runtime,
                        std::uint32_t osBuild, bool requireNewer = true);
Bytes decodeBase64(std::string_view);
std::string encodeBase64(const Bytes &);
std::string hex(const Bytes &);
Bytes unhex(std::string_view);

struct FileEntry {
    std::string path, sha256;
    std::uint64_t size;
};
std::vector<FileEntry> fileIndex(std::string_view);
using Notify = std::function<void(const Json &)>;
struct Cancellation {
    std::atomic_bool requested{false};
    void check() const {
        if (requested.load())
            fail("update.cancelled", 50);
    }
};
inline constexpr std::uint64_t MaxPackage = 512ULL * 1024 * 1024;
inline constexpr std::uint64_t MaxUnpacked = 2ULL * 1024 * 1024 * 1024;
inline constexpr std::string_view Repository = "downdone/daymark";
inline constexpr std::string_view DataAbi = "task-store-v11-rw1";
inline constexpr unsigned Schema = 11;
} // namespace daymark::update
