#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <windows.h>

namespace daymark::update::ipc {
// Header-only transport: each host uses its own CRT. No Qt/JSON/SDK ABI crosses processes.
inline void transfer(HANDLE pipe, void *buffer, DWORD length, bool writing, DWORD timeout = 30000) {
    auto *data = static_cast<char *>(buffer);
    DWORD offset = 0;
    const auto deadline = GetTickCount64() + timeout;
    while (offset < length) {
        OVERLAPPED overlapped{};
        overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!overlapped.hEvent)
            throw std::runtime_error("update.ipc_failed");
        DWORD done = 0;
        BOOL ok = writing ? WriteFile(pipe, data + offset, length - offset, &done, &overlapped)
                          : ReadFile(pipe, data + offset, length - offset, &done, &overlapped);
        if (!ok && GetLastError() == ERROR_IO_PENDING) {
            auto now = GetTickCount64();
            auto wait = now >= deadline ? WAIT_TIMEOUT
                                        : WaitForSingleObject(overlapped.hEvent,
                                                              static_cast<DWORD>(deadline - now));
            if (wait == WAIT_OBJECT_0)
                ok = GetOverlappedResult(pipe, &overlapped, &done, FALSE);
            else {
                CancelIoEx(pipe, &overlapped);
                GetOverlappedResult(pipe, &overlapped, &done, TRUE);
            }
        }
        CloseHandle(overlapped.hEvent);
        if (!ok || !done)
            throw std::runtime_error("update.ipc_failed");
        offset += done;
    }
}
inline void send(HANDLE pipe, std::string_view value, DWORD timeout = 30000) {
    if (value.empty() || value.size() > 65536)
        throw std::runtime_error("update.ipc_failed");
    auto length = static_cast<std::uint32_t>(value.size());
    transfer(pipe, &length, 4, true, timeout);
    transfer(pipe, const_cast<char *>(value.data()), length, true, timeout);
}
inline std::string receive(HANDLE pipe, DWORD timeout = 30000) {
    std::uint32_t length = 0;
    transfer(pipe, &length, 4, false, timeout);
    if (!length || length > 65536)
        throw std::runtime_error("update.ipc_failed");
    std::string value(length, 0);
    transfer(pipe, value.data(), length, false, timeout);
    return value;
}
} // namespace daymark::update::ipc
