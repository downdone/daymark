#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <algorithm>
#include <windows.h>

namespace daymark::update::win {
// Shared by the native host and Qt entry without linking their different CRTs.
inline const char *lockErrorKey(DWORD error, bool opening) {
    if (error == ERROR_ACCESS_DENIED || error == ERROR_WRITE_PROTECT)
        return "update.lock_access_denied";
    if (opening && error == ERROR_SHARING_VIOLATION)
        return "update.lock_file_in_use";
    if (!opening && error == ERROR_LOCK_VIOLATION)
        return "update.instance_busy";
    return opening ? "update.lock_open_failed" : "update.lock_failed";
}

inline DWORD acquireByteLock(HANDLE file, bool exclusive, OVERLAPPED &overlapped,
                             DWORD timeoutMs = 0) {
    const auto deadline = GetTickCount64() + timeoutMs;
    for (;;) {
        if (LockFileEx(file, LOCKFILE_FAIL_IMMEDIATELY |
                                 (exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0),
                       0, 1, 0, &overlapped))
            return ERROR_SUCCESS;
        const auto error = GetLastError();
        const auto tick = GetTickCount64();
        if (error != ERROR_LOCK_VIOLATION || tick >= deadline)
            return error;
        Sleep(static_cast<DWORD>(std::min<ULONGLONG>(10, deadline - tick)));
    }
}
} // namespace daymark::update::win
