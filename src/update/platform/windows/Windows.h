#pragma once
#include "update/sdk/Update.h"
#include <windows.h>

namespace daymark::update::win {
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    Handle() = default;
    explicit Handle(HANDLE h) : value(h) {}
    ~Handle() {
        if (value && value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
    }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    Handle(Handle &&other) noexcept : value(std::exchange(other.value, INVALID_HANDLE_VALUE)) {}
    Handle &operator=(Handle &&other) noexcept {
        if (this != &other) {
            if (value && value != INVALID_HANDLE_VALUE)
                CloseHandle(value);
            value = std::exchange(other.value, INVALID_HANDLE_VALUE);
        }
        return *this;
    }
    explicit operator bool() const { return value && value != INVALID_HANDLE_VALUE; }
};
std::wstring wide(std::string_view);
std::string utf8(std::wstring_view);
fs::path executable();
std::string read(const fs::path &, std::size_t limit = 65536);
void writeAtomic(const fs::path &, std::string_view);
void logEvent(const fs::path& root,Json event) noexcept;
void noLinks(const fs::path &);
fs::path inside(const fs::path &root, std::string_view relative);
Bytes sha256(std::string_view);
std::string fileHash(const fs::path &);
std::string randomHex(std::size_t bytes = 16);
Platform crypto();
Bytes generatePrivate();
TrustKey publicKey(const Bytes &privateKey, std::string id);
std::string sign(std::string_view raw, const Bytes &privateKey, std::string_view keyId,
                 std::string_view domain = "Daymark.UpdateManifest.v1");
std::uint32_t osBuild();
} // namespace daymark::update::win
